#include "lab.h"
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

// Queue struct definition
typedef struct queue {
    void **buffer;              // Array of void* pointers to hold queue items
    int capacity;               // Maximum number of items the queue can hold
    int head;                   // Index of the next item to dequeue
    int tail;                   // Index of the next slot to enqueue into
    int count;                  // Current number of items in the queue
    bool shutdown;              // Flag indicating if shutdown has been initiated

    pthread_mutex_t lock;       // Mutex for synchronizing access to the queue
    pthread_cond_t not_empty;   // Condition variable to wait for queue to be non-empty
    pthread_cond_t not_full;    // Condition variable to wait for queue to be non-full
} queue;


// Initializes a new queue with the specified capacity.
queue_t queue_init(int capacity) {
    // Allocate memory for the queue struct
    queue_t q = malloc(sizeof(struct queue));
    if (!q) return NULL;

    // Allocate memory for the buffer that holds elements
    q->buffer = malloc(sizeof(void *) * capacity);
    if (!q->buffer) {
        free(q);
        return NULL;
    }

    // Initialize fields
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->shutdown = false;

    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);

    return q;
}


void queue_destroy(queue_t q) {
    if (!q) return;

    pthread_mutex_lock(&q->lock);           // Lock the queue to safely modify i
    q->shutdown = true;                     // Set shutdown flag
    pthread_cond_broadcast(&q->not_empty);  // Wake up any waiting consumers
    pthread_cond_broadcast(&q->not_full);   // Wake up any waiting producers
    pthread_mutex_unlock(&q->lock);         // Unlock the queue

    //Destroy and free everything
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);

    free(q->buffer);
    free(q);
}

void enqueue(queue_t q, void *data) {
    pthread_mutex_lock(&q->lock);       // Lock the queue

    // Wait until there is space or shutdown is signaled
    while (q->count == q->capacity && !q->shutdown) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    // If shutdown was triggered, exit immediately
    if (q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        return;
    }

    // Insert the new item at the tail index
    q->buffer[q->tail] = data;
    // Move the tail forward circularly
    q->tail = (q->tail + 1) % q->capacity;
    // Increment the count of items
    q->count++;

    pthread_cond_signal(&q->not_empty);     // Signal consumers that queue is not empty
    pthread_mutex_unlock(&q->lock);         // Unlock the queue
}


void *dequeue(queue_t q) {
    pthread_mutex_lock(&q->lock);

    // Wait until there is an item or shutdown is signaled
    while (q->count == 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    // If shutdown occurred and queue is still empty, return NULL
    if (q->count == 0 && q->shutdown) {
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    // Remove the item at the head index
    void *data = q->buffer[q->head];
    // Move the head forward circularly
    q->head = (q->head + 1) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->not_full);      // Signal producers that queue is not full
    pthread_mutex_unlock(&q->lock);

    return data;
}

void queue_shutdown(queue_t q) {
    pthread_mutex_lock(&q->lock);          // Lock the queue
    q->shutdown = true;                    // Set the shutdown flag
    pthread_cond_broadcast(&q->not_empty); // Wake all waiting consumers
    pthread_cond_broadcast(&q->not_full);  // Wake all waiting producers
    pthread_mutex_unlock(&q->lock);        // Unlock the queue
}

bool is_empty(queue_t q) {
    pthread_mutex_lock(&q->lock);        // Lock to safely check count
    bool result = (q->count == 0);       // Check if count is zero
    pthread_mutex_unlock(&q->lock);      // Unlock the queue
    return result;
}

bool is_shutdown(queue_t q) {
    pthread_mutex_lock(&q->lock);         // Lock to safely check shutdown flag
    bool result = q->shutdown;            // Read shutdown flag
    pthread_mutex_unlock(&q->lock);       // Unlock the queue
    return result;
}