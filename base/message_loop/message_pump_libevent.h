// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"
#include "base/threading/thread_checker.h"

/**
 * @brief MessagePumpLibevent类的处理就比较复杂了.
 * 它除了处理Task任务，还要处理IPC消息，系统消息等。这时需要同时监听多个事件，
 * 就不能直接用条件变量了。虽然是用 libevent库 实现的，但底层是用epoll/kequeue
 * 系统调用，来同时监听多个文件描述符，来达到监听多个事件的目的。
 * 其中Task任务会阻塞在一个管道(pipe)的read函数上，在init函数中创建的该管道，然后
 * 在OnWakeup函数read，如果管道中没有数据在这里会阻塞住。在ScheduleWork函数中会
 * 像管道(pipe)中写数据以唤醒OnWakeup。至于ScheduleWork怎么调用的就不用多说了吧。
 * 而对IPC消息的监听会在WatchFileDescriptor函数注册需要监听的套接字，而该函数是
 * 在channel类的AcceptConnect函数调用过来的，具体流程可参见这里:
 * https://blog.csdn.net/qq295445028/article/details/8013699
 * 若是套接字有数据到达，触发事件后会调用 OnLibeventNotification函数 来处理
 * IPC消息，关于IPC消息的处理流程还是参见这里
 *
 * LibEvent深入浅出文档：
 * https://aceld.gitbooks.io/libevent/content/
 *
 * event_base 是 libevent 的事件处理框架，负责事件注册、删除等，属于 Reactor 模式
 * 中的Reactor。接口函数:
 * 1. event_new
 * 创建事件（涉及内存分配）。
 * 1. event_add  添加事件到event_base。
 * 2. event_del  将事件从监听列表（pending list）中移除。
 * 3. event_free 释放由event_new创建的事件（内存）。
 *
 * 使用 Libevent 库函数进行编程时，我们一般需要先分配一个或多个event_base结构体。
 * 每一个 event_base 都包含了一组事件，并且通过轮询确定哪些事件是激活的。
 *
 * 每一个 event_base 都有一个 “方法”（或称 “后端”（backend）），用来确定哪些事件是激活了的。
 * 这些 “方法” 主要有：select, poll, epoll, kqueue, devpoll, evport, win32等。
 * 用户可以使用环境变量来禁止某个或某些特定的后端方法。比如，通过设置 EVENT_NOKQUEUE 环境变量
 * 来关闭 kqueue 后端方法。
 * https://dulishu.top/libevent-event_base/
 *
 * event_base 相当于 epoll 红黑树的树根。
 * 每个 event_base 都有一种用于检测哪种事件已经就绪的 ”方法“, 或者说后端。
 * https://bbs.huaweicloud.com/blogs/264808
 *
 * 一旦有了一个已经注册了某些事件的 event_base, 就需要让 libevent 等待事件并且通知事件的发生
 * #define EVLOOP_ONCE             0x01
    事件只会被触发一次
    事件没有被触发, 阻塞等
 * #define EVLOOP_NONBLOCK         0x02
    非阻塞 等方式去做事件检测
    不关心事件是否被触发了
 * #define EVLOOP_NO_EXIT_ON_EMPTY 0x04
    没有事件的时候, 也不退出轮询检测

 * 数据缓冲区（Bufferevent）
 * 1. 是 libevent 为IO缓冲区操作提供的一种通用机制
 * 2. bufferevent 由一个底层的传输端口(如套接字)，一个读取缓冲区和一个写入缓冲区组成。
 * 3. 与通常的事件在底层传输端口已经就绪，可以读取或者写入的时候执行回调，不同的是，
 *    bufferevent 在读取或者写入了足够量的数据之后调用用户提供的回调。
 *
 * 如何等待自定义任务？
 * 假设现在MessageLoop没有任务和消息需要处理，就需要等待自定义任务到来。
 * 但不能盲目等待。chromium非常巧妙，他在 MessagePumpLibEvent 为例：在Linux
 * 上创建一个管道(pipe)，等待读取这个管道的内容，当有新的自定义任务到来的时候，就
 * 写入一个字节到管道中，从而MessageLoop被唤醒，简单直接。
 */

// Declare structs we need from libevent.h rather than including it
struct event_base; // libevent库
struct event; // libevent库

namespace base {

// Class to monitor sockets and issue callbacks when sockets are ready for I/O
// TODO(dkegel): add support for background file IO somehow
// 这里用到了一个第三方库libevent来进行事件注册、阻塞、唤醒等操作。
class BASE_EXPORT MessagePumpLibevent : public MessagePump,
                                        public WatchableIOMessagePumpPosix {
 public:
  class FdWatchController : public FdWatchControllerInterface {
   public:
    explicit FdWatchController(const Location& from_here);

    FdWatchController(const FdWatchController&) = delete;
    FdWatchController& operator=(const FdWatchController&) = delete;

    // Implicitly calls StopWatchingFileDescriptor.
    ~FdWatchController() override;

    // FdWatchControllerInterface:
    bool StopWatchingFileDescriptor() override;

   private:
    friend class MessagePumpLibevent;
    friend class MessagePumpLibeventTest;

    // Called by MessagePumpLibevent.
    void Init(std::unique_ptr<event> e);

    // Used by MessagePumpLibevent to take ownership of |event_|.
    std::unique_ptr<event> ReleaseEvent();

    void set_pump(MessagePumpLibevent* pump) { pump_ = pump; }
    MessagePumpLibevent* pump() const { return pump_; }

    void set_watcher(FdWatcher* watcher) { watcher_ = watcher; }

    void OnFileCanReadWithoutBlocking(int fd, MessagePumpLibevent* pump);
    void OnFileCanWriteWithoutBlocking(int fd, MessagePumpLibevent* pump);

    std::unique_ptr<event> event_;
    MessagePumpLibevent* pump_ = nullptr;
    FdWatcher* watcher_ = nullptr;
    // If this pointer is non-NULL, the pointee is set to true in the destructor.
    bool* was_destroyed_ = nullptr;
  };

  MessagePumpLibevent();

  MessagePumpLibevent(const MessagePumpLibevent&) = delete;
  MessagePumpLibevent& operator=(const MessagePumpLibevent&) = delete;

  ~MessagePumpLibevent() override;

  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           int mode,
                           FdWatchController* controller,
                           FdWatcher* delegate);

  // MessagePump methods:
  void Run(Delegate* delegate) override;
  void Quit() override;
  void ScheduleWork() override;
  void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;

 private:
  friend class MessagePumpLibeventTest;

  // Risky part of constructor.  Returns true on success.
  bool Init();

  // Called by libevent to tell us a registered FD can be read/written to.
  static void OnLibeventNotification(int fd, short flags, void* context);

  // Unix pipe used to implement ScheduleWork()
  // ... callback; called by libevent inside Run() when pipe is ready to read
  static void OnWakeup(int socket, short flags, void* context);

  struct RunState {
    explicit RunState(Delegate* delegate_in) : delegate(delegate_in) {}

    Delegate* const delegate;

    // Used to flag that the current Run() invocation should return ASAP.
    // 用于标记当前的 Run() 调用应尽快返回。
    bool should_quit = false;
  };

  // State for the current invocation of Run(). null if not running.
  // 当前调用 Run() 的状态。 如果未运行，则为 null。
  RunState* run_state_ = nullptr;

  // This flag is set if libevent has processed I/O events.
  // 如果 libevent 已处理 I/O 事件，则设置此标志。
  bool processed_io_events_ = false;

  // Libevent dispatcher.  Watches all sockets registered with it, and sends
  // readiness callbacks when a socket is ready for I/O.
  // Libevent 调度程序。监视向它注册的所有套接字，并在套接字准备好进行 I/O 时发送准备就绪回调。
  event_base* const event_base_; // libevent库

  /**
   * @brief
   * FIXME: 这里是该方案的一个亮点，相比于普通的生产者消费者消息循环机制而言，这里采用
   * 管道(pipe)的写入和读出口来响应Quit事件，无需生产者持有消息队列的锁，不阻塞。
   */
  // ... write end; ScheduleWork() writes a single byte to it
  // ...写结束；ScheduleWork() 向其写入一个字节
  int wakeup_pipe_in_ = -1; // pipe[1] 管道描述符读
  // ... read end; OnWakeup reads it and then breaks Run() out of its sleep
  // ...读结束；OnWakeup 读取它，然后将 Run() 从睡眠中中断
  int wakeup_pipe_out_ = -1; // pipe[0] 管道描述符写
  // ... libevent wrapper for read end
  // ... 用于读取结束的 libevent 包装器
  event* wakeup_event_ = nullptr; // 唤醒事件

  ThreadChecker watch_file_descriptor_caller_checker_;
};

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_LIBEVENT_H_
