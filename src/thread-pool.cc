/**
 * File: thread-pool.cc
 * --------------------
 * Presents the implementation of the ThreadPool class.
 */

#include "Semaphore.h"
#include "thread-pool.h"
#include <thread>
#include <mutex>
#include <iostream>
#include <unistd.h>
using namespace std;


ThreadPool::ThreadPool(size_t numThreads) : threads(numThreads), dtSemaphore(0), wtSemaphore(numThreads) {
    pending = true;
    wCounter = 0;
    dt = thread([this](){dispatcher();});
    for (int i = 0; i < numThreads; i++) {
        threads[i].t = thread([this, i](){worker(i);});
        threads[i].id = i;
        threads[i].busy = false;
    }
}

void ThreadPool::dispatcher(void) {
    while (1) {
        dtSemaphore.wait();
        wtSemaphore.wait();
        if (!pending) {
            break;
        }
        for (int j = 0; j < threads.size(); j++) {
            if (!threads[j].busy) {
                threads[j].busy = true;
                threads[j].sem.signal();
                break;
            }
        }
    }
    return;
}

void ThreadPool::worker(int id) {
    while (1) {
        threads[id].sem.wait();
        if (!pending) {
            break;
        }
        qmut.lock();
        counter_mut.lock();
        wCounter++;
        counter_mut.unlock();
        function<void(void)> thunk = thunks.front();
        thunks.pop();
        qmut.unlock();
        thunk();
        counter_mut.lock();
        wCounter--;
        if (wCounter == 0) {
            cv.notify_all();
        }
        counter_mut.unlock();
        threads[id].busy = false;
        wtSemaphore.signal();
    }
    return;
}
void ThreadPool::schedule(const function<void(void)> thunk) {
    qmut.lock();
    thunks.push(thunk);
    dtSemaphore.signal();
    qmut.unlock();
    return;
}
void ThreadPool::wait() {

    qmut.lock();
    counter_mut.lock();
    while (!(wCounter == 0 && thunks.size() == 0)) {
        counter_mut.unlock();
        cv.wait(qmut);
        counter_mut.lock();
    }
    counter_mut.unlock();
    qmut.unlock();

    return;
}
ThreadPool::~ThreadPool() {

    wait();
    pending = false;
    dtSemaphore.signal();
    wtSemaphore.signal();
    for (int i = 0; i < threads.size(); i++) {
        threads[i].sem.signal();
        threads[i].t.join();
    }
    dt.join();
}