#include<stdio.h>
#include<string.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdbool.h>

#define MAX_IN_POOL 128

static int histogram[256] = {0, };
static bool flag[128] = {false, };
int pool_size=0, tid=-1,f_count,num=0;
pthread_t* threads;
double* threads_time;
double* priority_wating_time;
int* priority_size;
sem_t sem, full, h_buffer, create, end, h_sem;	
void* worker(void* arg);

typedef struct
{
    char f_name[20];
    int priority;
    struct timespec data_start, data_end;
}data;

typedef struct
{
    data* root;
    int heap_size;
}min_heap;

min_heap heap;

void insert_min_heap(min_heap *h, data item);
data delete_min_heap(min_heap *h);

int main(int argc, char* argv[])
{
	struct timespec total_start, total_end;
	clock_gettime(CLOCK_REALTIME, &total_start);
	int i,j,status,k,buffer_size;
    char* batch;
    // ./hw3 batch.txt 5 5

    if (argc == 2)
    {
        batch = argv[1];    pool_size = 1;  buffer_size = 1;
    }
    else if (argc == 3)
    {
        batch = argv[1];    pool_size = atoi(argv[2]);  buffer_size = 1;
    }
    else if (argc == 4)
    {
        batch = argv[1];    pool_size = atoi(argv[2]);  buffer_size = atoi(argv[3]);
    }
    else
    {
        printf("인자 갯수를 확인해주세요!\n");
        return 0;
    }

    heap.root = (data*)malloc(sizeof(data)*(buffer_size + 1));
    heap.heap_size = 0;
    //최소힙 생성 완료.
    int* buffer = (int*)malloc(sizeof(int)*buffer_size);
    //버퍼생성 완료

    //////////////////////////////////////waiting time check///////////////////////////////////////////////

    priority_wating_time = (double*)malloc(sizeof(double) * 5);//각 우선순위를 가지는 worker가 실행한 시간
    priority_size = (int*)malloc(sizeof(int) * 5); //각 우선순위 갯수 카운트

    //////////////////////////////////////waiting time check///////////////////////////////////////////////
    FILE *fp = fopen(batch, "rb");
    fscanf(fp, "%d", &f_count);
	int fp2 = open("histogram.bin", O_RDWR);
	if(fp2 == '\0')
	{
	    puts("파일오픈 실패");
	    return;
	}
	threads_time = (double*)malloc(sizeof(double)*pool_size);

	sem_init(&full,0,0);
	sem_init(&sem,0,1);
    sem_init(&h_buffer, 0, buffer_size);
	sem_init(&create,0,1);
	sem_init(&end,0,0);
    sem_init(&h_sem, 0, 1);

	threads = (pthread_t*)malloc(sizeof(pthread_t)*pool_size);
	for(i=1;i<=pool_size;i++)
	{
		if(pthread_create(&(threads[i]),NULL,&worker,NULL))
		{
			fprintf(stderr,"Thread initiation error!\n");
			return 0;
		}
	}
//스레드풀 생성 완료
    char name[20];
    int priority;
    data f_data;
	for(i=1;i<=f_count;i++)
	{
        fscanf(fp, "%s %d", name, &priority);
        strcpy(f_data.f_name, name);     f_data.priority = priority;

        sem_wait(&h_buffer);
        sem_wait(&h_sem);

        clock_gettime(CLOCK_REALTIME, &f_data.data_start);
        insert_min_heap(&heap, f_data);       //최소힙 삽입

        sem_post(&h_sem);
		sem_post(&full);
	}

	sem_wait(&end);
//생산자부분
	int n = write(fp2,(int*)histogram,sizeof(histogram));
//완성@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
    sem_init(&sem, 0, 1);
    data t_data;
    t_data.priority = -1;
    for (i = 0; i < pool_size; i++)
    {
        sem_wait(&h_buffer);
        sem_wait(&h_sem);

        insert_min_heap(&heap, t_data);       //최소힙 삽입

        sem_post(&h_sem);
        sem_post(&full);
    }
///////////////////////////////////////////////////////////////////
    for (i = 0; i < pool_size; i++)
        pthread_join(threads[i], (void*)&status);
///////////////////////////////////////////////////////////////////
	close(fp2);
		int min=0,temp=0;	
	for(i=0;i<pool_size-1;i++)
	{
		min=i;
		for(j=i+1;j<pool_size;j++)
		{
			if(threads_time[min]>threads_time[j])
				min=j;
		}
		temp=threads_time[i];
		threads_time[i]=threads_time[min];
		threads_time[min]=temp;
	}
//정렬
	double sum=0;
	for(i=0;i<pool_size;i++)
		sum+=threads_time[i];
	printf("MAX TIME : %lf\n",threads_time[pool_size-1]);
	printf("MIN TIME : %lf\n",threads_time[0]);
	printf("AVG TIME : %lf\n",sum/pool_size);
    clock_gettime(CLOCK_REALTIME, &total_end);
    double total_time = (double)(total_end.tv_sec - total_start.tv_sec) * 1000 + (double)(total_end.tv_nsec - total_start.tv_nsec) / 1000000;
    printf("total_time : %f\n\n", total_time);

    ////////////////////////////////////waiting time 출력////////////////////////////////////
    for (i = 0; i < 5; i++)
    {
        priority_wating_time[i] /= priority_size[i];
        printf("\"%d\" priority_wating_time : %lf\n", i,priority_wating_time[i]);
    }
    ////////////////////////////////////출력종료 프로그램////////////////////////////////////
	return 0;
}				

void* worker(void* arg)//소비자부분
{	
    double exe_time = 0.0;
	struct timespec starttime, endtime;
	int id=0;
	sem_wait(&create);
	id=++tid; //스레드당 ID할당

	sem_post(&create);
	int i,j,k,t,count = 0,priority;
    double data_time = 0;
	char filename[20] = { 0, };
	unsigned char str[20];
    data h_data;    
	while(1)
	{	
		sem_wait(&full);
        //////////////////////////////////////////////////
        sem_wait(&h_sem);

        h_data = delete_min_heap(&heap);
        priority = h_data.priority;
        strcpy(filename, h_data.f_name);

        clock_gettime(CLOCK_REALTIME, &starttime);
        clock_gettime(CLOCK_REALTIME, &h_data.data_end);

        sem_post(&h_sem);
        if (priority == -1)
            break;
        data_time = (double)((h_data.data_end).tv_sec - (h_data.data_start).tv_sec) * 1000 + (double)((h_data.data_end).tv_nsec - (h_data.data_start).tv_nsec) / 1000000;
	printf("@@@@@@@@@@@@@%s    %lf\n",filename,data_time);
        priority_size[h_data.priority]++;//우선순위에 따라 카운트 증가
        priority_wating_time[h_data.priority] += data_time;

        //////////////////////////////////////////////////


		int hissum[256] = {0, };

		FILE * fp1 = fopen(filename, "rb"); 
		if (fp1 == NULL) 
		{
			printf("%s\n",filename);
			puts("파일오픈 실패");
		}

		while (1)
		{
		    count = fread((void*)str, sizeof(char), (sizeof(str) / sizeof(char)), fp1); 
		    for (t = 0; t < count; t++)
			hissum[str[t]]++;
		    if (count < (sizeof(str) / sizeof(char)))
		    {
			if (feof(fp1) != 0) 
			    break;
			else
			    puts("실패");
			break;
		    }
		}
		fclose(fp1);
	//한프로세스에 할당된 파일 카운팅 완료
	//세마포어 wait
		sem_wait(&sem);
//---------------------------------------C.S---------------------------------------------
		for(j=0;j<256;j++)
			histogram[j] = histogram[j] + hissum[j];
        num++; //작업이 끝날 데이터파일 갯수 체크
//---------------------------------------C.S---------------------------------------------			
		sem_post(&sem);
	//세마포어 post
		if(num==f_count)
			sem_post(&end);
        sem_post(&h_buffer);
        clock_gettime(CLOCK_REALTIME, &endtime);
        exe_time = (double)(endtime.tv_sec - starttime.tv_sec) * 1000 + (double)(endtime.tv_nsec - starttime.tv_nsec) / 1000000;
        printf("ID : %d             %lf            \n", id, exe_time);
        threads_time[id]+= exe_time;
	}
    sem_post(&h_buffer);
}

void insert_min_heap(min_heap *h, data item)
{
    int i;
    i = ++(h->heap_size);
    //히프크기가하나증가. 

    //트리를거슬러올라가면서부모노드와비교하는과정 
    while ((i != 1) && (item.priority <= h->root[i / 2].priority))
    {
        h->root[i] = h->root[i / 2];
        i /= 2;
    }

    h->root[i] = item;    //새로운노드를삽입 
}

data delete_min_heap(min_heap *h)
{
    int parent, child;
    data item, temp;
    item = h->root[1];    //삭제할루트노드 
    temp = h->root[(h->heap_size)--];    //말단노드 

    parent = 1;
    child = 2;    //child는아래로아래로그리고parent는child를따라가고 

    while (child <= h->heap_size)
    {
        //현재노드의자식노드중더작은자식노드를찾는다. 
        if ((child < h->heap_size) && (h->root[child].priority) > (h->root[child + 1].priority))
            child++;    //오른쪽자식이됨. child가오른쪽자식이됨 
        if (temp.priority <= h->root[child].priority)
            break;    //맨밑에내려온듯 

        //한단계아래로이동 
        h->root[parent] = h->root[child];
        parent = child;
        child *= 2;
    }
    h->root[parent] = temp;

    return item;
}
