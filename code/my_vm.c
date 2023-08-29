#include "my_vm.h"

#include <limits.h>
#include <math.h>
#include<string.h>
#include <pthread.h>
#include <stdint.h>

/*
Function responsible for allocating and setting your physical memory
*/
#define NUMPAGES MEMSIZE/PGSIZE
#define OFFSETBIT double offset
#define ADDRESS_SPACE 32
#define PAGELEVELS 2
void * base_address;
void * physical_bitmap;
void * virtual_bitmap;
pde_t * pgDir;
int offsetBits;
int pgTabOffsetBits;
int pgDirOffsetBits;
int no_of_pgLevels;
int no_of_EntriesPerPage;
int no_of_EntriesPGDir;
int no_of_EntriesPGTable;
int initialized = 0;
pthread_mutex_t lock;





//bitmap functions

void * initializeBitMap(int size){
    int num_size8_bits = size/8;
    void * bitmap = (void *)malloc(num_size8_bits);
    memset(bitmap,0,num_size8_bits);
                                                        
    return bitmap;
}



void set_bit(void *bitmap, int bitIndex){
                                                      
    int index_size8_bits = bitIndex/8;                                                   
    int bitoffset_size8_bits = bitIndex%8;                                                  
    uint8_t mask = 1<<(8-1-bitoffset_size8_bits);
    ((uint8_t *)bitmap)[index_size8_bits] ^=mask;
                                                    
}



int get_bit(void *bitmap, int bitIndex){
                                                      
    int index_size8_bits = bitIndex/8;
                                                   
    int bitoffset_size8_bits = bitIndex%8;
                                                    
    uint8_t bit = ((uint8_t *)bitmap)[index_size8_bits];
    uint8_t mask = 1<<(8-1-bitoffset_size8_bits);
    bit &= mask;
    bit = bit>>(8-1-bitoffset_size8_bits);
                                                    
    return bit==1?1:0;
    
}


unsigned long bitExtract(int addrSpace,int fromIndex,int noOfBits,uint32_t address){
                                                      
    uint32_t val = address;
                                                       
    val = val<<(fromIndex);
                                                       
    val = val>>(addrSpace-noOfBits);
                                                       
    return val;
}


void checkVirtualPage(uint32_t *directoryIndex, uint32_t *pageTableIndex, int numPages, int * index){
                                                      
     for(int i=0;i<(no_of_EntriesPGDir * no_of_EntriesPGTable);i++){
        if(get_bit(virtual_bitmap, i)==0){
            numPages--;
            int j=i+1;
            int status = 0;
            while(numPages>0){
                if(get_bit(virtual_bitmap, j)==0){
                    j++;
                    numPages--;
                    if(numPages==0){
                        status = 1;
                    }
                }
            }
            if(numPages<=0){
                status = 1;
                                                        
            }
            if(status==1){
            *directoryIndex = (uint32_t)(i/no_of_EntriesPGTable);
            *pageTableIndex = (uint32_t)(i - (*directoryIndex * no_of_EntriesPGTable));
                if(index != NULL)
                {*index = i;}
            break;
            }
        }
    }
}


void * checkPhysicalPage(int * index){
                                                        
    void * physical = malloc(sizeof(pte_t));
    for(int i =  0; i<NUMPAGES;i++){
        if(get_bit(physical_bitmap, i)==0){
            (*((pte_t *)physical)) = (pte_t)*pgDir + i*PGSIZE;
                                                             
            if(index!=NULL){
            *index = i;
            }
            return physical;
        }
    }
    *index = -1;
    return NULL;
}











void set_physical_mem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating

    
    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
                                                                 
    
    base_address = malloc(MEMSIZE);
    memset(base_address, 0, MEMSIZE);
    pgDir = malloc(sizeof(pde_t));
    *pgDir = (pde_t)base_address;
    physical_bitmap = initializeBitMap(NUMPAGES);
    set_bit(physical_bitmap, 0);

    offsetBits = (int)((log(PGSIZE*1.0))/(log(2.0)));
    pgTabOffsetBits = (int)((log(PGSIZE/sizeof(pte_t)*1.0)/log(2.0)));
    pgDirOffsetBits = ADDRESS_SPACE - pgTabOffsetBits-offsetBits;
    no_of_pgLevels = PAGELEVELS - 1;
    no_of_EntriesPerPage = PGSIZE/sizeof(pte_t);
    no_of_EntriesPGDir= PGSIZE/sizeof(pde_t)<pow(2,pgDirOffsetBits)?PGSIZE/sizeof(pde_t):pow(2,pgDirOffsetBits);
    no_of_EntriesPGTable = PGSIZE/sizeof(pte_t);
    
    virtual_bitmap  = initializeBitMap(no_of_EntriesPGDir * no_of_EntriesPGTable);
    
    
    
    int i =0;
    void * availablePhyPage = checkPhysicalPage(&i);
                                                                
    *((pde_t *)base_address) = (pde_t)*((pde_t *)availablePhyPage);
                                                               
    set_bit(physical_bitmap, i);
    set_bit(virtual_bitmap, 0);

}



/*
 * Part 2: Add a virtual to physical page translation to the TLB.
 * Feel free to extend the function arguments or return type.
 */
void init_TLB(){
    int count = TLB_ENTRIES;
    tlb_node* t_node = (tlb_node*)malloc(sizeof(tlb_node));
    tlb_store.head = t_node;
    tlb_store.head->pa = NULL;
    tlb_store.head->va = NULL;
    tlb_store.head->next = NULL;
    tlb_store.head->index = TLB_ENTRIES-count;
    tlb_store.miss = 0;
    tlb_store.lookup = 0;
    tlb_node *current = tlb_store.head;
    tlb_store.map[tlb_store.head->index] = 0;
    count--;
    while(count>0){
        if(current->next == NULL){
             tlb_node* t_node = (tlb_node*)malloc(sizeof(tlb_node));
            current->next = t_node;
            current = current->next;
            current->pa = NULL;
            current->va = NULL;
            current->next = NULL;
            current->index = TLB_ENTRIES-count;
            tlb_store.map[current->index] = 0;
            count--;
        }
    }
}
void tlb_evict(void* va,void* pa){
    int min = 9999999999;
    for(int i=0;i<TLB_ENTRIES;i++){
        if(tlb_store.map[i]<=min){
            min = tlb_store.map[i];
        }
    tlb_node* ptr = tlb_store.head;
    while(ptr->index!=min){
        ptr = ptr->next;
    }
    ptr->va = va;
    ptr->pa = (pte_t) pa;
    }
}
int add_TLB(void *va, void *pa)
{

    /*Part 2 HINT: Add a virtual to physical page translation to the TLB */
    tlb_node* ptr = tlb_store.head;
    while(ptr->next!=NULL){
        if(ptr->va==NULL){
            ptr->pa = (pte_t) pa;
            ptr->va = va;
            return 1;
        }
        ptr = ptr->next;
    }
    if(ptr->index==TLB_ENTRIES-1){
        tlb_evict(va,pa);
    }
    return -1;
}


/*
 * Part 2: Check TLB for a valid translation.
 * Returns the physical page address.
 * Feel free to extend this function and change the return type.
 */
pte_t *check_TLB(void *va) {

    /* Part 2: TLB lookup code here */
    tlb_store.lookup+=1;
                                   // printf("lookUp: %d\n",tlb_store.lookup);//testingPurpose
    tlb_node* ptr = tlb_store.head;
    while(ptr->next!=NULL){
        if(ptr->va==va){
            tlb_store.map[ptr->index]+=1;
            return(ptr->pa);
        }
        ptr = ptr->next;
    }
    tlb_store.miss+=1;
                                  //  printf("Miss: %d\n",tlb_store.miss);//testingPurpose
    return NULL;

}

void free_TLB(void *va){
    tlb_node* ptr = tlb_store.head;
    while(ptr->next!=NULL){
        if(ptr->va==va){
            tlb_store.map[ptr->index]=0;
            ptr->pa = NULL;
            ptr->va = NULL;
           return;
        }
        fprintf(stderr,"No such virtual address in TLB");
    }
}
/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void print_TLB_missrate()
{
    double miss_rate = tlb_store.miss*1.0/tlb_store.lookup;

    /*Part 2 Code here to calculate and print the TLB miss rate*/

    //return(miss_rate);


    fprintf(stderr, "TLB miss rate %lf \n", miss_rate);
}





/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t *translate(pde_t *pgdir, void *va) {
    /* Part 1 HINT: Get the Page directory index (1st level) Then get the
    * 2nd-level-page table index using the virtual address.  Using the page
    * directory index and page table index get the physical address.
    *
    * Part 2 HINT: Check the TLB before performing the translation. If
    * translation exists, then you can return physical address from the TLB.
    */
                                                                       
    pte_t * pa = check_TLB(va);
    if(pa!=NULL){
        return pa;
    }
    
    pde_t pgdir1 = *pgdir;
    
unsigned int physicalDirectIndex = (unsigned long)bitExtract(ADDRESS_SPACE, 0, pgDirOffsetBits, (uint32_t)va);
    void * physicalTable = *((pde_t *)((pgdir1)+ (sizeof(pte_t)*physicalDirectIndex)));
    if(physicalTable == 0){
        return NULL;
    }
unsigned int physicalTableIndex = (unsigned int)bitExtract(ADDRESS_SPACE, pgDirOffsetBits, pgTabOffsetBits, (uint32_t )va);//pageTable
    void * physicalPage  = *(((pte_t *)physicalTable)+ (sizeof(pte_t)*physicalTableIndex));
unsigned int physicalOffset =  bitExtract(ADDRESS_SPACE, pgDirOffsetBits+pgTabOffsetBits, offsetBits, (uint32_t )va);//offSet

    
    physicalPage +=physicalOffset;
    //If translation not successful, then return NULL
    add_TLB(va,physicalPage);
    return physicalPage;
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
page_map(pde_t *pgdir, void *va, void *pa)
{

    /*HINT: Similar to translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
                                                                   
    pde_t pgdir1 = *pgdir;
                                                                   
    unsigned int physicalDirectIndex = (unsigned long)bitExtract(ADDRESS_SPACE, 0, pgDirOffsetBits, (uint32_t )va);//pageDirec
                                                                   
    void * physicalTable = (pte_t)*((pde_t *)((pgdir1)+ (sizeof(pte_t)*physicalDirectIndex)));
    
                                                                   
        if(physicalTable == 0){
            int i =0;
            void * availablePhyPage = checkPhysicalPage(&i);
                                                                    
            if(availablePhyPage == NULL){
                printf("No memory left");
                return -1;
            }
            *((pde_t *)((pgdir1)+ (sizeof(pte_t)*physicalDirectIndex))) = (pde_t)*((pde_t *)availablePhyPage);
            physicalTable = (pde_t)*((pde_t *)availablePhyPage);
            set_bit(physical_bitmap, i);
        }
    unsigned int physicalTableIndex = (unsigned int)bitExtract(ADDRESS_SPACE, pgDirOffsetBits, pgTabOffsetBits, (uint32_t )va);//pageTable
        *(((pte_t *)physicalTable)+ (sizeof(pte_t)*physicalTableIndex)) = (pte_t)*(pte_t *)pa;

        add_TLB(va,(pte_t)*(pte_t *)pa);

    return 1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
    //Use virtual address bitmap to find the next free page
                                                        
    uint32_t diretory = 999999;
    uint32_t physical = 999999;
                                                        
    int i = 0;
    checkVirtualPage(&diretory, &physical,num_pages,&i);
                                                       
    uint32_t virtual_address = ((uint32_t)diretory<<(32-pgDirOffsetBits))|((uint32_t)physical<<offsetBits)|(uint32_t)0;

    set_bit(virtual_bitmap, i);
    return virtual_address;
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *t_malloc(unsigned int num_bytes) {

    /*
     * HINT: If the physical memory is not yet initialized, then allocate and initialize.
     */

   /*
    * HINT: If the page directory is not initialized, then initialize the
    * page directory. Next, using get_next_avail(), check if there are free pages. If
    * free pages are available, set the bitmaps and map a new page. Note, you will
    * have to mark which physical pages are used.
    */
    pthread_mutex_lock(&lock);
                                                         
    if(!initialized){
        set_physical_mem();
        init_TLB();
        initialized = 1;
    }
    int num_pages = (int)ceil(num_bytes*1.0/PGSIZE) ;
    
                                                        
    
    void *va =(uint32_t *) get_next_avail(num_pages);
    int i = 0;
    void  *pa = checkPhysicalPage(&i);
    if(pa ==NULL){
        printf("out Of Memory");
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    set_bit(physical_bitmap, i);
    if(page_map(pgDir,  va, pa)==-1){
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    num_pages--;
    while(num_pages>0){
        void *va = get_next_avail(num_pages);
        int i = 0;
        void  * pa = checkPhysicalPage(&i);
        if(pa ==NULL){
            printf("out Of Memory");
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        set_bit(physical_bitmap, i);
        if(page_map(pgDir,  va, pa)==-1){
            pthread_mutex_unlock(&lock);
            return NULL;
        }
        num_pages--;
    }
    pthread_mutex_unlock(&lock);
    return va;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void t_free(void *va, int size) {

    /* Part 1: Free the page table entries starting from this virtual address
     * (va). Also mark the pages free in the bitmap. Perform free only if the
     * memory from "va" to va+size is valid.
     *
     * Part 2: Also, remove the translation from the TLB
     */
    pthread_mutex_lock(&lock);
    int numPages = 0;
    unsigned long offset = bitExtract(ADDRESS_SPACE, pgDirOffsetBits+pgTabOffsetBits, offsetBits, (uint32_t )va);
    if(offset == 0){
        numPages = (int)ceil(size*1.0/PGSIZE);
    }else{
        unsigned long newSize = (unsigned long)size - (PGSIZE-offset);
        numPages = ((int)ceil(size*1.0/PGSIZE))+1;
    }
    va -= offset;
    void *pa[numPages];
    for(int i=0;i<numPages;i++){
        pa[i] = translate(pgDir, va+i*PGSIZE);
        if(pa[i]==NULL){
            
            pthread_mutex_unlock(&lock);
            return;
        }
    }
    for(int i=0;i<numPages;i++){
        pa[i] = translate(pgDir, va+i*PGSIZE);
        unsigned int physicalDirectIndex = (unsigned long)bitExtract(ADDRESS_SPACE, 0, pgDirOffsetBits, (uint32_t)va+i*PGSIZE);
        void * physicalTable = *((pde_t *)(((pde_t)*pgDir)+(sizeof(pte_t)*physicalDirectIndex)));
        unsigned int physicalTableIndex = (unsigned int)bitExtract(ADDRESS_SPACE, pgDirOffsetBits, pgTabOffsetBits, (uint32_t )va+i*PGSIZE);//pageTable
        int virtualBitIndex = physicalTableIndex + physicalDirectIndex*no_of_EntriesPerPage;
        int physicalBitIndex = ((pte_t)pa[i] - (pde_t)*pgDir)/PGSIZE;
        if(pa[i]!=NULL){
            memset(pa[i],0,PGSIZE);
        
        }
        *(((pte_t *)physicalTable)+ (sizeof(pte_t)*physicalTableIndex)) = 0;
        set_bit(physical_bitmap, physicalBitIndex);
        set_bit(virtual_bitmap, virtualBitIndex);
        
    }
    pthread_mutex_unlock(&lock);
}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
 * The function returns 0 if the put is successfull and -1 otherwise.
*/
void put_value(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
     * the contents of "val" to a physical page. NOTE: The "size" value can be larger
     * than one page. Therefore, you may have to find multiple pages using translate()
     * function.
     */
    pthread_mutex_lock(&lock);
    void *pa = translate(pgDir, va);
    if(pa == NULL){
        printf("Invalid address");
        pthread_mutex_unlock(&lock);
        return;
    }
    for(int i=0;i<size;i++) {
        *((char *)pa + i) = *((char *)val + i);
    }
    pthread_mutex_unlock(&lock);

}


/*Given a virtual address, this function copies the contents of the page to val*/
void get_value(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    * "val" address. Assume you can access "val" directly by derefencing them.
    */
    pthread_mutex_lock(&lock);
    void *pa = translate(pgDir, va);
    if(pa == NULL){
        printf("Invalid address");
        pthread_mutex_unlock(&lock);
        return;
    }
    for(int i=0;i<size;i++) {
        *((char *)val + i) = *((char *)pa + i);
    }
    pthread_mutex_unlock(&lock);

}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void mat_mult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
     * matrix accessed. Similar to the code in test.c, you will use get_value() to
     * load each element and perform multiplication. Take a look at test.c! In addition to
     * getting the values from two matrices, you will perform multiplication and
     * store the result to the "answer array"
     */
    int x, y, val_size = sizeof(int);
    int i, j, k;
    for (i = 0; i < size; i++) {
        for(j = 0; j < size; j++) {
            unsigned int a, b, c = 0;
            for (k = 0; k < size; k++) {
                int address_a = (unsigned int)mat1 + ((i * size * sizeof(int))) + (k * sizeof(int));
                int address_b = (unsigned int)mat2 + ((k * size * sizeof(int))) + (j * sizeof(int));
                get_value( (void *)address_a, &a, sizeof(int));
                get_value( (void *)address_b, &b, sizeof(int));
                // printf("Values at the index: %d, %d, %d, %d, %d\n",
                //     a, b, size, (i * size + k), (k * size + j));
                c += (a * b);
            }
            int address_c = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            // printf("This is the c: %d, address: %x!\n", c, address_c);
            put_value((void *)address_c, (void *)&c, sizeof(int));
        }
    }
}




