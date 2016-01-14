/*
 * IOHandler.h
 *
 *  Created on: Aug 13, 2015
 *      Author: Saman Barghi
 */

#ifndef IOHANDLER_H_
#define IOHANDLER_H_

#include <unordered_map>
#include <vector>
#include <sys/socket.h>
#include <mutex>
#include "runtime/uThread.h"

#define POLL_READY  ((uThread*)1)
#define POLL_WAIT   ((uThread*)2)


class Connection;
class kThread;

/*
 * Include poll data
 */
class PollData{
    friend Connection;
    friend IOHandler;
private:

    std::mutex mtx;                                     //Mutex that protects this  PollData
    int fd = -1;                                       //file descriptor

    uThread* rut = nullptr;
    uThread* wut = nullptr;                             //read and write uThreads

    std::atomic<bool> closing;

    //TODO: in case of multiple epollfd's, this structure can include
    //a pointer to the related epollfd that is being used on.
    void reset(){
       std::lock_guard<std::mutex> pdlock(this->mtx);
       rut = nullptr;
       wut = nullptr;
       closing = false;
    };

public:
    PollData( int fd) : fd(fd), closing(false){};
    PollData(): closing(false){};
    ~PollData(){};

};
/*
 * This class is a virtual class to provide nonblocking I/O
 * to uThreads. select/poll/epoll or other type of nonblocking
 * I/O can have their own class that inherits from IOHandler.
 * The purpose of this class is to be used as a thread local
 * I/O handler per kThread.
 */
class IOHandler{
    friend Connection;

    virtual int _Open(int fd, PollData *pd) = 0;         //Add current fd to the polling list, and add current uThread to IOQueue
    virtual int  _Close(int fd) = 0;
    virtual void _Poll(int timeout)=0;                ///



    void block(PollData &pd, bool isRead);
    void unblock(PollData *pd, bool isRead);


    static Cluster ioCluster;           //default IO Cluster
    static kThread  ioKT;               //default IO kThread
	static uThread*  ioUT;               //default IO uThread
    std::once_flag io_once_flag;

    static void io_once_function(){
        //IOHandler::ioUT->start(IOHandler::ioCluster, (ptr_t)IOHandler::defaultIOFunc, nullptr, nullptr, nullptr);
    }

protected:
    IOHandler(){
        std::call_once(io_once_flag, io_once_function);
    };
    void PollReady(PollData* pd, int flag);                   //When there is notification update pollData and unblock the related ut

public:
   ~IOHandler(){};  //should be protected
    /* public interfaces */
   void open(PollData &pd);
   int close(PollData &pd);
   void wait(PollData& pd, int flag);
   void poll(int timeout, int flag);
   void reset(PollData &pd);
   //dealing with uThreads

   //IO function for dedicated kThread
   static void defaultIOFunc(void*) __noreturn;

   //create an instance of IOHandler based on the platform
   static IOHandler* createIOHandler();
};

/* epoll wrapper */
class EpollIOHandler : public IOHandler {
private:
    static const int MAXEVENTS  = 1024;                           //Maximum number of events this thread can monitor, TODO: do we want this to be modified?
    int epoll_fd = -1;
    struct epoll_event* events;

    int _Open(int fd, PollData* pd);
    int  _Close(int fd);
    void _Poll(int timeout);
public:
    EpollIOHandler();
    virtual ~EpollIOHandler();
};


#endif /* IOHANDLER_H_ */