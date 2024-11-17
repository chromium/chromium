// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/message_loop/message_pump_kqueue.h"

#include <sys/errno.h>

#include <atomic>

#include "base/apple/mach_logging.h"
#include "base/apple/scoped_nsautorelease_pool.h"
#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task/task_features.h"
#include "base/time/time_override.h"

namespace base {

namespace {

// Under this feature native work is batched. Remove it once crbug.com/1200141
// is resolved.
BASE_FEATURE(kBatchNativeEventsInMessagePumpKqueue,
             "BatchNativeEventsInMessagePumpKqueue",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Caches the state of the "BatchNativeEventsInMessagePumpKqueue".
std::atomic_bool g_use_batched_version = false;

// Caches the state of the "TimerSlackMac" feature for efficiency.
std::atomic_bool g_timer_slack = false;

#if DCHECK_IS_ON()
// Prior to macOS 10.14, kqueue timers may spuriously wake up, because earlier
// wake ups race with timer resets in the kernel. As of macOS 10.14, updating a
// timer from the thread that reads the kqueue does not cause spurious wakeups.
// Note that updating a kqueue timer from one thread while another thread is
// waiting in a kevent64 invocation is still (inherently) racy.
bool KqueueTimersSpuriouslyWakeUp() {
#if BUILDFLAG(IS_MAC)
  return false;
#else
  // This still happens on iOS15.
  return true;
#endif
}
#endif

int ChangeOneEvent(const ScopedFD& kqueue, kevent64_s* event) {
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

MessagePumpKqueue::MessagePumpKqueue()
    : kqueue_(kqueue()), weak_factory_(this) {
  PCHECK(kqueue_.is_valid()) << "kqueue";

  // Create a Mach port that will be used to wake up the pump by sending
  // a message in response to ScheduleWork(). This is significantly faster than
  // using an EVFILT_USER event, especially when triggered across threads.
  kern_return_t kr = mach_port_allocate(
      mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
      base::apple::ScopedMachReceiveRight::Receiver(wakeup_).get());
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_allocate";

  // Configure the event to directly receive the Mach message as part of the
  // kevent64() call.
  kevent64_s event{};
  event.ident = wakeup_.get();
  event.filter = EVFILT_MACHPORT;
  event.flags = EV_ADD;
  event.fflags = MACH_RCV_MSG;
  event.ext[0] = reinterpret_cast<uint64_t>(&wakeup_buffer_);
  event.ext[1] = sizeof(wakeup_buffer_);

  int rv = ChangeOneEvent(kqueue_, &event);
  PCHECK(rv == 0) << "kevent64";
}

MessagePumpKqueue::~MessagePumpKqueue() = default;

void MessagePumpKqueue::InitializeFeatures() {
  g_use_batched_version.store(
      base::FeatureList::IsEnabled(kBatchNativeEventsInMessagePumpKqueue),
      std::memory_order_relaxed);
  g_timer_slack.store(FeatureList::IsEnabled(kTimerSlackMac),
                      std::memory_order_relaxed);
}

void MessagePumpKqueue::Run(Delegate* delegate) {
  AutoReset<bool> reset_keep_running(&keep_running_, true);

  if (g_use_batched_version.load(std::memory_order_relaxed)) {
    RunBatched(delegate);
  } else {
    while (keep_running_) {
      apple::ScopedNSAutoreleasePool pool;

      bool do_more_work = DoInternalWork(delegate, nullptr);
      if (!keep_running_)
        break;

      Delegate::NextWorkInfo next_work_info = delegate->DoWork();
      do_more_work |= next_work_info.is_immediate();
      if (!keep_running_)
        break;

      if (do_more_work)
        continue;

      delegate->DoIdleWork();
      if (!keep_running_)
        break;

      DoInternalWork(delegate, &next_work_info);
    }
  }
}

void MessagePumpKqueue::RunBatched(Delegate* delegate) {
  // Look for native work once before the loop starts. Without this call the
  // loop would break without checking native work even once in cases where
  // QuitWhenIdle was used. This is sometimes the case in tests.
  DoInternalWork(delegate, nullptr);

  while (keep_running_) {
    apple::ScopedNSAutoreleasePool pool;

    Delegate::NextWorkInfo next_work_info = delegate->DoWork();
    if (!keep_running_)
      break;

    if (!next_work_info.is_immediate()) {
      delegate->DoIdleWork();
    }
    if (!keep_running_)
      break;

    int batch_size = 0;
    if (DoInternalWork(delegate, &next_work_info)) {
      // More than one call can be necessary to fully dispatch all available
      // internal work. Making an effort to dispatch more than the minimum
      // before moving on to application tasks reduces the overhead of going
      // through the whole loop. It also more closely mirrors the behavior of
      // application task execution where tasks are batched. A value of 16 was
      // chosen via local experimentation showing that is was sufficient to
      // dispatch all work in roughly 95% of cases.
      constexpr int kMaxAttempts = 16;
      while (DoInternalWork(delegate, nullptr) && batch_size < kMaxAttempts) {
        ++batch_size;
      }
    }
  }
}

void MessagePumpKqueue::Quit() {
  keep_running_ = false;
  ScheduleWork();
}

void MessagePumpKqueue::ScheduleWork() {
  mach_msg_empty_send_t message{};
  message.header.msgh_size = sizeof(message);
  message.header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_MAKE_SEND_ONCE);
  message.header.msgh_remote_port = wakeup_.get();
  kern_return_t kr = mach_msg_send(&message.header);
  if (kr != KERN_SUCCESS) {
    // If ScheduleWork() is being called by other threads faster than the pump
    // can dispatch work, the kernel message queue for the wakeup port can fill
    // up (this happens under base_perftests, for example). The kernel does
    // return a SEND_ONCE right in the case of failure, which must be destroyed
    // to avoid leaking.
    MACH_DLOG_IF(ERROR, (kr & ~MACH_MSG_IPC_SPACE) != MACH_SEND_NO_BUFFER, kr)
        << "mach_msg_send";
    mach_msg_destroy(&message.header);
  }
}

void MessagePumpKqueue::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  // Nothing to do. This MessagePump uses DoWork().
}

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

  controller->Init(weak_factory_.GetWeakPtr(), port, delegate);
  port_controllers_.AddWithID(controller, port);

  return true;
}

TimeTicks MessagePumpKqueue::AdjustDelayedRunTime(TimeTicks earliest_time,
                                                  TimeTicks run_time,
                                                  TimeTicks latest_time) {
  if (GetAlignWakeUpsEnabled() &&
      g_timer_slack.load(std::memory_order_relaxed)) {
    return earliest_time;
  }
  return MessagePump::AdjustDelayedRunTime(earliest_time, run_time,
                                           latest_time);
}

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

  kevent64_s base_event{};
  base_event.ident = static_cast<uint64_t>(fd);
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

  int rv = HANDLE_EINTR(kevent64(kqueue_.get(), events.data(),
                                 checked_cast<int>(events.size()), nullptr, 0,
                                 0, nullptr));
  if (rv < 0) {
    DPLOG(ERROR) << "WatchFileDescriptor kevent64";
    return false;
  }

  event_count_ += events.size();
  controller->Init(weak_factory_.GetWeakPtr(), fd, mode, delegate);

  return true;
}

void MessagePumpKqueue::SetWakeupTimerEvent(const base::TimeTicks& wakeup_time,
                                            base::TimeDelta leeway,
                                            kevent64_s* timer_event) {
  // The ident of the wakeup timer. There's only the one timer as the pair
  // (ident, filter) is the identity of the event.
  constexpr uint64_t kWakeupTimerIdent = 0x0;
  timer_event->ident = kWakeupTimerIdent;
  timer_event->filter = EVFILT_TIMER;
  if (wakeup_time == base::TimeTicks::Max()) {
    timer_event->flags = EV_DELETE;
  } else {
    timer_event->filter = EVFILT_TIMER;
    // This updates the timer if it already exists in |kqueue_|.
    timer_event->flags = EV_ADD | EV_ONESHOT;

    // Specify the sleep in microseconds to avoid undersleeping due to
    // numeric problems. The sleep is computed from TimeTicks::Now rather than
    // NextWorkInfo::recent_now because recent_now is strictly earlier than
    // current wall-clock. Using an earlier wall clock time  to compute the
    // delta to the next wakeup wall-clock time would guarantee oversleep.
    // If wakeup_time is in the past, the delta below will be negative and the
    // timer is set immediately.
    timer_event->fflags = NOTE_USECONDS;
    timer_event->data = (wakeup_time - base::TimeTicks::Now()).InMicroseconds();

    if (!leeway.is_zero() && g_timer_slack.load(std::memory_order_relaxed)) {
      // Specify slack based on |leeway|.
      // See "man kqueue" in recent macOSen for documentation.
      timer_event->fflags |= NOTE_LEEWAY;
      timer_event->ext[1] = static_cast<uint64_t>(leeway.InMicroseconds());
    }
  }
}

bool MessagePumpKqueue::StopWatchingMachPort(
    MachPortWatchController* controller) {
  mach_port_t port = controller->port();
  controller->Reset();
  port_controllers_.Remove(port);

  kevent64_s event{};
  event.ident = port;
  event.filter = EVFILT_MACHPORT;
  event.flags = EV_DELETE;
  --event_count_;
  int rv = ChangeOneEvent(kqueue_, &event);
  if (rv < 0) {
    DPLOG(ERROR) << "kevent64";
    return false;
  }

  return true;
}

bool MessagePumpKqueue::StopWatchingFileDescriptor(
    FdWatchController* controller) {
  int fd = controller->fd();
  int mode = controller->mode();
  controller->Reset();

  if (fd < 0)
    return true;

  std::vector<kevent64_s> events;

  kevent64_s base_event{};
  base_event.ident = static_cast<uint64_t>(fd);
  base_event.flags = EV_DELETE;

  if (mode & Mode::WATCH_READ) {
    base_event.filter = EVFILT_READ;
    events.push_back(base_event);
  }
  if (mode & Mode::WATCH_WRITE) {
    base_event.filter = EVFILT_WRITE;
    events.push_back(base_event);
  }

  int rv = HANDLE_EINTR(kevent64(kqueue_.get(), events.data(),
                                 checked_cast<int>(events.size()), nullptr, 0,
                                 0, nullptr));
  DPLOG_IF(ERROR, rv < 0) << "StopWatchingFileDescriptor kevent64";

  // The keys for the IDMap aren't recorded anywhere (they're attached to the
  // kevent object in the kernel), so locate the entries by controller pointer.
  for (IDMap<FdWatchController*, uint64_t>::iterator it(&fd_controllers_);
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
  unsigned int flags = immediate ? KEVENT_FLAG_IMMEDIATE : 0;

  if (!immediate) {
    MaybeUpdateWakeupTimer(next_work_info->delayed_run_time,
                           next_work_info->leeway);
    DCHECK_EQ(scheduled_wakeup_time_, next_work_info->delayed_run_time);
    delegate->BeforeWait();
  }

  int rv =
      HANDLE_EINTR(kevent64(kqueue_.get(), nullptr, 0, events_.data(),
                            checked_cast<int>(events_.size()), flags, nullptr));
  if (rv == 0) {
    // No events to dispatch so no need to call ProcessEvents().
    return false;
  }

  PCHECK(rv > 0) << "kevent64";
  return ProcessEvents(delegate, static_cast<size_t>(rv));
}

bool MessagePumpKqueue::ProcessEvents(Delegate* delegate, size_t count) {
  bool did_work = false;

  delegate->BeginNativeWorkBeforeDoWork();
  for (size_t i = 0; i < count; ++i) {
    auto* event = &events_[i];
    if (event->filter == EVFILT_READ || event->filter == EVFILT_WRITE) {
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
      // WatchFileDescriptor() originally upcasts event->ident from an int.
      if (event->filter == EVFILT_READ) {
        fd_watcher->OnFileCanReadWithoutBlocking(
            static_cast<int>(event->ident));
      } else if (event->filter == EVFILT_WRITE) {
        fd_watcher->OnFileCanWriteWithoutBlocking(
            static_cast<int>(event->ident));
      }
    } else if (event->filter == EVFILT_MACHPORT) {
      // WatchMachReceivePort() originally sets event->ident from a mach_port_t.
      mach_port_t port = static_cast<mach_port_t>(event->ident);
      if (port == wakeup_.get()) {
        // The wakeup event has been received, do not treat this as "doing
        // work", this just wakes up the pump.
        continue;
      }

      did_work = true;

      MachPortWatchController* controller = port_controllers_.Lookup(port);
      // The controller could have been removed by some other work callout
      // before this event could be processed.
      if (controller) {
        auto scoped_do_work_item = delegate->BeginWorkItem();
        controller->watcher()->OnMachMessageReceived(port);
      }
    } else if (event->filter == EVFILT_TIMER) {
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
    const base::TimeTicks& wakeup_time,
    base::TimeDelta leeway) {
  if (wakeup_time == scheduled_wakeup_time_) {
    // No change in the timer setting necessary.
    return;
  }

  if (wakeup_time == base::TimeTicks::Max()) {
    // If the timer was already reset, don't re-reset it on a suspend toggle.
    if (scheduled_wakeup_time_ != base::TimeTicks::Max()) {
      // Clear the timer.
      kevent64_s timer{};
      SetWakeupTimerEvent(wakeup_time, leeway, &timer);
      int rv = ChangeOneEvent(kqueue_, &timer);
      PCHECK(rv == 0) << "kevent64, delete timer";
      --event_count_;
    }
  } else {
    // Set/reset the timer.
    kevent64_s timer{};
    SetWakeupTimerEvent(wakeup_time, leeway, &timer);
    int rv = ChangeOneEvent(kqueue_, &timer);
    PCHECK(rv == 0) << "kevent64, set timer";

    // Bump the event count if we just added the timer.
    if (scheduled_wakeup_time_ == base::TimeTicks::Max())
      ++event_count_;
  }

  scheduled_wakeup_time_ = wakeup_time;
}

}  // namespace base
