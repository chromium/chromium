// Copyright 2012 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "third_party/libevent/event.h"

#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
#include "base/message_loop/message_pump_epoll.h"
#endif

// Lifecycle of struct event
// Libevent uses two main data structures:
// struct event_base (of which there is one per message pump), and
// struct event (of which there is roughly one per socket).
// The socket's struct event is created in
// MessagePumpLibevent::WatchFileDescriptor(),
// is owned by the FdWatchController, and is destroyed in
// StopWatchingFileDescriptor().
// It is moved into and out of lists in struct event_base by
// the libevent functions event_add() and event_del().

namespace base {

#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
namespace {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
bool g_use_epoll = true;
#else
// TODO(crbug.com/1243354): Enable by default on chromeos.
bool g_use_epoll = false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
}  // namespace

BASE_FEATURE(kMessagePumpEpoll, "MessagePumpEpoll", FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)

MessagePumpLibevent::FdWatchController::FdWatchController(
    const Location& from_here)
    : FdWatchControllerInterface(from_here) {}

MessagePumpLibevent::FdWatchController::~FdWatchController() {
  CHECK(StopWatchingFileDescriptor());
  if (was_destroyed_) {
    DCHECK(!*was_destroyed_);
    *was_destroyed_ = true;
  }
}

bool MessagePumpLibevent::FdWatchController::StopWatchingFileDescriptor() {
  watcher_ = nullptr;

  std::unique_ptr<event> e = ReleaseEvent();
  if (e) {
    // event_del() is a no-op if the event isn't active.
    int rv = event_del(e.get());
    libevent_pump_ = nullptr;
    return (rv == 0);
  }

#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  if (epoll_interest_ && epoll_pump_) {
    epoll_pump_->UnregisterInterest(epoll_interest_);
    epoll_interest_.reset();
    epoll_pump_.reset();
  }
#endif

  return true;
}

void MessagePumpLibevent::FdWatchController::Init(std::unique_ptr<event> e) {
  DCHECK(e);
  DCHECK(!event_);

  event_ = std::move(e);
}

std::unique_ptr<event> MessagePumpLibevent::FdWatchController::ReleaseEvent() {
  return std::move(event_);
}

void MessagePumpLibevent::FdWatchController::OnFileCanReadWithoutBlocking(
    int fd,
    MessagePumpLibevent* pump) {
  // Since OnFileCanWriteWithoutBlocking() gets called first, it can stop
  // watching the file descriptor.
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

const scoped_refptr<MessagePumpLibevent::EpollInterest>&
MessagePumpLibevent::FdWatchController::AssignEpollInterest(
    const EpollInterestParams& params) {
  epoll_interest_ = MakeRefCounted<EpollInterest>(this, params);
  return epoll_interest_;
}

void MessagePumpLibevent::FdWatchController::OnFdReadable() {
  if (!watcher_) {
    // When a watcher is watching both read and write and both are possible, the
    // pump will call OnFdWritable() first, followed by OnFdReadable(). But
    // OnFdWritable() may stop or destroy the watch. If the watch is destroyed,
    // the pump will not call OnFdReadable() at all, but if it's merely stopped,
    // OnFdReadable() will be called while `watcher_` is  null. In this case we
    // don't actually want to call the client.
    return;
  }
  watcher_->OnFileCanReadWithoutBlocking(epoll_interest_->params().fd);
}

void MessagePumpLibevent::FdWatchController::OnFdWritable() {
  DCHECK(watcher_);
  watcher_->OnFileCanWriteWithoutBlocking(epoll_interest_->params().fd);
}

MessagePumpLibevent::MessagePumpLibevent() {
#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  if (g_use_epoll) {
    epoll_pump_ = std::make_unique<MessagePumpEpoll>();
    return;
  }
#endif

  if (!Init())
    NOTREACHED();
  DCHECK_NE(wakeup_pipe_in_, -1);
  DCHECK_NE(wakeup_pipe_out_, -1);
  DCHECK(wakeup_event_);
}

MessagePumpLibevent::~MessagePumpLibevent() {
#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  const bool using_libevent = !epoll_pump_;
#else
  const bool using_libevent = true;
#endif

  DCHECK(event_base_);
  if (using_libevent) {
    DCHECK(wakeup_event_);
    event_del(wakeup_event_.get());
    wakeup_event_.reset();
    if (wakeup_pipe_in_ >= 0) {
      if (IGNORE_EINTR(close(wakeup_pipe_in_)) < 0)
        DPLOG(ERROR) << "close";
    }
    if (wakeup_pipe_out_ >= 0) {
      if (IGNORE_EINTR(close(wakeup_pipe_out_)) < 0)
        DPLOG(ERROR) << "close";
    }
  }
  event_base_.reset();
}

// Must be called early in process startup, but after FeatureList
// initialization. This allows MessagePumpLibevent to query and cache the
// enabled state of any relevant features.
// static
void MessagePumpLibevent::InitializeFeatures() {
#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  g_use_epoll = FeatureList::IsEnabled(kMessagePumpEpoll);
#endif
}

bool MessagePumpLibevent::WatchFileDescriptor(int fd,
                                              bool persistent,
                                              int mode,
                                              FdWatchController* controller,
                                              FdWatcher* delegate) {
#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  if (epoll_pump_) {
    return epoll_pump_->WatchFileDescriptor(fd, persistent, mode, controller,
                                            delegate);
  }
#endif

  TRACE_EVENT("base", "MessagePumpLibevent::WatchFileDescriptor", "fd", fd,
              "persistent", persistent, "watch_read", mode & WATCH_READ,
              "watch_write", mode & WATCH_WRITE);
  DCHECK_GE(fd, 0);
  DCHECK(controller);
  DCHECK(delegate);
  DCHECK(mode == WATCH_READ || mode == WATCH_WRITE || mode == WATCH_READ_WRITE);
  // WatchFileDescriptor should be called on the pump thread. It is not
  // threadsafe, and your watcher may never be registered.
  DCHECK(watch_file_descriptor_caller_checker_.CalledOnValidThread());

  short event_mask = persistent ? EV_PERSIST : 0;
  if (mode & WATCH_READ) {
    event_mask |= EV_READ;
  }
  if (mode & WATCH_WRITE) {
    event_mask |= EV_WRITE;
  }

  std::unique_ptr<event> evt(controller->ReleaseEvent());
  if (!evt) {
    // Ownership is transferred to the controller.
    evt = std::make_unique<event>();
  } else {
    // Make sure we don't pick up any funky internal libevent masks.
    int old_interest_mask = evt->ev_events & (EV_READ | EV_WRITE | EV_PERSIST);

    // Combine old/new event masks.
    event_mask |= old_interest_mask;

    // Must disarm the event before we can reuse it.
    event_del(evt.get());

    // It's illegal to use this function to listen on 2 separate fds with the
    // same |controller|.
    if (EVENT_FD(evt.get()) != fd) {
      NOTREACHED() << "FDs don't match" << EVENT_FD(evt.get()) << "!=" << fd;
      return false;
    }
  }

  // Set current interest mask and message pump for this event.
  event_set(evt.get(), fd, event_mask, OnLibeventNotification, controller);

  // Tell libevent which message pump this socket will belong to when we add it.
  if (event_base_set(event_base_.get(), evt.get())) {
    DPLOG(ERROR) << "event_base_set(fd=" << EVENT_FD(evt.get()) << ")";
    return false;
  }

  // Add this socket to the list of monitored sockets.
  if (event_add(evt.get(), nullptr)) {
    DPLOG(ERROR) << "event_add failed(fd=" << EVENT_FD(evt.get()) << ")";
    return false;
  }

  controller->Init(std::move(evt));
  controller->set_watcher(delegate);
  controller->set_libevent_pump(this);
  return true;
}

// Tell libevent to break out of inner loop.
static void timer_callback(int fd, short events, void* context) {
  event_base_loopbreak((struct event_base*)context);
}

// Reentrant!
void MessagePumpLibevent::Run(Delegate* delegate) {
#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  if (epoll_pump_) {
    epoll_pump_->Run(delegate);
    return;
  }
#endif

  RunState run_state(delegate);
  AutoReset<raw_ptr<RunState>> auto_reset_run_state(&run_state_, &run_state);

  // event_base_loopexit() + EVLOOP_ONCE is leaky, see http://crbug.com/25641.
  // Instead, make our own timer and reuse it on each call to event_base_loop().
  std::unique_ptr<event> timer_event(new event);

  for (;;) {
    // Do some work and see if the next task is ready right away.
    Delegate::NextWorkInfo next_work_info = delegate->DoWork();
    bool immediate_work_available = next_work_info.is_immediate();

    if (run_state.should_quit)
      break;

    // Process native events if any are ready. Do not block waiting for more. Do
    // not instantiate a ScopedDoWorkItem for this call as:
    //  - This most often ends up calling OnLibeventNotification() below which
    //    already instantiates a ScopedDoWorkItem (and doing so twice would
    //    incorrectly appear as nested work).
    //  - "ThreadController active" is already up per the above DoWork() so this
    //    would only be about detecting #work-in-work-implies-nested
    //    (ref. thread_controller.h).
    //  - This can result in the same work as the
    //    event_base_loop(event_base_, EVLOOP_ONCE) call at the end of this
    //    method and that call definitely can't be in a ScopedDoWorkItem as
    //    it includes sleep.
    //  - The only downside is that, if a native work item other than
    //    OnLibeventNotification() did enter a nested loop from here, it
    //    wouldn't be labeled as such in tracing by "ThreadController active".
    //    Contact gab@/scheduler-dev@ if a problematic trace emerges.
    event_base_loop(event_base_.get(), EVLOOP_NONBLOCK);

    bool attempt_more_work = immediate_work_available || processed_io_events_;
    processed_io_events_ = false;

    if (run_state.should_quit)
      break;

    if (attempt_more_work)
      continue;

    attempt_more_work = delegate->DoIdleWork();

    if (run_state.should_quit)
      break;

    if (attempt_more_work)
      continue;

    bool did_set_timer = false;

    // If there is delayed work.
    DCHECK(!next_work_info.delayed_run_time.is_null());
    if (!next_work_info.delayed_run_time.is_max()) {
      const TimeDelta delay = next_work_info.remaining_delay();

      // Setup a timer to break out of the event loop at the right time.
      struct timeval poll_tv;
      poll_tv.tv_sec = static_cast<time_t>(delay.InSeconds());
      poll_tv.tv_usec = delay.InMicroseconds() % Time::kMicrosecondsPerSecond;
      event_set(timer_event.get(), -1, 0, timer_callback, event_base_.get());
      event_base_set(event_base_.get(), timer_event.get());
      event_add(timer_event.get(), &poll_tv);

      did_set_timer = true;
    }

    // Block waiting for events and process all available upon waking up. This
    // is conditionally interrupted to look for more work if we are aware of a
    // delayed task that will need servicing.
    delegate->BeforeWait();
    event_base_loop(event_base_.get(), EVLOOP_ONCE);

    // We previously setup a timer to break out the event loop to look for more
    // work. Now that we're here delete the event.
    if (did_set_timer) {
      event_del(timer_event.get());
    }

    if (run_state.should_quit)
      break;
  }
}

void MessagePumpLibevent::Quit() {
#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  if (epoll_pump_) {
    epoll_pump_->Quit();
    return;
  }
#endif

  DCHECK(run_state_) << "Quit was called outside of Run!";
  // Tell both libevent and Run that they should break out of their loops.
  run_state_->should_quit = true;
  ScheduleWork();
}

void MessagePumpLibevent::ScheduleWork() {
#if BUILDFLAG(ENABLE_MESSAGE_PUMP_EPOLL)
  if (epoll_pump_) {
    epoll_pump_->ScheduleWork();
    return;
  }
#endif

  // Tell libevent (in a threadsafe way) that it should break out of its loop.
  char buf = 0;
  long nwrite = HANDLE_EINTR(write(wakeup_pipe_in_, &buf, 1));
  DPCHECK(nwrite == 1 || errno == EAGAIN) << "nwrite:" << nwrite;
}

void MessagePumpLibevent::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  // When using libevent we know that we can't be blocked on Run()'s
  // `timer_event` right now since this method can only be called on the same
  // thread as Run(). When using epoll, the pump clearly must be in between
  // waits if we're here. In either case, any scheduled work will be seen prior
  // to the next libevent loop or epoll wait, so there's nothing to do here.
}

bool MessagePumpLibevent::Init() {
  int fds[2];
  if (!CreateLocalNonBlockingPipe(fds)) {
    DPLOG(ERROR) << "pipe creation failed";
    return false;
  }
  wakeup_pipe_out_ = fds[0];
  wakeup_pipe_in_ = fds[1];

  wakeup_event_ = std::make_unique<event>();
  event_set(wakeup_event_.get(), wakeup_pipe_out_, EV_READ | EV_PERSIST,
            OnWakeup, this);
  event_base_set(event_base_.get(), wakeup_event_.get());

  if (event_add(wakeup_event_.get(), nullptr))
    return false;
  return true;
}

// static
void MessagePumpLibevent::OnLibeventNotification(int fd,
                                                 short flags,
                                                 void* context) {
  FdWatchController* controller = static_cast<FdWatchController*>(context);
  DCHECK(controller);

  MessagePumpLibevent* pump = controller->libevent_pump();
  pump->processed_io_events_ = true;

  // Make the MessagePumpDelegate aware of this other form of "DoWork". Skip if
  // OnLibeventNotification is called outside of Run() (e.g. in unit tests).
  Delegate::ScopedDoWorkItem scoped_do_work_item;
  if (pump->run_state_)
    scoped_do_work_item = pump->run_state_->delegate->BeginWorkItem();

  // Trace events must begin after the above BeginWorkItem() so that the
  // ensuing "ThreadController active" outscopes all the events under it.
  TRACE_EVENT("toplevel", "OnLibevent", "controller_created_from",
              controller->created_from_location(), "fd", fd, "flags", flags,
              "context", context);
  TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION heap_profiler_scope(
      controller->created_from_location().file_name());

  if ((flags & (EV_READ | EV_WRITE)) == (EV_READ | EV_WRITE)) {
    // Both callbacks will be called. It is necessary to check that |controller|
    // is not destroyed.
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

// Called if a byte is received on the wakeup pipe.
// static
void MessagePumpLibevent::OnWakeup(int socket, short flags, void* context) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("base"),
              "MessagePumpLibevent::OnWakeup", "socket", socket, "flags", flags,
              "context", context);
  MessagePumpLibevent* that = static_cast<MessagePumpLibevent*>(context);
  DCHECK(that->wakeup_pipe_out_ == socket);

  // Remove and discard the wakeup byte.
  char buf;
  long nread = HANDLE_EINTR(read(socket, &buf, 1));
  DCHECK_EQ(nread, 1);
  that->processed_io_events_ = true;
  // Tell libevent to break out of inner loop.
  event_base_loopbreak(that->event_base_.get());
}

MessagePumpLibevent::EpollInterest::EpollInterest(
    FdWatchController* controller,
    const EpollInterestParams& params)
    : controller_(controller), params_(params) {}

MessagePumpLibevent::EpollInterest::~EpollInterest() = default;

}  // namespace base
