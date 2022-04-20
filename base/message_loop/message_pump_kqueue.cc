// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_kqueue.h"

#include <sys/errno.h>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/message_loop/timer_slack.h"
#include "base/posix/eintr_wrapper.h"
#include "base/time/time_override.h"

namespace base {

namespace {

// Prior to macOS 10.12, a kqueue could not watch individual Mach ports, only
// port sets. MessagePumpKqueue will directly use Mach ports in the kqueue if
// it is possible.
// 在 macOS 10.12 之前，kqueue 无法查看单个 Mach 端口，只能查看端口集。
// 如果可能，MessagePumpKqueue 将直接使用 kqueue 中的 Mach 端口。
bool KqueueNeedsPortSet() { // 是否支持端口集
  static const bool kqueue_needs_port_set = mac::IsAtMostOS10_11();
  return kqueue_needs_port_set;
}

#if DCHECK_IS_ON()
// Prior to macOS 10.14, kqueue timers may spuriously wake up, because earlier
// wake ups race with timer resets in the kernel. As of macOS 10.14, updating a
// timer from the thread that reads the kqueue does not cause spurious wakeups.
// Note that updating a kqueue timer from one thread while another thread is
// waiting in a kevent64 invocation is still (inherently) racy.
// 在 macOS 10.14 之前，kqueue 计时器可能会虚假唤醒，因为较早的唤醒与内核中的计时器重置竞争。
// 从 macOS 10.14 开始，从读取 kqueue 的线程更新计时器不会导致虚假唤醒。请注意，当另一个线
// 程在 kevent64 调用中等待时，从一个线程更新 kqueue 计时器仍然（本质上）活泼。
bool KqueueTimersSpuriouslyWakeUp() {
  static const bool kqueue_timers_spuriously_wakeup = mac::IsAtMostOS10_13();
  return kqueue_timers_spuriously_wakeup;
}
#endif

/**
 * @brief 设置kqueue监听，并通过event指针返回有变化的事件
 */
int ChangeOneEvent(const ScopedFD& kqueue, kevent64_s* event) {
  // kevent() 和 kevent64() 系统调用用于向队列注册事件，并将任何未决事件返回给用户。
  // changelist 参数是一个指向 kevent 或 kevent64_s 结构数组的指针，如
  // <sys/event.h> 中所定义。 在从队列中读取任何未决事件之前，将应用更改列表中包含的
  // 所有更改。 nchanges 参数给出了更改列表的大小。eventlist 参数是一个指向 kevent
  // 或 kevent64_s 结构数组的指针。 nevents 参数确定事件列表的大小。
  // 如果 timeout 是一个非 NULL 指针，它指定等待事件的最大间隔，这将被解释为
  // struct timespec。如果 timeout 是一个 NULL 指针，那么 kevent() 和 kevent64()
  // 都会无限期地等待。 为了进行轮询，超时参数应该是非空的，指向一个零值的时间规范结构。
  // 相同的数组可用于更改列表和事件列表。
  // kevent()函数的功能有：
  // 1. 向kqueue中添加监听事件 (根据 envent->flag 而定)
  // 2. 从kqueue中删除监听事件 (根据 envent->flag 而定)
  // 3. 读取kqueue中的事件等等
  // 第二个参数：changeList持有kqueue要监测的新事件集合，也监测对事件的任何变化，如果
  // changeList非空且numChanges大于0，那么changeList内的事件将应用于kqueue.
  // eventList持有kevent()正在报告的事件
  return HANDLE_EINTR(kevent64(kqueue.get(), event, 1, nullptr, 0, 0, nullptr));
}

}  // namespace

MessagePumpKqueue::FdWatchController::FdWatchController(
    const Location& from_here)
    : FdWatchControllerInterface(from_here) {}

MessagePumpKqueue::FdWatchController::~FdWatchController() {
  StopWatchingFileDescriptor();
}

bool MessagePumpKqueue::FdWatchController::StopWatchingFileDescriptor() {
  if (!pump_)
    return true;
  return pump_->StopWatchingFileDescriptor(this);
}

void MessagePumpKqueue::FdWatchController::Init(WeakPtr<MessagePumpKqueue> pump,
                                                int fd,
                                                int mode,
                                                FdWatcher* watcher) {
  DCHECK_NE(fd, -1);
  DCHECK(!watcher_);
  DCHECK(watcher);
  DCHECK(pump);

  fd_ = fd;
  mode_ = mode;
  watcher_ = watcher;
  pump_ = pump;
}

void MessagePumpKqueue::FdWatchController::Reset() {
  fd_ = -1;
  mode_ = 0;
  watcher_ = nullptr;
  pump_ = nullptr;
}

MessagePumpKqueue::MachPortWatchController::MachPortWatchController(
    const Location& from_here)
    : from_here_(from_here) {}

MessagePumpKqueue::MachPortWatchController::~MachPortWatchController() {
  StopWatchingMachPort();
}

bool MessagePumpKqueue::MachPortWatchController::StopWatchingMachPort() {
  if (!pump_)
    return true;
  return pump_->StopWatchingMachPort(this);
}

void MessagePumpKqueue::MachPortWatchController::Init(
    WeakPtr<MessagePumpKqueue> pump,
    mach_port_t port,
    MachPortWatcher* watcher) {
  DCHECK(!watcher_);
  DCHECK(watcher);
  DCHECK(pump);
  port_ = port;
  watcher_ = watcher;
  pump_ = pump;
}

void MessagePumpKqueue::MachPortWatchController::Reset() {
  port_ = MACH_PORT_NULL;
  watcher_ = nullptr;
  pump_ = nullptr;
}

/**
 * @brief 构造函数中创建kqueue文件描述符，分配Mach端口 或 把 wakeup_ 设置到
 * Match端口集，或者直接监听wakeup_事件，设置到kqueue监控。
 */
MessagePumpKqueue::MessagePumpKqueue()
    : kqueue_(kqueue()), // kqueue()返回一个kqueue文件描述符
      is_ludicrous_timer_slack_enabled_(base::IsLudicrousTimerSlackEnabled()),
      ludicrous_timer_slack_was_suspended_(
          base::IsLudicrousTimerSlackSuspended()),
      weak_factory_(this) {
  PCHECK(kqueue_.is_valid()) << "kqueue";

  // Create a Mach port that will be used to wake up the pump by sending
  // a message in response to ScheduleWork(). This is significantly faster than
  // using an EVFILT_USER event, especially when triggered across threads.
  // 创建一个 Mach 端口，用于通过发送消息响应 ScheduleWork() 来唤醒泵。
  // 这比使用 EVFILT_USER 事件要快得多，尤其是在跨线程触发时。
  // 通过mach_port_allocate()在内核中建立一个消息队列，并返回一个与之对应的的 Mach Port
  // 分配wakeup_有发送权限的共享端口
  kern_return_t kr = mach_port_allocate(
      mach_task_self(), // ipc空间
      MACH_PORT_RIGHT_RECEIVE, // 接收类型的Mach端口
      base::mac::ScopedMachReceiveRight::Receiver(wakeup_).get());
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_allocate";

  kevent64_s event{};
  // 在 macOS 10.12 之前，kqueue 无法查看单个 Mach 端口，只能查看端口集。
  // 如果可能，MessagePumpKqueue 将直接使用 kqueue 中的 Mach 端口。
  if (KqueueNeedsPortSet()) { // 是否支持端口集
    kr = mach_port_allocate(mach_task_self(),
                            MACH_PORT_RIGHT_PORT_SET,
                            mac::ScopedMachPortSet::Receiver(port_set_).get());
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_allocate PORT_SET";
    // 把 wakeup_ 加入 port_set_(Mach端口集合)中
    kr = mach_port_insert_member(mach_task_self(), wakeup_.get(),
                                 port_set_.get());
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_insert_member";

    event.ident = port_set_.get();  // 事件标识符为 Mach Port集合
    event.filter = EVFILT_MACHPORT; // 过滤类型是 Mach portsets
    event.flags = EV_ADD;           // 添加事件
  } else {
    // When not using a port set, the wakeup port event can be specified to
    // directly receive the Mach message as part of the kevent64() syscall.
    // This is not done when using a port set, since that would potentially
    // receive client MachPortWatchers' messages.
    // 当不使用端口集时，可以指定唤醒端口事件以直接接收 Mach 消息作为 kevent64()
    // 系统调用的一部分。 使用端口集时不会这样做，因为这可能会接收客户端
    // MachPortWatchers 的消息。
    event.ident = wakeup_.get();    // 唤醒文件描述符
    event.filter = EVFILT_MACHPORT; // 过滤类型是 Mach portsets
    event.flags = EV_ADD;
    event.fflags = MACH_RCV_MSG; // 接收消息
    // 用于接收发送到 |wakeup_| 的消息的暂存缓冲区
    event.ext[0] = reinterpret_cast<uint64_t>(&wakeup_buffer_);
    event.ext[1] = sizeof(wakeup_buffer_);
  }
  // 向kqueue（IO多路复用）中添加监听事件：Mach端口集监听(内含wakeup_) 或 直接接收唤醒事件(wakeup_)
  int rv = ChangeOneEvent(kqueue_, &event);
  PCHECK(rv == 0) << "kevent64";
}

MessagePumpKqueue::~MessagePumpKqueue() {}

void MessagePumpKqueue::Run(Delegate* delegate) {
  AutoReset<bool> reset_keep_running(&keep_running_, true);

  while (keep_running_) {
    mac::ScopedNSAutoreleasePool pool;

    bool do_more_work = DoInternalWork(delegate, nullptr);
    if (!keep_running_)
      break;

    Delegate::NextWorkInfo next_work_info = delegate->DoWork();
    do_more_work |= next_work_info.is_immediate();
    if (!keep_running_)
      break;

    if (do_more_work)
      continue;

    do_more_work |= delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (do_more_work)
      continue;

    DoInternalWork(delegate, &next_work_info);
  }
}

void MessagePumpKqueue::Quit() {
  keep_running_ = false;
  ScheduleWork();
}

void MessagePumpKqueue::ScheduleWork() {
  // 向 Mach端口发送消息 wakeup_
  mach_msg_empty_send_t message{};
  message.header.msgh_size = sizeof(message);
  // 一次消息类型的mask
  message.header.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MAKE_SEND_ONCE);
  // 远端Mach端口（支持跨进程）
  message.header.msgh_remote_port = wakeup_.get();
  // 这里是真正向Mach端口发送消息，触发kqueue监听的事件集合中的port_set中
  // 的wakeup_这个Mach端口消息
  kern_return_t kr = mach_msg_send(&message.header);
  if (kr != KERN_SUCCESS) {
    // If ScheduleWork() is being called by other threads faster than the pump
    // can dispatch work, the kernel message queue for the wakeup port can fill
    // up (this happens under base_perftests, for example). The kernel does
    // return a SEND_ONCE right in the case of failure, which must be destroyed
    // to avoid leaking.
    // 如果 ScheduleWork() 被其他线程调用的速度快于泵可以分派工作的速度（过饱），则唤醒
    // 端口的内核消息队列可能会填满（例如，这发生在 base_perftests 下）。 内核在失败的
    // 情况下会返回一个 SEND_ONCE 权限，必须将其销毁以避免泄漏。
    MACH_DLOG_IF(ERROR, (kr & ~MACH_MSG_IPC_SPACE) != MACH_SEND_NO_BUFFER, kr)
        << "mach_msg_send";
    mach_msg_destroy(&message.header);
  }
}

void MessagePumpKqueue::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  // Nothing to do. This MessagePump uses DoWork().
}

/**
 * @brief 监听Match端口
 */
bool MessagePumpKqueue::WatchMachReceivePort(
    mach_port_t port,
    MachPortWatchController* controller,
    MachPortWatcher* delegate) {

  DCHECK(port != MACH_PORT_NULL);
  DCHECK(controller);
  DCHECK(delegate);

  if (controller->port() != MACH_PORT_NULL) {
    DLOG(ERROR)
        << "Cannot use the same MachPortWatchController while it is active";
    return false;
  }

  if (KqueueNeedsPortSet()) { // 是否支持端口集
    // 把端口port插入到端口集中
    kern_return_t kr =
        mach_port_insert_member(mach_task_self(), port, port_set_.get());
    if (kr != KERN_SUCCESS) {
      MACH_LOG(ERROR, kr) << "mach_port_insert_member";
      return false;
    }
  } else {
    // 新建该端口port的事件
    kevent64_s event{};
    event.ident = port;
    event.filter = EVFILT_MACHPORT;
    event.flags = EV_ADD;
    int rv = ChangeOneEvent(kqueue_, &event);
    if (rv < 0) {
      DPLOG(ERROR) << "kevent64";
      return false;
    }
    ++event_count_;
  }

  controller->Init(weak_factory_.GetWeakPtr(), port, delegate);
  port_controllers_.AddWithID(controller, port);

  return true;
}

/**
 * @brief 监听文件描述符
 */
bool MessagePumpKqueue::WatchFileDescriptor(int fd,
                                            bool persistent,
                                            int mode,
                                            FdWatchController* controller,
                                            FdWatcher* delegate) {
  DCHECK_GE(fd, 0);
  DCHECK(controller);
  DCHECK(delegate);
  DCHECK_NE(mode & Mode::WATCH_READ_WRITE, 0);

  if (controller->fd() != -1 && controller->fd() != fd) {
    DLOG(ERROR) << "Cannot use the same FdWatchController on two different FDs";
    return false;
  }
  StopWatchingFileDescriptor(controller);

  std::vector<kevent64_s> events;
  // 设置kqueue的事件参数
  kevent64_s base_event{};
  base_event.ident = fd;
  base_event.flags = EV_ADD | (!persistent ? EV_ONESHOT : 0);

  if (mode & Mode::WATCH_READ) {
    base_event.filter = EVFILT_READ;
    base_event.udata = fd_controllers_.Add(controller);
    events.push_back(base_event);
  }
  if (mode & Mode::WATCH_WRITE) {
    base_event.filter = EVFILT_WRITE;
    base_event.udata = fd_controllers_.Add(controller);
    events.push_back(base_event);
  }

  // 把events中的事件键入kqueue多路复用机制中
  int rv = HANDLE_EINTR(kevent64(kqueue_.get(), events.data(), events.size(),
                                 nullptr, 0, 0, nullptr));
  if (rv < 0) {
    DPLOG(ERROR) << "WatchFileDescriptor kevent64";
    return false;
  }

  event_count_ += events.size();
  controller->Init(weak_factory_.GetWeakPtr(), fd, mode, delegate);

  return true;
}

bool MessagePumpKqueue::
    GetIsLudicrousTimerSlackEnabledAndNotSuspendedForTesting() const {
  return IsLudicrousTimerSlackEnabledAndNotSuspended();
}

void MessagePumpKqueue::MaybeUpdateWakeupTimerForTesting(
    const base::TimeTicks& wakeup_time) {
  MaybeUpdateWakeupTimer(wakeup_time);
}

/**
 * @brief 设置定时器唤醒事件
 */
void MessagePumpKqueue::SetWakeupTimerEvent(const base::TimeTicks& wakeup_time,
                                            bool use_slack,
                                            kevent64_s* timer_event) {
  // The ident of the wakeup timer. There's only the one timer as the pair
  // (ident, filter) is the identity of the event.
  constexpr uint64_t kWakeupTimerIdent = 0x0;
  timer_event->ident = kWakeupTimerIdent;
  timer_event->filter = EVFILT_TIMER;
  if (wakeup_time == base::TimeTicks::Max()) {
    timer_event->flags = EV_DELETE; // 删除
  } else {
    timer_event->filter = EVFILT_TIMER; // 定时器时间
    // This updates the timer if it already exists in |kqueue_|.
    timer_event->flags = EV_ADD | EV_ONESHOT; // 增加 | 单次有效

    // Specify the sleep in microseconds to avoid undersleeping due to
    // numeric problems. The sleep is computed from TimeTicks::Now rather than
    // NextWorkInfo::recent_now because recent_now is strictly earlier than
    // current wall-clock. Using an earlier wall clock time  to compute the
    // delta to the next wakeup wall-clock time would guarantee oversleep.
    // If wakeup_time is in the past, the delta below will be negative and the
    // timer is set immediately.
    timer_event->fflags = NOTE_USECONDS; // 单位us
    // 传递剩余时长
    timer_event->data = (wakeup_time - base::TimeTicks::Now()).InMicroseconds();

    if (use_slack) {
      // Specify ludicrous slack when the experiment is enabled and hasn't
      // been process-locally suspended.
      // See "man kqueue" in recent macOSen for documentation.
      timer_event->fflags |= NOTE_LEEWAY;
      timer_event->ext[1] = GetLudicrousTimerSlack().InMicroseconds();
    }
  }
}

bool MessagePumpKqueue::StopWatchingMachPort(
    MachPortWatchController* controller) {
  mach_port_t port = controller->port();
  controller->Reset();
  port_controllers_.Remove(port);

  if (KqueueNeedsPortSet()) { // 是否支持端口集
    // 从Mach端口集合中删除目标端口port
    kern_return_t kr =
        mach_port_extract_member(mach_task_self(), port, port_set_.get());
    if (kr != KERN_SUCCESS) {
      MACH_LOG(ERROR, kr) << "mach_port_extract_member";
      return false;
    }
  } else { // 不支持端口集合
    kevent64_s event{};
    event.ident = port;
    event.filter = EVFILT_MACHPORT;
    event.flags = EV_DELETE;
    --event_count_;
    // 把event加入到kqueue_的消息循环中，这里是删除port
    int rv = ChangeOneEvent(kqueue_, &event);
    if (rv < 0) {
      DPLOG(ERROR) << "kevent64";
      return false;
    }
  }

  return true;
}

bool MessagePumpKqueue::StopWatchingFileDescriptor(
    FdWatchController* controller) {
  int fd = controller->fd();
  int mode = controller->mode();
  controller->Reset();

  if (fd == -1)
    return true;

  std::vector<kevent64_s> events;

  kevent64_s base_event{};
  base_event.ident = fd;
  base_event.flags = EV_DELETE; // 删除事件

  if (mode & Mode::WATCH_READ) {
    base_event.filter = EVFILT_READ;
    events.push_back(base_event);
  }
  if (mode & Mode::WATCH_WRITE) {
    base_event.filter = EVFILT_WRITE;
    events.push_back(base_event);
  }
  // 向 kqueue 注册events监听，这里是删除操作，即把fd对应的event从kqueue监控队列中删除
  int rv = HANDLE_EINTR(kevent64(kqueue_.get(), events.data(), events.size(),
                                 nullptr, 0, 0, nullptr));
  DPLOG_IF(ERROR, rv < 0) << "StopWatchingFileDescriptor kevent64";

  // The keys for the IDMap aren't recorded anywhere (they're attached to the
  // kevent object in the kernel), so locate the entries by controller pointer.
  // IDMap 的 keys 没有记录在任何地方(它们附加到内核中的 kevent 对象)，因此通过控制器指针定位条目
  for (auto it = IDMap<FdWatchController*>::iterator(&fd_controllers_);
       !it.IsAtEnd(); it.Advance()) {
    if (it.GetCurrentValue() == controller) {
      fd_controllers_.Remove(it.GetCurrentKey());
    }
  }

  event_count_ -= events.size();

  return rv >= 0;
}

bool MessagePumpKqueue::DoInternalWork(Delegate* delegate,
                                       Delegate::NextWorkInfo* next_work_info) {
  if (events_.size() < event_count_) {
    events_.resize(event_count_);
  }

  bool immediate = next_work_info == nullptr;
  int flags = immediate ? KEVENT_FLAG_IMMEDIATE : 0;

  if (!immediate) {
    MaybeUpdateWakeupTimer(next_work_info->delayed_run_time);
    DCHECK_EQ(scheduled_wakeup_time_, next_work_info->delayed_run_time);
    delegate->BeforeWait();
  }
  // 在kqueue上阻塞等待，直到内核队列(kqueue)通知有事件变更，
  // 内核以events_参数通知出来
  int rv = HANDLE_EINTR(kevent64(kqueue_.get(), nullptr, 0, events_.data(),
                                 events_.size(), flags, nullptr));

  PCHECK(rv >= 0) << "kevent64";
  if (rv == 0) {
    // No events to dispatch so no need to call ProcessEvents().
    return false;
  }
  // 遍历events_，根据filter判定可读、可写事件、以及EVFILT_MACHPORT
  return ProcessEvents(delegate, rv);
}

bool MessagePumpKqueue::ProcessEvents(Delegate* delegate, int count) {
  bool did_work = false;

  for (int i = 0; i < count; ++i) { // 遍历event_
    auto* event = &events_[i];
    if (event->filter == EVFILT_READ || event->filter == EVFILT_WRITE) {
      // IO事件
      did_work = true;

      FdWatchController* controller = fd_controllers_.Lookup(event->udata);
      if (!controller) {
        // The controller was removed by some other work callout before
        // this event could be processed.
        continue;
      }
      FdWatcher* fd_watcher = controller->watcher();

      if (event->flags & EV_ONESHOT) {
        // If this was a one-shot event, the Controller needs to stop tracking
        // the descriptor, so it is not double-removed when it is told to stop
        // watching.
        controller->Reset();
        fd_controllers_.Remove(event->udata);
        --event_count_;
      }

      auto scoped_do_work_item = delegate->BeginWorkItem();
      if (event->filter == EVFILT_READ) {
        fd_watcher->OnFileCanReadWithoutBlocking(event->ident);
      } else if (event->filter == EVFILT_WRITE) {
        fd_watcher->OnFileCanWriteWithoutBlocking(event->ident);
      }
    } else if (event->filter == EVFILT_MACHPORT) {
      // March端口事件
      mach_port_t port = KqueueNeedsPortSet() ? event->data : event->ident;

      if (port == wakeup_.get()) {
        // The wakeup event has been received, do not treat this as "doing
        // work", this just wakes up the pump.
        if (KqueueNeedsPortSet()) {
          // When using the kqueue directly, the message can be received
          // straight into a buffer that was created when adding the event.
          // But when using a port set, the message must be drained manually.
          wakeup_buffer_.header.msgh_local_port = port;
          wakeup_buffer_.header.msgh_size = sizeof(wakeup_buffer_);
          // 从目标Mach端口port上读取数据
          kern_return_t kr = mach_msg_receive(&wakeup_buffer_.header);
          MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
              << "mach_msg_receive wakeup";
        }
        continue;
      }

      did_work = true;

      MachPortWatchController* controller = port_controllers_.Lookup(port);
      // The controller could have been removed by some other work callout
      // before this event could be processed.
      if (controller) {
        auto scoped_do_work_item = delegate->BeginWorkItem();
        // 回调到UI层
        controller->watcher()->OnMachMessageReceived(port);
      }
    } else if (event->filter == EVFILT_TIMER) {
      // 定时器事件
      // The wakeup timer fired.
#if DCHECK_IS_ON()
      // On macOS 10.13 and earlier, kqueue timers may spuriously wake up.
      // When this happens, the timer will be re-scheduled the next time
      // DoInternalWork is entered, which means this doesn't lead to a
      // spinning wait.
      // When clock overrides are active, TimeTicks::Now may be decoupled from
      // wall-clock time, and can therefore not be used to validate whether the
      // expected wall-clock time has passed.
      if (!KqueueTimersSpuriouslyWakeUp() &&
          !subtle::ScopedTimeClockOverrides::overrides_active()) {
        // Given the caveats above, assert that the timer didn't fire early.
        DCHECK_LE(scheduled_wakeup_time_, base::TimeTicks::Now());
      }
#endif
      DCHECK_NE(scheduled_wakeup_time_, base::TimeTicks::Max());
      scheduled_wakeup_time_ = base::TimeTicks::Max();
      --event_count_;
    } else {
      NOTREACHED() << "Unexpected event for filter " << event->filter;
    }
  }

  return did_work;
}

void MessagePumpKqueue::MaybeUpdateWakeupTimer(
    const base::TimeTicks& wakeup_time) {
  // Read the state of the suspend flag only once in this function to avoid
  // TOCTTOU problems.
  const bool is_ludicrous_slack_suspended = IsLudicrousTimerSlackSuspended();
  if (wakeup_time == scheduled_wakeup_time_) {
    if (scheduled_wakeup_time_ == base::TimeTicks::Max() ||
        ludicrous_timer_slack_was_suspended_ == is_ludicrous_slack_suspended) {
      // No change in the timer setting necessary.
      return;
    }
  }

  if (ludicrous_timer_slack_was_suspended_ == is_ludicrous_slack_suspended) {
    // If there wasn't a suspension toggle, the wakeup time must have changed.
    DCHECK_NE(wakeup_time, scheduled_wakeup_time_);
  }

  const bool use_slack =
      is_ludicrous_timer_slack_enabled_ && !is_ludicrous_slack_suspended;
  if (wakeup_time == base::TimeTicks::Max()) {
    // If the timer was already reset, don't re-reset it on a suspend toggle.
    if (scheduled_wakeup_time_ != base::TimeTicks::Max()) {
      // Clear the timer.
      kevent64_s timer{};
      SetWakeupTimerEvent(wakeup_time, use_slack, &timer);
      int rv = ChangeOneEvent(kqueue_, &timer);
      PCHECK(rv == 0) << "kevent64, delete timer";
      --event_count_;
    }
  } else {
    // Set/reset the timer.
    kevent64_s timer{};
    SetWakeupTimerEvent(wakeup_time, use_slack, &timer);
    int rv = ChangeOneEvent(kqueue_, &timer);
    PCHECK(rv == 0) << "kevent64, set timer";

    // Bump the event count if we just added the timer.
    if (scheduled_wakeup_time_ == base::TimeTicks::Max())
      ++event_count_;
  }

  ludicrous_timer_slack_was_suspended_ = is_ludicrous_slack_suspended;
  scheduled_wakeup_time_ = wakeup_time;

  // This odd-looking check is here to validate that message pumps aren't
  // constructed before the feature flag is initialized.
  DCHECK_EQ(base::IsLudicrousTimerSlackEnabled(),
            is_ludicrous_timer_slack_enabled_);
}

bool MessagePumpKqueue::IsLudicrousTimerSlackEnabledAndNotSuspended() const {
  return is_ludicrous_timer_slack_enabled_ && !IsLudicrousTimerSlackSuspended();
}

}  // namespace base
