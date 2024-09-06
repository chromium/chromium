// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_epoll.h"

#include <sys/eventfd.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/ranges/algorithm.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"

#if DCHECK_IS_ON()
#include <iomanip>
#endif

namespace base {

namespace {

// Under this feature native work is batched.
BASE_FEATURE(kBatchNativeEventsInMessagePumpEpoll,
             "BatchNativeEventsInMessagePumpEpoll",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Caches the state of the "BatchNativeEventsInMessagePumpEpoll".
std::atomic_bool g_use_batched_version = false;
std::atomic_bool g_use_poll = false;

constexpr std::pair<uint32_t, short int> kEpollToPollEvents[] = {
    {EPOLLIN, POLLIN},   {EPOLLOUT, POLLOUT}, {EPOLLRDHUP, POLLRDHUP},
    {EPOLLPRI, POLLPRI}, {EPOLLERR, POLLERR}, {EPOLLHUP, POLLHUP}};

void SetEventsForPoll(const uint32_t epoll_events, struct pollfd* poll_entry) {
  poll_entry->events = 0;
  for (const auto& epoll_poll : kEpollToPollEvents) {
    if (epoll_events & epoll_poll.first) {
      poll_entry->events |= epoll_poll.second;
    }
  }
}
}  // namespace

// Parameters used to construct and describe an interest.
struct MessagePumpEpoll::InterestParams {
  // The file descriptor of interest.
  int fd;

  // Indicates an interest in being able to read() from `fd`.
  bool read;

  // Indicates an interest in being able to write() to `fd`.
  bool write;

  // Indicates whether this interest is a one-shot interest, meaning that it
  // must be automatically deactivated every time it triggers an epoll event.
  bool one_shot;

  bool IsEqual(const InterestParams& rhs) const {
    return std::tie(fd, read, write, one_shot) ==
           std::tie(rhs.fd, rhs.read, rhs.write, rhs.one_shot);
  }
};

// Represents a single controller's interest in a file descriptor via epoll,
// and tracks whether that interest is currently active. Though an interest
// persists as long as its controller is alive and hasn't changed interests,
// it only participates in epoll waits while active.
class MessagePumpEpoll::Interest : public RefCounted<Interest> {
 public:
  Interest(FdWatchController* controller, const InterestParams& params)
      : controller_(controller), params_(params) {}

  Interest(const Interest&) = delete;
  Interest& operator=(const Interest&) = delete;

  FdWatchController* controller() { return controller_; }
  const InterestParams& params() const { return params_; }

  bool active() const { return active_; }
  void set_active(bool active) { active_ = active; }

  // Only meaningful between WatchForControllerDestruction() and
  // StopWatchingForControllerDestruction().
  bool was_controller_destroyed() const { return was_controller_destroyed_; }

  void WatchForControllerDestruction() {
    DCHECK_GE(nested_controller_destruction_watchers_, 0);
    if (nested_controller_destruction_watchers_ == 0) {
      DCHECK(!controller_->was_destroyed_);
      controller_->was_destroyed_ = &was_controller_destroyed_;
    } else {
      // If this is a nested event we should already be watching `controller_`
      // for destruction from an outer event handler.
      DCHECK_EQ(controller_->was_destroyed_, &was_controller_destroyed_);
    }
    ++nested_controller_destruction_watchers_;
  }

  void StopWatchingForControllerDestruction() {
    --nested_controller_destruction_watchers_;
    DCHECK_GE(nested_controller_destruction_watchers_, 0);
    if (nested_controller_destruction_watchers_ == 0 &&
        !was_controller_destroyed_) {
      DCHECK_EQ(controller_->was_destroyed_, &was_controller_destroyed_);
      controller_->was_destroyed_ = nullptr;
    }
  }

 private:
  friend class RefCounted<Interest>;
  ~Interest() = default;

  const raw_ptr<FdWatchController, DanglingUntriaged> controller_;
  const InterestParams params_;
  bool active_ = true;
  bool was_controller_destroyed_ = false;

  // Avoid resetting `controller_->was_destroyed` when nested destruction
  // watchers are active.
  int nested_controller_destruction_watchers_ = 0;
};

MessagePumpEpoll::MessagePumpEpoll() {
  epoll_.reset(epoll_create1(/*flags=*/0));
  PCHECK(epoll_.is_valid());

  wake_event_.reset(eventfd(0, EFD_NONBLOCK));
  PCHECK(wake_event_.is_valid());

  epoll_event wake{.events = EPOLLIN, .data = {.ptr = &wake_event_}};
  int rv = epoll_ctl(epoll_.get(), EPOLL_CTL_ADD, wake_event_.get(), &wake);
  PCHECK(rv == 0);

  struct pollfd poll_entry;
  poll_entry.fd = wake_event_.get();
  poll_entry.events = POLLIN;
  poll_entry.revents = 0;
  pollfds_.push_back(poll_entry);

  next_metrics_time_ = base::TimeTicks::Now() + base::Minutes(1);
}

MessagePumpEpoll::~MessagePumpEpoll() = default;

void MessagePumpEpoll::InitializeFeatures() {
  // Relaxed memory order since no memory access depends on value.
  g_use_batched_version.store(
      base::FeatureList::IsEnabled(kBatchNativeEventsInMessagePumpEpoll),
      std::memory_order_relaxed);
  g_use_poll.store(base::FeatureList::IsEnabled(kUsePollForMessagePumpEpoll),
                   std::memory_order_relaxed);
}

bool MessagePumpEpoll::WatchFileDescriptor(int fd,
                                           bool persistent,
                                           int mode,
                                           FdWatchController* controller,
                                           FdWatcher* watcher) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT("base", "MessagePumpEpoll::WatchFileDescriptor", "fd", fd,
              "persistent", persistent, "watch_read", mode & WATCH_READ,
              "watch_write", mode & WATCH_WRITE);

  const InterestParams params{
      .fd = fd,
      .read = (mode == WATCH_READ || mode == WATCH_READ_WRITE),
      .write = (mode == WATCH_WRITE || mode == WATCH_READ_WRITE),
      .one_shot = !persistent,
  };

  auto [it, is_new_fd_entry] = entries_.emplace(fd, fd);
  EpollEventEntry& entry = it->second;
  scoped_refptr<Interest> existing_interest = controller->interest();
  if (existing_interest && existing_interest->params().IsEqual(params)) {
    // WatchFileDescriptor() has already been called for this controller at
    // least once before, and as in the most common cases, it is now being
    // called again with the same parameters.
    //
    // We don't need to allocate and register a new Interest in this case, but
    // we can instead reactivate the existing (presumably deactivated,
    // non-persistent) Interest.
    existing_interest->set_active(true);
  } else {
    entry.interests.push_back(controller->AssignInterest(params));
    if (existing_interest) {
      UnregisterInterest(existing_interest);
    }
  }

  if (is_new_fd_entry) {
    AddEpollEvent(entry);
  } else {
    UpdateEpollEvent(entry);
  }

  controller->set_pump(weak_ptr_factory_.GetWeakPtr());
  controller->set_watcher(watcher);
  return true;
}

void MessagePumpEpoll::Run(Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RunState run_state(delegate);
  AutoReset<raw_ptr<RunState>> auto_reset_run_state(&run_state_, &run_state);
  for (;;) {
    // Do some work and see if the next task is ready right away.
    Delegate::NextWorkInfo next_work_info = delegate->DoWork();
    const bool immediate_work_available = next_work_info.is_immediate();
    if (run_state.should_quit) {
      break;
    }

    if (next_work_info.recent_now > next_metrics_time_) {
      RecordPeriodicMetrics();
    }

    // Reset the native work flag before processing IO events.
    native_work_started_ = false;

    // Process any immediately ready IO event, but don't sleep yet.
    // Process epoll events until none is available without blocking or
    // the maximum number of iterations is reached. The maximum number of
    // iterations when `g_use_batched_version` is true was chosen so that
    // all available events are dispatched 95% of the time in local tests.
    // The maximum is not infinite because we want to yield to application
    // tasks at some point.
    bool did_native_work = false;
    const int max_iterations =
        g_use_batched_version.load(std::memory_order_relaxed) ? 16 : 1;
    for (int i = 0; i < max_iterations; ++i) {
      if (!WaitForEpollEvents(TimeDelta())) {
        break;
      }
      did_native_work = true;
    }

    bool attempt_more_work = immediate_work_available || did_native_work;

    if (run_state.should_quit) {
      break;
    }
    if (attempt_more_work) {
      continue;
    }

    delegate->DoIdleWork();
    if (run_state.should_quit) {
      break;
    }

    TimeDelta next_metrics_delay =
        next_metrics_time_ - next_work_info.recent_now;
    TimeDelta timeout = TimeDelta::Max();
    DCHECK(!next_work_info.delayed_run_time.is_null());
    if (!next_work_info.delayed_run_time.is_max()) {
      timeout = next_work_info.remaining_delay();
    }
    if (timeout > next_metrics_delay) {
      timeout = next_metrics_delay;
      // Ensure we never get a negative timeout from the next_metrics_delay as
      // this will cause epoll to block indefinitely if no fds are signaled,
      // preventing existing non-fd tasks from running.
      if (timeout < base::Milliseconds(0)) {
        timeout = base::Milliseconds(0);
      }
    }
    delegate->BeforeWait();
    WaitForEpollEvents(timeout);
    if (run_state.should_quit) {
      break;
    }
  }
}

void MessagePumpEpoll::RecordPeriodicMetrics() {
  UMA_HISTOGRAM_COUNTS_1000("MessagePumpEpoll.WatchedFileDescriptors",
                            (int)entries_.size());
  next_metrics_time_ += base::Minutes(1);
}

void MessagePumpEpoll::Quit() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(run_state_) << "Quit() called outside of Run()";
  run_state_->should_quit = true;
}

void MessagePumpEpoll::ScheduleWork() {
  const uint64_t value = 1;
  ssize_t n = HANDLE_EINTR(write(wake_event_.get(), &value, sizeof(value)));

  // EAGAIN here implies that the write() would overflow of the event counter,
  // which is a condition we can safely ignore. It implies that the event
  // counter is non-zero and therefore readable, which is enough to ensure that
  // any pending wait eventually wakes up.
  DPCHECK(n == sizeof(value) || errno == EAGAIN);
}

void MessagePumpEpoll::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  // Nothing to do. This can only be called from the same thread as Run(), so
  // the pump must be in between waits. The scheduled work therefore will be
  // seen in time for the next wait.
}

void MessagePumpEpoll::AddEpollEvent(EpollEventEntry& entry) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!entry.stopped);
  const uint32_t events = entry.ComputeActiveEvents();
  epoll_event event{.events = events, .data = {.ptr = &entry}};
  int rv = epoll_ctl(epoll_.get(), EPOLL_CTL_ADD, entry.fd, &event);
#if DCHECK_IS_ON()
  // TODO(361611793): Remove these debug logs after resolving the issue.
  if (rv != 0) {
    for (auto& history : entry.epoll_history_) {
      if (history.event) {
        auto& e = history.event.value();
        LOG(ERROR) << "events=0x" << std::hex << std::setfill('0')
                   << std::setw(8) << e.events;
        LOG(ERROR) << "data=0x" << std::hex << std::setfill('0')
                   << std::setw(16) << e.data.u64;
      }
      LOG(ERROR) << history.stack_trace;
    }
  } else {
    entry.PushEpollHistory(std::make_optional(event));
  }
#endif
  DPCHECK(rv == 0);
  entry.registered_events = events;

  DCHECK(FindPollEntry(entry.fd) == pollfds_.end());
  struct pollfd poll_entry;
  poll_entry.fd = entry.fd;
  poll_entry.revents = 0;
  SetEventsForPoll(events, &poll_entry);

  pollfds_.push_back(poll_entry);
}

void MessagePumpEpoll::UpdateEpollEvent(EpollEventEntry& entry) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const uint32_t events = entry.ComputeActiveEvents();
  if (!entry.stopped) {
    if (events == 0) {
      // There is no active interest now.
      // We don't have to call epoll_ctl() if the last event was registered as
      // one-shot since the fd has already been disabled.
      if (!(entry.registered_events & EPOLLONESHOT)) {
        // The fd is still enabled. We need to disable it but don't remove the
        // entry from `entries_` to keep the reference alive because handling
        // the entry isn't finished yet.
        StopEpollEvent(entry);
      } else {
        // No work needs to be done for epoll, but for poll we have to implement
        // the equivalent of oneshot ourselves by unregistering for all events.
        auto poll_entry = FindPollEntry(entry.fd);
        CHECK(poll_entry != pollfds_.end());
        poll_entry->events = 0;
      }
      return;
    }
    if (events == entry.registered_events && !(events & EPOLLONESHOT)) {
      // Persistent events don't need to be modified if no bits are changing.
      return;
    }
    epoll_event event{.events = events, .data = {.ptr = &entry}};
    int rv = epoll_ctl(epoll_.get(), EPOLL_CTL_MOD, entry.fd, &event);
    DPCHECK(rv == 0);
#if DCHECK_IS_ON()
    entry.PushEpollHistory(std::make_optional(event));
#endif
    entry.registered_events = events;

    auto poll_entry = FindPollEntry(entry.fd);
    CHECK(poll_entry != pollfds_.end());
    SetEventsForPoll(events, &(*poll_entry));
  } else if (events != 0) {
    // An interest for the fd has been reactivated. Re-enable the fd.
    entry.stopped = false;
    AddEpollEvent(entry);
  }
}

void MessagePumpEpoll::StopEpollEvent(EpollEventEntry& entry) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!entry.stopped) {
    int rv = epoll_ctl(epoll_.get(), EPOLL_CTL_DEL, entry.fd, nullptr);
    DPCHECK(rv == 0);
#if DCHECK_IS_ON()
    entry.PushEpollHistory(std::nullopt);
#endif
    entry.stopped = true;
    entry.registered_events = 0;
    RemovePollEntry(entry.fd);
  }
}

void MessagePumpEpoll::UnregisterInterest(
    const scoped_refptr<Interest>& interest) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const int fd = interest->params().fd;
  auto entry_it = entries_.find(fd);
  CHECK(entry_it != entries_.end(), base::NotFatalUntil::M125);

  EpollEventEntry& entry = entry_it->second;
  auto& interests = entry.interests;
  auto* it = ranges::find(interests, interest);
  CHECK(it != interests.end(), base::NotFatalUntil::M125);
  interests.erase(it);

  if (interests.empty()) {
    StopEpollEvent(entry);
    entries_.erase(entry_it);
  } else {
    UpdateEpollEvent(entry);
  }
}

bool MessagePumpEpoll::WaitForEpollEvents(TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // `timeout` has microsecond resolution, but timeouts accepted by epoll_wait()
  // are integral milliseconds. Round up to the next millisecond.
  // TODO(crbug.com/40245876): Consider higher-resolution timeouts.
  const int epoll_timeout =
      timeout.is_max() ? -1
                       : saturated_cast<int>(timeout.InMillisecondsRoundedUp());

  // Used in the "epoll" code path.
  epoll_event epoll_events[16];
  // Used in the "poll" code path.
  std::vector<epoll_event> poll_events;
  // Will refer to `events` or `events_vector` depending on which
  // code path is taken.
  span<epoll_event> ready_events;

  // When there are many FDs, epoll() can be significantly faster as poll needs
  // to iterate through the list of watched fds. This value is pretty arbitrary,
  // the internet suggests that under 1000 fds that epoll isn't noticeably
  // faster than poll but this isn't easy to empirically measure.
  bool use_poll =
      g_use_poll.load(std::memory_order_relaxed) && entries_.size() < 500;

  if (use_poll) {
    if (!GetEventsPoll(epoll_timeout, &poll_events)) {
      return false;
    }
    ready_events = span(poll_events).first(poll_events.size());
  } else {
    const int epoll_result = epoll_wait(epoll_.get(), epoll_events,
                                        std::size(epoll_events), epoll_timeout);
    if (epoll_result < 0) {
      DPCHECK(errno == EINTR);
      return false;
    }
    if (epoll_result == 0) {
      return false;
    }

    ready_events =
        span(epoll_events).first(base::checked_cast<size_t>(epoll_result));
  }

  for (epoll_event& e : ready_events) {
    if (e.data.ptr == &wake_event_) {
      // Wake-up events are always safe to handle immediately. Unlike other
      // events used by MessagePumpEpoll they also don't point to an
      // EpollEventEntry, so we handle them separately here.
      HandleWakeUp();
      e.data.ptr = nullptr;
      continue;
    }

    // To guard against one of the ready events unregistering and thus
    // invalidating one of the others here, first link each entry to the
    // corresponding epoll_event returned by epoll_wait(). We do this before
    // dispatching any events, and the second pass below will only dispatch an
    // event if its epoll_event data is still valid.
    auto& entry = EpollEventEntry::FromEpollEvent(e);
    DCHECK(!entry.active_event);
    EpollEventEntry::FromEpollEvent(e).active_event = &e;
  }

  for (auto& e : ready_events) {
    if (e.data.ptr) {
      auto& entry = EpollEventEntry::FromEpollEvent(e);
      entry.active_event = nullptr;
      OnEpollEvent(entry, e.events);
    }
  }

  return true;
}

std::vector<struct pollfd>::iterator MessagePumpEpoll::FindPollEntry(int fd) {
  return std::find_if(
      pollfds_.begin(), pollfds_.end(),
      [fd](const struct pollfd poll_entry) { return poll_entry.fd == fd; });
}

void MessagePumpEpoll::RemovePollEntry(int fd) {
  pollfds_.erase(FindPollEntry(fd));
}

bool MessagePumpEpoll::GetEventsPoll(int epoll_timeout,
                                     std::vector<epoll_event>* epoll_events) {
  int retval = poll(&pollfds_[0], base::checked_cast<nfds_t>(pollfds_.size()),
                    epoll_timeout);
  if (retval < 0) {
    DPCHECK(errno == EINTR);
    return false;
  }
  // Nothing to do, timeout.
  if (retval == 0) {
    return false;
  }

  for (struct pollfd& pollfd_entry : pollfds_) {
    if (pollfd_entry.revents == 0) {
      continue;
    }

    epoll_event event;
    memset(&event, 0, sizeof(event));

    if (pollfd_entry.fd == wake_event_.get()) {
      event.data.ptr = &wake_event_;
    } else {
      auto entry = entries_.find(pollfd_entry.fd);
      CHECK(entry != entries_.end());
      event.data.ptr = &(entry->second);
    }

    for (const auto& epoll_poll : kEpollToPollEvents) {
      if (pollfd_entry.revents & epoll_poll.second) {
        event.events |= epoll_poll.first;
      }
    }
    epoll_events->push_back(event);
    pollfd_entry.revents = 0;
  }
  return true;
}

void MessagePumpEpoll::OnEpollEvent(EpollEventEntry& entry, uint32_t events) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!entry.stopped);

  const bool readable = (events & EPOLLIN) != 0;
  const bool writable = (events & EPOLLOUT) != 0;

  // Under different circumstances, peer closure may raise both/either EPOLLHUP
  // and/or EPOLLERR. Treat them as equivalent. Notify the watchers to
  // gracefully stop watching if disconnected.
  const bool disconnected = (events & (EPOLLHUP | EPOLLERR)) != 0;
  DCHECK(readable || writable || disconnected);

  // Copy the set of Interests, since interests may be added to or removed from
  // `entry` during the loop below. This copy is inexpensive in practice
  // because the size of this vector is expected to be very small (<= 2).
  auto interests = entry.interests;

  // Any of these interests' event handlers may destroy any of the others'
  // controllers. Start all of them watching for destruction before we actually
  // dispatch any events.
  for (const auto& interest : interests) {
    interest->WatchForControllerDestruction();
  }

  bool event_handled = false;
  for (const auto& interest : interests) {
    if (!interest->active()) {
      continue;
    }

    const bool can_read = (readable || disconnected) && interest->params().read;
    const bool can_write =
        (writable || disconnected) && interest->params().write;
    if (!can_read && !can_write) {
      // If this Interest is active but not watching for whichever event was
      // raised here, there's nothing to do. This can occur if a descriptor has
      // multiple active interests, since only one interest needs to be
      // satisfied in order for us to process an epoll event.
      continue;
    }

    if (interest->params().one_shot) {
      // This is a one-shot event watch which is about to be triggered. We
      // deactivate the interest and update epoll immediately. The event handler
      // may reactivate it.
      interest->set_active(false);
      UpdateEpollEvent(entry);
    }

    if (!interest->was_controller_destroyed()) {
      HandleEvent(entry.fd, can_read, can_write, interest->controller());
      event_handled = true;
    }
  }

  // Stop `EpollEventEntry` for disconnected file descriptor without active
  // interests.
  if (disconnected && !event_handled) {
    StopEpollEvent(entry);
  }

  for (const auto& interest : interests) {
    interest->StopWatchingForControllerDestruction();
  }
}

void MessagePumpEpoll::HandleEvent(int fd,
                                   bool can_read,
                                   bool can_write,
                                   FdWatchController* controller) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  BeginNativeWorkBatch();
  // Make the MessagePumpDelegate aware of this other form of "DoWork". Skip if
  // HandleNotification() is called outside of Run() (e.g. in unit tests).
  Delegate::ScopedDoWorkItem scoped_do_work_item;
  if (run_state_) {
    scoped_do_work_item = run_state_->delegate->BeginWorkItem();
  }

  // Trace events must begin after the above BeginWorkItem() so that the
  // ensuing "ThreadController active" outscopes all the events under it.
  TRACE_EVENT("toplevel", "EpollEvent", "controller_created_from",
              controller->created_from_location(), "fd", fd, "can_read",
              can_read, "can_write", can_write, "context",
              static_cast<void*>(controller));
  TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION heap_profiler_scope(
      controller->created_from_location().file_name());
  if (can_read && can_write) {
    bool controller_was_destroyed = false;
    bool* previous_was_destroyed_flag =
        std::exchange(controller->was_destroyed_, &controller_was_destroyed);

    controller->OnFdWritable();
    if (!controller_was_destroyed) {
      controller->OnFdReadable();
    }
    if (!controller_was_destroyed) {
      controller->was_destroyed_ = previous_was_destroyed_flag;
    } else if (previous_was_destroyed_flag) {
      *previous_was_destroyed_flag = true;
    }
  } else if (can_write) {
    controller->OnFdWritable();
  } else if (can_read) {
    controller->OnFdReadable();
  }
}

void MessagePumpEpoll::HandleWakeUp() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  BeginNativeWorkBatch();
  uint64_t value;
  ssize_t n = HANDLE_EINTR(read(wake_event_.get(), &value, sizeof(value)));
  DPCHECK(n == sizeof(value));
}

void MessagePumpEpoll::BeginNativeWorkBatch() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Call `BeginNativeWorkBeforeDoWork()` if native work hasn't started.
  if (!native_work_started_) {
    if (run_state_) {
      run_state_->delegate->BeginNativeWorkBeforeDoWork();
    }
    native_work_started_ = true;
  }
}

MessagePumpEpoll::EpollEventEntry::EpollEventEntry(int fd) : fd(fd) {}

MessagePumpEpoll::EpollEventEntry::~EpollEventEntry() {
  if (active_event) {
    DCHECK_EQ(this, active_event->data.ptr);
    active_event->data.ptr = nullptr;
  }
}

uint32_t MessagePumpEpoll::EpollEventEntry::ComputeActiveEvents() const {
  uint32_t events = 0;
  bool one_shot = true;
  for (const auto& interest : interests) {
    if (!interest->active()) {
      continue;
    }
    const InterestParams& params = interest->params();
    events |= (params.read ? EPOLLIN : 0) | (params.write ? EPOLLOUT : 0);
    one_shot &= params.one_shot;
  }
  if (events != 0 && one_shot) {
    return events | EPOLLONESHOT;
  }
  return events;
}

MessagePumpEpoll::FdWatchController::FdWatchController(
    const Location& from_here)
    : FdWatchControllerInterface(from_here) {}

MessagePumpEpoll::FdWatchController::~FdWatchController() {
  CHECK(StopWatchingFileDescriptor());
  if (was_destroyed_) {
    DCHECK(!*was_destroyed_);
    *was_destroyed_ = true;
  }
}

bool MessagePumpEpoll::FdWatchController::StopWatchingFileDescriptor() {
  watcher_ = nullptr;
  if (pump_ && interest_) {
    pump_->UnregisterInterest(interest_);
    interest_.reset();
    pump_.reset();
  }
  return true;
}

const scoped_refptr<MessagePumpEpoll::Interest>&
MessagePumpEpoll::FdWatchController::AssignInterest(
    const InterestParams& params) {
  interest_ = MakeRefCounted<Interest>(this, params);
  return interest_;
}

void MessagePumpEpoll::FdWatchController::ClearInterest() {
  interest_.reset();
}

void MessagePumpEpoll::FdWatchController::OnFdReadable() {
  if (!watcher_) {
    // When a watcher is watching both read and write and both are possible, the
    // pump will call OnFdWritable() first, followed by OnFdReadable(). But
    // OnFdWritable() may stop or destroy the watch. If the watch is destroyed,
    // the pump will not call OnFdReadable() at all, but if it's merely stopped,
    // OnFdReadable() will be called while `watcher_` is  null. In this case we
    // don't actually want to call the client.
    return;
  }
  watcher_->OnFileCanReadWithoutBlocking(interest_->params().fd);
}

void MessagePumpEpoll::FdWatchController::OnFdWritable() {
  DCHECK(watcher_);
  watcher_->OnFileCanWriteWithoutBlocking(interest_->params().fd);
}

}  // namespace base
