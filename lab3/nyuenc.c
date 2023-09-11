#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <math.h>
//SOURCES:
//https://www.youtube.com/watch?v=_n2hE2gyPxU
//https://nachtimwald.com/2019/04/12/thread-pool-in-c/

int finished=0;
char* taskQ[1000];
int lengthQ[1000];
char** encoded;
int* encodedLEN;
int taskCount=0;
int done=0;
pthread_mutex_t lockQ=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond=PTHREAD_COND_INITIALIZER;
int order=0;
int workingTHREADS=0;

void performEncode(char *args,int size,int threadNUM){ 
    //pass -1 if not using threads
    //Pass >=0 if using threads
    
    int let = args[0];
    int rcount=0;
    char* ret=malloc(0);
    int sizeret=2;
    unsigned int count=0;
    int LEN=0;
    for(int i = 0; i < size;i++){
        if(args[i]!=let){
            ret=(char*)realloc(ret,sizeret);
            sizeret+=2;
            ret[rcount]=let;
            rcount++;
            ret[rcount]=count;
            
            let = args[i];
            count = 1;
            LEN+=2;
            if(threadNUM!=-1){
                pthread_mutex_lock(&lockQ);
                encoded[threadNUM]=realloc(encoded[threadNUM],LEN*sizeof(char));
                encoded[threadNUM][rcount-1]=ret[rcount-1];
                encoded[threadNUM][rcount]=ret[rcount];  
                pthread_mutex_unlock(&lockQ);
                
            }
            rcount++;
        }
        else count++;
    }
    ret=(char*)realloc(ret,sizeret);
    ret[rcount]=let;
    ret[rcount+1]=count;
    LEN+=2;
    if(threadNUM!=-1){
        pthread_mutex_lock(&lockQ);
        encoded[threadNUM]=realloc(encoded[threadNUM],LEN*sizeof(char));
        encoded[threadNUM][rcount]=ret[rcount];
        encoded[threadNUM][rcount+1]=ret[rcount+1];
        encodedLEN[threadNUM]=LEN;
        
        pthread_mutex_unlock(&lockQ);
    }
    
    if(threadNUM==-1){
        write(1,ret,LEN);
    }
    
}
void addTask(char* task,int len){
    pthread_mutex_lock(&lockQ);
    taskQ[taskCount]=malloc(len*sizeof(char));
    for(int i=0;i<len;i++)taskQ[taskCount][i]=task[i];
    lengthQ[taskCount]=len;
    taskCount++; 
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lockQ);
}


void* startT(void* arg){
    //figure out way to end this loop!!
    
    while(1){
        char* words;
        int l;
        pthread_mutex_lock(&lockQ);
 
        while(taskCount==0){
            if(done==1){
                pthread_mutex_unlock(&lockQ);
			    pthread_exit(NULL);
            }
            workingTHREADS++;
            pthread_cond_wait(&cond,&lockQ);
            workingTHREADS--;
            if(done==1){
                pthread_mutex_unlock(&lockQ);
			    pthread_exit(NULL);
            }
        }
        l=lengthQ[0];
        words=malloc(l*sizeof(char));
        words=taskQ[0];
        order=order+1;
        for(int i = 0; i < taskCount-1;i++){
            taskQ[i]=malloc(lengthQ[i+1]);
            taskQ[i]=taskQ[i+1];
            lengthQ[i]=lengthQ[i+1];
        }
        taskCount--;
        pthread_mutex_unlock(&lockQ);
        performEncode(words,l,order-1);
    }
    pthread_exit(NULL);
    return arg;
}


int main(int argc, char* argv[]){
    //Encoded String!
    
    int opt;
    int num_thread=0;
    char* fullString=(char*)malloc(0);
    struct stat size;
    int startFull=0;
    int sizeFull=0;
    int passedJ=-1;
    while ((opt = getopt(argc, argv, "j:")) != -1) {
        switch(opt){
            case 'j':
                num_thread = atoi(optarg);
                break;
            default:
                break;
        }
    }
    for(int x = 1; x < argc; x++){
        //makes sure we can access files
        if(num_thread==0)passedJ=0;
        if(access(argv[x],F_OK|R_OK)!=-1&&passedJ==0){
            //access file!
            
            int file = open(argv[x], O_RDONLY);
            if(file==-1)return 0;
            
            if(fstat(file,&size)==-1)return 0;
            char *ad = mmap(NULL,size.st_size,PROT_READ,MAP_PRIVATE,file,0);
            if(ad==MAP_FAILED)return 0;

            sizeFull+=(int)size.st_size*sizeof(char);
            fullString=(char*)realloc(fullString,sizeFull);
            for(int i = 0; i< (int)size.st_size;i++){
                fullString[startFull]=ad[i];        
                startFull++;
            }
            munmap(ad,size.st_size);
            close(file);
        }
        if(strcmp(argv[x],"-j")==0){
            passedJ=0;
            x++;
        }
    }
    int itLEN=0;
    if(num_thread>0){
        itLEN=floor(sizeFull/4096);
        if(sizeFull%4096!=0)itLEN+=1;
        encoded=calloc(itLEN*sizeof(char*),sizeof(char*));
        encodedLEN=calloc(itLEN*sizeof(int),sizeof(int));
    }
    pthread_t threads[num_thread];
    if(num_thread>0){
        for(int i = 0; i < num_thread;i++){
            if(pthread_create(&threads[i],NULL,&startT,NULL)!=0)perror("Thread Failed to Create");
        }
        if(sizeFull<4096)addTask(fullString,sizeFull);
        else{
            int it=0;
            char* task=malloc(0);
            
            for(int i = 0; i < sizeFull;i++){
                task=realloc(task,it+1);
                task[it]=fullString[i];
                it++;
                
                if(it==4096){
                    addTask(task,4096);
                    task=malloc(0);
                    it=0;
                }
                
            }
            if(it>0){
                addTask(task,it);
            }
            free(task);
        }
        free(fullString);
    }
    if(num_thread==0){
        performEncode(fullString,sizeFull,-1);
    }
    
    
    if(num_thread>0){

        while(workingTHREADS<num_thread);

        pthread_mutex_lock(&lockQ);
        done=1;
        for(int i = 0; i < num_thread;i++)pthread_cond_signal(&cond);
        pthread_mutex_unlock(&lockQ);
        
        int i;
        for(i=0;i<itLEN;i++){
            if(i!=itLEN-1){
                if(encodedLEN[i]>0){
                    if(encoded[i][encodedLEN[i]-2]==encoded[i+1][0]){
                        encoded[i+1][1]+=encoded[i][encodedLEN[i]-1];
                        encoded[i][encodedLEN[i]-1]='\0';
                        encoded[i][encodedLEN[i]-2]='\0';
                        encoded[i]=realloc(encoded[i],encodedLEN[i]-2);
                        write(1,encoded[i],encodedLEN[i]-2);
                    }
                    else write(1,encoded[i],encodedLEN[i]);
                }
                else write(1,encoded[i],encodedLEN[i]);
            }
            else{
                write(1,encoded[i],encodedLEN[i]);
            }
            free(encoded[i]);
        }

        //Exits all threads before joining them
        for(int i = 0; i < num_thread;i++){
            if(pthread_join(threads[i],NULL)!=0)perror("Thread Failed to Join");
        }   
        
    }
    pthread_mutex_destroy(&lockQ);
    pthread_cond_destroy(&cond);
    
    free(encodedLEN);
    free(encoded);
    return 0;
}
