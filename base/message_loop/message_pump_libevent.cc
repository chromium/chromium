// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_libevent.h"

#include <errno.h>
#include <unistd.h>

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/third_party/libevent/event.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

/**
 * @brief
 * 普通生产者消费者模型，缺点是：生产者可能一直生产导致消息队列撑死，消费者可能一直消费导致饿死。
 * FIXME: 这里chromium采用 libevent 的原因？？？？？？
 * 举例：中间层的坑：https://t.wtturl.cn/NPXgC9s/
 * 这个问题是getInfoStickerPinState()大量快速调用sync()发送同步消息，导致消息循环队列过饱（
 * 大量滞留的sync消息），一直等到getInfoStickerPinState()这个同步消息停止执行后，才继续执行，
 * block了8s，日志如下：
  43710: [2022-03-23 GMT+08:00 01:46:51.183][31517:31517*][I][SessionLog][, , ][sync:112]Loop: sync.waiting: id=23234
  43711  [2022-03-23 GMT+08:00 01:46:51.184][31517:32533][W][SessionLog][, , ][sendMessage:210]Loop: sendMessage: id=23235, type=0, left.cout=151
  /////////////////////////////////////////这个消息卡了8s
  51377  [2022-03-23 GMT+08:00 01:46:59.126][31517:7715][I][SessionLog][, , ][setSyncPromise:345]Loop: message-sync, id=23234
 */

// Lifecycle of struct event 事件生命周期
// Libevent uses two main data structures:
// struct event_base (of which there is one per message pump), and
// struct event (of which there is roughly one per socket).
// The socket's struct event is created in
// MessagePumpLibevent::WatchFileDescriptor(),
// is owned by the FdWatchController, and is destroyed in
// StopWatchingFileDescriptor().
// It is moved into and out of lists in struct event_base by
// the libevent functions event_add() and event_del().
// Libevent 使用两种主要的数据结构：
// struct event_base（每个消息泵有一个）和 struct event（每个套接字大约有一个）。
// 套接字的结构事件在 MessagePumpLibevent::WatchFileDescriptor() 中创建，由
// FdWatchController 拥有，并在 StopWatchingFileDescriptor() 中销毁。
// 它通过 libevent 函数 event_add() 和 event_del() 移入和移出 struct event_base 中的列表。

namespace base {

MessagePumpLibevent::FdWatchController::FdWatchController(const Location& from_here)
    : FdWatchControllerInterface(from_here) {}

MessagePumpLibevent::FdWatchController::~FdWatchController() {
  if (event_) {
    CHECK(StopWatchingFileDescriptor()); // 析构时停止watch文件描述符
  }
  if (was_destroyed_) {
    DCHECK(!*was_destroyed_);
    *was_destroyed_ = true;
  }
}

bool MessagePumpLibevent::FdWatchController::StopWatchingFileDescriptor() {
  std::unique_ptr<event> e = ReleaseEvent();
  if (!e)
    return true;

  // event_del() is a no-op if the event isn't active.
  // 如果事件未激活，则 event_del() 是无操作的。如果事件不是未决的或者激活的，调用将没有效果。
  // 设置非未决，对已经初始化的事件调用 event_del() 将使其成为非未决和非激活的。
  int rv = event_del(e.get());
  pump_ = nullptr;
  watcher_ = nullptr;
  return (rv == 0);
}

void MessagePumpLibevent::FdWatchController::Init(std::unique_ptr<event> e) {
  DCHECK(e);
  DCHECK(!event_);

  event_ = std::move(e);
}

std::unique_ptr<event> MessagePumpLibevent::FdWatchController::ReleaseEvent() {
  return std::move(event_);
}

/**
 * @brief 非阻塞式的文件可读
 */
void MessagePumpLibevent::FdWatchController::OnFileCanReadWithoutBlocking(
    int fd,
    MessagePumpLibevent* pump) {

  // Since OnFileCanWriteWithoutBlocking() gets called first, it can stop
  // watching the file descriptor.
  // 由于首先调用 OnFileCanWriteWithoutBlocking()，它可以停止查看文件描述符。
  if (!watcher_)
    return;
  watcher_->OnFileCanReadWithoutBlocking(fd);
}

void MessagePumpLibevent::FdWatchController::OnFileCanWriteWithoutBlocking(
    int fd,
    MessagePumpLibevent* pump) {

  DCHECK(watcher_);
  watcher_->OnFileCanWriteWithoutBlocking(fd);
}

/**
 * @brief event_base_new() 创建 event_base(struct event_base*)
 * event_base_dispatch() 循环监听base对应的事件, 等待条件满足，等同于没有设置标志的
 * event_base_loop()，将一直运行，直到没有已经注册的事件了，或者调用了
 * event_base_loopbreak() 或者 event_base_loopexit() 为止。
 */
MessagePumpLibevent::MessagePumpLibevent() : event_base_(event_base_new()) {
  if (!Init())
    NOTREACHED();
  DCHECK_NE(wakeup_pipe_in_, -1);
  DCHECK_NE(wakeup_pipe_out_, -1);
  DCHECK(wakeup_event_);
}

MessagePumpLibevent::~MessagePumpLibevent() {
  DCHECK(wakeup_event_);
  DCHECK(event_base_);

  // 将 weakup_event_ 成为非未决和非激活的。
  event_del(wakeup_event_);
  delete wakeup_event_;
  if (wakeup_pipe_in_ >= 0) {
    if (IGNORE_EINTR(close(wakeup_pipe_in_)) < 0)
      DPLOG(ERROR) << "close";
  }
  if (wakeup_pipe_out_ >= 0) {
    if (IGNORE_EINTR(close(wakeup_pipe_out_)) < 0)
      DPLOG(ERROR) << "close";
  }

  // 释放event_base
  event_base_free(event_base_);
}

/**
 * @brief IO任务（多路）监听
 */
bool MessagePumpLibevent::WatchFileDescriptor(int fd,
                                              bool persistent,
                                              int mode,
                                              FdWatchController* controller,
                                              FdWatcher* delegate) {
  DCHECK_GE(fd, 0);
  DCHECK(controller);
  DCHECK(delegate);
  DCHECK(mode == WATCH_READ || mode == WATCH_WRITE || mode == WATCH_READ_WRITE);
  // WatchFileDescriptor should be called on the pump thread. It is not
  // threadsafe, and your watcher may never be registered.
  // 应在泵线程上调用 Watch FileDescriptor。 它不是线程安全的，你的观察者可能永远不会被注册。
  DCHECK(watch_file_descriptor_caller_checker_.CalledOnValidThread());

  // 转换为 LibEvent 的配置
  int event_mask = persistent ? EV_PERSIST : 0;
  if (mode & WATCH_READ) {
    event_mask |= EV_READ;
  }
  if (mode & WATCH_WRITE) {
    event_mask |= EV_WRITE;
  }

  std::unique_ptr<event> evt(controller->ReleaseEvent());
  if (!evt) {
    // Ownership is transferred to the controller. 所有权转移给控制器
    evt = std::make_unique<event>();
  } else {
    // Make sure we don't pick up any funky internal libevent masks.
    // 确保我们没有选择任何时髦的内部 libevent 掩码。
    int old_interest_mask = evt->ev_events & (EV_READ | EV_WRITE | EV_PERSIST);

    // Combine old/new event masks.
    event_mask |= old_interest_mask;

    // Must disarm the event before we can reuse it. 在我们可以重用它之前必须解除事件
    event_del(evt.get());

    // It's illegal to use this function to listen on 2 separate fds with the
    // same |controller|. 使用这个函数来监听 2 个具有相同 |controller| 的独立 fds 是非法的
    if (EVENT_FD(evt.get()) != fd) {
      NOTREACHED() << "FDs don't match" << EVENT_FD(evt.get()) << "!=" << fd;
      return false;
    }
  }

  // Set current interest mask and message pump for this event.
  // 为此事件设置当前兴趣掩码和消息泵
  event_set(evt.get(), fd, event_mask, OnLibeventNotification, controller);

  // Tell libevent which message pump this socket will belong to when we add it.
  // 当我们添加它时，告诉 libevent 这个套接字将属于哪个消息泵
  // 设置 event 从属的 event_base
  if (event_base_set(event_base_, evt.get())) { // 监听IO事件
    DPLOG(ERROR) << "event_base_set(fd=" << EVENT_FD(evt.get()) << ")";
    return false;
  }

  // Add this socket to the list of monitored sockets.
  // 将此 套接字 添加到受监视的套接字列表中。
  if (event_add(evt.get(), nullptr)) {
    DPLOG(ERROR) << "event_add failed(fd=" << EVENT_FD(evt.get()) << ")";
    return false;
  }

  controller->Init(std::move(evt));
  controller->set_watcher(delegate);
  controller->set_pump(this);
  return true;
}

// Tell libevent to break out of inner loop.
static void timer_callback(int fd, short events, void* context) {
  // 让 event_base 立即退出循环
  event_base_loopbreak((struct event_base*) context);
}

// Reentrant! 可重入
void MessagePumpLibevent::Run(Delegate* delegate) {
  RunState run_state(delegate);
  AutoReset<RunState*> auto_reset_run_state(&run_state_, &run_state);

  // event_base_loopexit() + EVLOOP_ONCE is leaky, see http://crbug.com/25641.
  // Instead, make our own timer and reuse it on each call to event_base_loop().
  // event_base_loopexit() + EVLOOP_ONCE 是泄漏的，请参阅 http://crbug.com/25641.
  // 相反，制作我们自己的计时器并在每次调用 event_base_loop() 时重用它。
  std::unique_ptr<event> timer_event(new event);

  // 优先执行自定义任务、IO任务，这些任务执行完之后，最后才会去执行定时器任务
  for (;;) {
    // Do some work and see if the next task is ready right away.
    // 做一些工作，看看下一个任务是否已经准备好。
    Delegate::NextWorkInfo next_work_info = delegate->DoWork();
    bool immediate_work_available = next_work_info.is_immediate();

    if (run_state.should_quit)
      break;

    // Process native events if any are ready. Do not block waiting for more.
    // 如果有任何准备就绪，则处理本机事件。不要阻塞等待更多。
    // 非阻塞方式去做事件检测，不关心事件是否被触发了，就是在DoWork()之后看一下libevent有没
    // 有要做的。所以可以看到它是在自己实现的事件循环里面又套了 libevent 的事件循环,
    // 只不过这个 libevent 是 nonblock，即每次只会执行一次就退出，同时它也具备唤醒的功能
    {
      auto scoped_do_work_item = delegate->BeginWorkItem();
      event_base_loop(event_base_, EVLOOP_NONBLOCK);
    }

    // 有立即执行的任务 || 正在处理IO事件
    bool attempt_more_work = immediate_work_available || processed_io_events_;
    processed_io_events_ = false;

    if (run_state.should_quit)
      break;

    if (attempt_more_work) // 如果是立即执行任务或io事件，则继续立即循环执行
      continue;

    attempt_more_work = delegate->DoIdleWork();

    if (run_state.should_quit)
      break;

    if (attempt_more_work) // 还有更多idle任务，则继续立即循环机制
      continue;

    // 执行到这里，下面是阻塞等待定时任务到来，再继续循环执行!

    bool did_set_timer = false;

    // If there is delayed work. 有延时任务，即下一个任务是延时任务，则阻塞等待延时到来
    DCHECK(!next_work_info.delayed_run_time.is_null());
    if (!next_work_info.delayed_run_time.is_max()) {
      // 执行下一个任务还需要的时长，设置到event_base中
      const TimeDelta delay = next_work_info.remaining_delay();

      // Setup a timer to break out of the event loop at the right time.
      // 设置一个计时器以在正确的时间跳出事件循环。
      struct timeval poll_tv;
      poll_tv.tv_sec = delay.InSeconds();
      poll_tv.tv_usec = delay.InMicroseconds() % Time::kMicrosecondsPerSecond;
      // 设置事件 timer_event 绑定的文件描述符(fd)或者信号，对于定时事件，设为-1即可，
      event_set(timer_event.get(), -1, 0, timer_callback, event_base_);
      event_base_set(event_base_, timer_event.get());
      event_add(timer_event.get(), &poll_tv);

      did_set_timer = true;
    }

    // Block waiting for events and process all available upon waking up. This
    // is conditionally interrupted to look for more work if we are aware of a
    // delayed task that will need servicing.
    // 阻止等待事件并在唤醒时处理所有可用的事件。如果我们知道需要服务的延迟任务，
    // 则有条件地中断以寻找更多工作。
    delegate->BeforeWait();

    // 一旦有了一个已经注册了某些事件的 event_base, 就需要让 libevent 等待事件并且通知事件的
    // 发生正常退出返回0, 失败返回-1，EVLOOP_ONCE：事件只会被触发一次，事件没有被触发则阻塞.
    event_base_loop(event_base_, EVLOOP_ONCE);

    // We previously setup a timer to break out the event loop to look for more
    // work. Now that we're here delete the event.
    // 我们之前设置了一个计时器来打破事件循环以寻找更多工作。 现在我们在这里删除事件。
    if (did_set_timer) {
      event_del(timer_event.get());
    }

    if (run_state.should_quit)
      break;
  }
}

void MessagePumpLibevent::Quit() {
  DCHECK(run_state_) << "Quit was called outside of Run!";
  // Tell both libevent and Run that they should break out of their loops.
  // 告诉 libevent 和 Run 他们应该跳出他们的循环。
  run_state_->should_quit = true;
  ScheduleWork();
}

/**
 * @brief 调度执行
 */
void MessagePumpLibevent::ScheduleWork() {
  // Tell libevent (in a threadsafe way) that it should break out of its loop.
  char buf = 0;
  int nwrite = HANDLE_EINTR(write(wakeup_pipe_in_, &buf, 1));
  DPCHECK(nwrite == 1 || errno == EAGAIN) << "nwrite:" << nwrite;
}

void MessagePumpLibevent::ScheduleDelayedWork(const TimeTicks& delayed_work_time) {
  // We know that we can't be blocked on Run()'s |timer_event| right now since
  // this method can only be called on the same thread as Run(). Hence we have
  // nothing to do here, this thread will sleep in Run() with the correct
  // timeout when it's out of immediate tasks.
  // 我们知道我们不能被 Run() 的 |timer_event| 阻塞。现在因为这个方法只能在与 Run() 相
  // 同的线程上调用。 因此我们在这里无事可做，当它没有立即任务时，这个线程将在 Run() 中以
  // 正确的超时休眠。
}

/**
 * @brief 初始化时，设置管道（pipe）作为 MessagePump（消息泵）从阻塞态激活进入死循
 * 环的手段, 类似接收用户调用的 std::condition_variable::notify() 唤醒消息队列.
 */
bool MessagePumpLibevent::Init() {
  int fds[2];
  if (!CreateLocalNonBlockingPipe(fds)) { // 创建非阻塞管道(Pipeline)
    DPLOG(ERROR) << "pipe creation failed";
    return false;
  }
  wakeup_pipe_out_ = fds[0]; // 管道读出
  wakeup_pipe_in_ = fds[1];  // 管道写入

  // 用于把 管道(Pipeline) add到 event中监听，主要是监听
  wakeup_event_ = new event; // libevent库
  // EV_PERSIST: 持续触发；EV_ET: 边沿模式
  // 向 wakeup_event_ 添加 wakeup_pipe_out_(pipe[0]) 监听
  // 这里使用管道时为啥不直接在多进程中直接使用管道自有的读写能力，而选择把pipe[0]使
  // 用epoll监听起来？
  // 答案是：管道本身的pipe[0]读时是阻塞等待的，一直等到有pipe[1]写如时才会唤醒读取；
  // 而这个阻塞式会导致进程停下来，并不好，所以使用epoll监听pipe[0]，这种休眠式的阻
  // 塞读取更好，因为不会一直阻塞进程，进程中的其他线程可以干其他事情，这里也是管道常
  // 用的使用方案：即pipe与epoll成对配合使用，高效又稳定。
  // 在Chromium中管道是用异步方式使用的，确保没有哪个端会等待另一个端。
  event_set(wakeup_event_, wakeup_pipe_out_, EV_READ | EV_PERSIST, OnWakeup, this);
  event_base_set(event_base_, wakeup_event_);

  if (event_add(wakeup_event_, nullptr))
    return false;
  return true;
}

// static
void MessagePumpLibevent::OnLibeventNotification(int fd,
                                                 short flags,
                                                 void* context) {

  FdWatchController* controller = static_cast<FdWatchController*>(context);
  DCHECK(controller);
  TRACE_EVENT("toplevel", "OnLibevent", "fd", fd);

  TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION heap_profiler_scope(
      controller->created_from_location().file_name());

  MessagePumpLibevent* pump = controller->pump();
  pump->processed_io_events_ = true;

  // Make the MessagePumpDelegate aware of this other form of "DoWork". Skip if
  // OnLibeventNotification is called outside of Run() (e.g. in unit tests).
  // 让 MessagePumpDelegate 了解这种其他形式的 “DoWork”。 如果在 Run() 之外调用
  // OnLibeventNotification 则跳过（例如在单元测试中）
  Delegate::ScopedDoWorkItem scoped_do_work_item;
  if (pump->run_state_)
    scoped_do_work_item = pump->run_state_->delegate->BeginWorkItem();

  if ((flags & (EV_READ | EV_WRITE)) == (EV_READ | EV_WRITE)) {
    // Both callbacks will be called. It is necessary to check that |controller|
    // is not destroyed. 两个回调都将被调用。 有必要检查 |controller| 没有被破坏。
    bool controller_was_destroyed = false;
    controller->was_destroyed_ = &controller_was_destroyed;
    controller->OnFileCanWriteWithoutBlocking(fd, pump);
    if (!controller_was_destroyed)
      controller->OnFileCanReadWithoutBlocking(fd, pump);
    if (!controller_was_destroyed)
      controller->was_destroyed_ = nullptr;
  } else if (flags & EV_WRITE) {
    controller->OnFileCanWriteWithoutBlocking(fd, pump);
  } else if (flags & EV_READ) {
    controller->OnFileCanReadWithoutBlocking(fd, pump);
  }
}

// Called if a byte is received on the wakeup pipe. 响应管道读
void MessagePumpLibevent::OnWakeup(int socket, short flags, void* context) {
  MessagePumpLibevent* that = static_cast<MessagePumpLibevent*>(context);
  DCHECK(that->wakeup_pipe_out_ == socket); // 检查是否是这个fd

  // Remove and discard the wakeup byte.
  char buf;
  int nread = HANDLE_EINTR(read(socket, &buf, 1));
  DCHECK_EQ(nread, 1);
  that->processed_io_events_ = true;

  // Tell libevent to break out of inner loop.
  // 让 event_base 立即退出循环，它与 event_base_loopexit(base, NULL)的不同在于,
  // 如果 event_base 当前正在执行激活事件的回调, 它将在执行完当前正在处理的事件后立即退出。
  // event_base_loopexit()让 event_base 在给定时间之后停止循环。如果 tv 参数为 NULL,
  // event_base 会立即停止循环,没有延时。如果 event_base 当前正在执行任何激活事件的回调,
  // 则回调会继续运行, 直到运行完所有激活事件的回调之才退出。
  event_base_loopbreak(that->event_base_);
}

}  // namespace base
