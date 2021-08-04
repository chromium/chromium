// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_blocking_call_internal.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/scoped_clear_last_error.h"
#include "base/task/scoped_set_task_priority_for_current_thread.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_local.h"
#include "build/build_config.h"

namespace base {
namespace internal {

namespace {

// The first 8 characters of sha1 of "ScopedBlockingCall".
// echo -n "ScopedBlockingCall" | sha1sum
constexpr uint32_t kActivityTrackerId = 0x11be9915;

LazyInstance<ThreadLocalPointer<BlockingObserver>>::Leaky
    tls_blocking_observer = LAZY_INSTANCE_INITIALIZER;

// Last ScopedBlockingCall instantiated on this thread.
LazyInstance<ThreadLocalPointer<UncheckedScopedBlockingCall>>::Leaky
    tls_last_scoped_blocking_call = LAZY_INSTANCE_INITIALIZER;

bool IsBackgroundPriorityWorker() {
  return GetTaskPriorityForCurrentThread() == TaskPriority::BEST_EFFORT &&
         CanUseBackgroundPriorityForWorkerThread();
}

}  // namespace

void SetBlockingObserverForCurrentThread(BlockingObserver* blocking_observer) {
  DCHECK(!tls_blocking_observer.Get().Get());
  tls_blocking_observer.Get().Set(blocking_observer);
}

void ClearBlockingObserverForCurrentThread() {
  tls_blocking_observer.Get().Set(nullptr);
}

IOJankMonitoringWindow::ScopedMonitoredCall::ScopedMonitoredCall()
    : call_start_(TimeTicks::Now()),
      assigned_jank_window_(MonitorNextJankWindowIfNecessary(call_start_)) {
  if (assigned_jank_window_) {
    // TimeTicks using a monotonic clock and MonitorNextJankWindowIfNecessary
    // synchronizing via a lock to return |assigned_jank_window_| was initially
    // believed to guarantee that |call_start_| is either equal or beyond
    // |assigned_jank_window_->start_time_|. Violating this assumption can
    // result in negative indexing and OOB-writes in AddJank().
    // We now know this assumption can be violated. This condition hotfixes
    // the issue by discarding ScopedMonitoredCalls where it occurs.
    // TODO(crbug.com/1209622): Implement a proper fix.
    if (call_start_ < assigned_jank_window_->start_time_)
      assigned_jank_window_.reset();
  }
}

IOJankMonitoringWindow::ScopedMonitoredCall::~ScopedMonitoredCall() {
  if (assigned_jank_window_) {
    assigned_jank_window_->OnBlockingCallCompleted(call_start_,
                                                   TimeTicks::Now());
  }
}

void IOJankMonitoringWindow::ScopedMonitoredCall::Cancel() {
  assigned_jank_window_ = nullptr;
}

IOJankMonitoringWindow::IOJankMonitoringWindow(TimeTicks start_time)
    : start_time_(start_time) {}

// static
void IOJankMonitoringWindow::CancelMonitoringForTesting() {
  AutoLock lock(current_jank_window_lock());
  current_jank_window_storage() = nullptr;
  reporting_callback_storage() = NullCallback();
}

// static
constexpr TimeDelta IOJankMonitoringWindow::kIOJankInterval;
// static
constexpr TimeDelta IOJankMonitoringWindow::kMonitoringWindow;
// static
constexpr TimeDelta IOJankMonitoringWindow::kTimeDiscrepancyTimeout;
// static
constexpr int IOJankMonitoringWindow::kNumIntervals;

// static
scoped_refptr<IOJankMonitoringWindow>
IOJankMonitoringWindow::MonitorNextJankWindowIfNecessary(TimeTicks recent_now) {
  DCHECK_GE(TimeTicks::Now(), recent_now);

  scoped_refptr<IOJankMonitoringWindow> next_jank_window;

  {
    AutoLock lock(current_jank_window_lock());

    if (!reporting_callback_storage())
      return nullptr;

    scoped_refptr<IOJankMonitoringWindow>& current_jank_window_ref =
        current_jank_window_storage();

    // Start the next window immediately after the current one (rather than
    // based on Now() to avoid uncovered gaps). Only use Now() for the very
    // first window in a monitoring chain.
    TimeTicks next_window_start_time =
        current_jank_window_ref
            ? current_jank_window_ref->start_time_ + kMonitoringWindow
            : recent_now;

    if (next_window_start_time > recent_now) {
      // Another thread beat us to constructing the next monitoring window and
      // |current_jank_window_ref| already covers |recent_now|.
      return current_jank_window_ref;
    }

    if (recent_now - next_window_start_time >= kTimeDiscrepancyTimeout) {
      // If the delayed task runs on a regular heartbeat, |recent_now| should be
      // roughly equal to |next_window_start_time|. If we miss by more than
      // kTimeDiscrepancyTimeout, we likely hit machine sleep, cancel sampling
      // that window in that case.
      //
      // Note: It is safe to touch |canceled_| without a lock here as this is
      // the only time it's set and it naturally happens-before
      // |current_jank_window_ref|'s destructor reads it.
      current_jank_window_ref->canceled_ = true;
      next_window_start_time = recent_now;
    }

    next_jank_window =
        MakeRefCounted<IOJankMonitoringWindow>(next_window_start_time);

    if (current_jank_window_ref && !current_jank_window_ref->canceled_) {
      // If there are still IO operations in progress within
      // |current_jank_window_ref|, they have a ref to it and will be the ones
      // triggering ~IOJankMonitoringWindow(). When doing so, they will overlap
      // into the |next_jank_window| we are setting up (|next_| will also own a
      // ref so a very long jank can safely unwind across a chain of pending
      // |next_|'s).
      DCHECK(!current_jank_window_ref->next_);
      current_jank_window_ref->next_ = next_jank_window;
    }

    // Make |next_jank_window| the new current before releasing the lock.
    current_jank_window_ref = next_jank_window;
  }

  // Post a task to kick off the next monitoring window if no monitored thread
  // beats us to it. Adjust the timing to alleviate any drift in the timer. Do
  // this outside the lock to avoid scheduling tasks while holding it.
  ThreadPool::PostDelayedTask(
      FROM_HERE, BindOnce([]() {
        IOJankMonitoringWindow::MonitorNextJankWindowIfNecessary(
            TimeTicks::Now());
      }),
      kMonitoringWindow - (recent_now - next_jank_window->start_time_));

  return next_jank_window;
}

// NO_THREAD_SAFETY_ANALYSIS because ~RefCountedThreadSafe() guarantees we're
// the last ones to access this state (and ordered after all other accesses).
IOJankMonitoringWindow::~IOJankMonitoringWindow() NO_THREAD_SAFETY_ANALYSIS {
  if (canceled_)
    return;

  int janky_intervals_count = 0;
  int total_jank_count = 0;

  for (size_t interval_jank_count : intervals_jank_count_) {
    if (interval_jank_count > 0) {
      ++janky_intervals_count;
      total_jank_count += interval_jank_count;
    }
  }

  // reporting_callback_storage() is safe to access without lock because an
  // IOJankMonitoringWindow existing means we're after the call to
  // EnableIOJankMonitoringForProcess() and it will not change after that call.
  DCHECK(reporting_callback_storage());
  reporting_callback_storage().Run(janky_intervals_count, total_jank_count);
}

void IOJankMonitoringWindow::OnBlockingCallCompleted(TimeTicks call_start,
                                                     TimeTicks call_end) {
  // Confirm we never hit a case of TimeTicks going backwards on the same thread
  // nor of TimeTicks rolling over the int64_t boundary (which would break
  // comparison operators).
  DCHECK_LE(call_start, call_end);

  if (call_end - call_start < kIOJankInterval)
    return;

  // Make sure the chain of |next_| pointers is sufficient to reach
  // |call_end| (e.g. if this runs before the delayed task kicks in)
  if (call_end >= start_time_ + kMonitoringWindow)
    MonitorNextJankWindowIfNecessary(call_end);

  // Begin attributing jank to the first interval in which it appeared, no
  // matter how far into the interval the jank began.
  const int jank_start_index =
      ClampFloor((call_start - start_time_) / kIOJankInterval);

  // Round the jank duration so the total number of intervals marked janky is as
  // close as possible to the actual jank duration.
  const int num_janky_intervals =
      ClampRound((call_end - call_start) / kIOJankInterval);

  AddJank(jank_start_index, num_janky_intervals);
}

void IOJankMonitoringWindow::AddJank(int local_jank_start_index,
                                     int num_janky_intervals) {
  DCHECK_GE(local_jank_start_index, 0);
  DCHECK_LT(local_jank_start_index, kNumIntervals);

  // Increment jank counts for intervals in this window. If
  // |num_janky_intervals| lands beyond kNumIntervals, the additional intervals
  // will be reported to |next_|.
  const int jank_end_index = local_jank_start_index + num_janky_intervals;
  const int local_jank_end_index = std::min(kNumIntervals, jank_end_index);

  {
    // Note: while this window could be |canceled| here we must add our count
    // unconditionally as it is only thread-safe to read |canceled| in
    // ~IOJankMonitoringWindow().
    AutoLock lock(intervals_lock_);
    for (int i = local_jank_start_index; i < local_jank_end_index; ++i)
      ++intervals_jank_count_[i];
  }

  if (jank_end_index != local_jank_end_index) {
    // OnBlockingCallCompleted() should have already ensured there's a |next_|
    // chain covering |num_janky_intervals| unless it caused this to be
    // |canceled_|. Exceptionally for this check, reading these fields when
    // they're expected to be true is thread-safe as their only modification
    // happened-before this point.
    DCHECK(next_ || canceled_);
    if (next_) {
      // If |next_| is non-null, it means |this| wasn't canceled and it implies
      // |next_| covers the time range starting immediately after this window.
      DCHECK_EQ(next_->start_time_, start_time_ + kMonitoringWindow);
      next_->AddJank(0, jank_end_index - local_jank_end_index);
    }
  }
}

// static
Lock& IOJankMonitoringWindow::current_jank_window_lock() {
  static NoDestructor<Lock> current_jank_window_lock;
  return *current_jank_window_lock;
}

// static
scoped_refptr<IOJankMonitoringWindow>&
IOJankMonitoringWindow::current_jank_window_storage() {
  static NoDestructor<scoped_refptr<IOJankMonitoringWindow>>
      current_jank_window;
  return *current_jank_window;
}

// static
IOJankReportingCallback& IOJankMonitoringWindow::reporting_callback_storage() {
  static NoDestructor<IOJankReportingCallback> reporting_callback;
  return *reporting_callback;
}

UncheckedScopedBlockingCall::UncheckedScopedBlockingCall(
    const Location& from_here,
    BlockingType blocking_type,
    BlockingCallType blocking_call_type)
    : blocking_observer_(tls_blocking_observer.Get().Get()),
      previous_scoped_blocking_call_(tls_last_scoped_blocking_call.Get().Get()),
      is_will_block_(blocking_type == BlockingType::WILL_BLOCK ||
                     (previous_scoped_blocking_call_ &&
                      previous_scoped_blocking_call_->is_will_block_)),
      scoped_activity_(from_here, 0, kActivityTrackerId, 0) {
  tls_last_scoped_blocking_call.Get().Set(this);

  // Only monitor non-nested ScopedBlockingCall(MAY_BLOCK) calls on foreground
  // threads. Cancels() any pending monitored call when a WILL_BLOCK or
  // ScopedBlockingCallWithBaseSyncPrimitives nests into a
  // ScopedBlockingCall(MAY_BLOCK).
  if (!IsBackgroundPriorityWorker()) {
    const bool is_monitored_type =
        blocking_call_type == BlockingCallType::kRegular && !is_will_block_;
    if (is_monitored_type && !previous_scoped_blocking_call_) {
      monitored_call_.emplace();
    } else if (!is_monitored_type && previous_scoped_blocking_call_ &&
               previous_scoped_blocking_call_->monitored_call_) {
      previous_scoped_blocking_call_->monitored_call_->Cancel();
    }
  }

  if (blocking_observer_) {
    if (!previous_scoped_blocking_call_) {
      blocking_observer_->BlockingStarted(blocking_type);
    } else if (blocking_type == BlockingType::WILL_BLOCK &&
               !previous_scoped_blocking_call_->is_will_block_) {
      blocking_observer_->BlockingTypeUpgraded();
    }
  }

  if (scoped_activity_.IsRecorded()) {
    // Also record the data for extended crash reporting.
    const TimeTicks now = TimeTicks::Now();
    auto& user_data = scoped_activity_.user_data();
    user_data.SetUint("timestamp_us", now.since_origin().InMicroseconds());
    user_data.SetUint("blocking_type", static_cast<uint64_t>(blocking_type));
  }
}

UncheckedScopedBlockingCall::~UncheckedScopedBlockingCall() {
  // TLS affects result of GetLastError() on Windows. ScopedClearLastError
  // prevents side effect.
  ScopedClearLastError save_last_error;
  DCHECK_EQ(this, tls_last_scoped_blocking_call.Get().Get());
  tls_last_scoped_blocking_call.Get().Set(previous_scoped_blocking_call_);
  if (blocking_observer_ && !previous_scoped_blocking_call_)
    blocking_observer_->BlockingEnded();
}

}  // namespace internal

void EnableIOJankMonitoringForProcess(
    IOJankReportingCallback reporting_callback) {
  {
    AutoLock lock(internal::IOJankMonitoringWindow::current_jank_window_lock());

    DCHECK(internal::IOJankMonitoringWindow::reporting_callback_storage()
               .is_null());
    internal::IOJankMonitoringWindow::reporting_callback_storage() =
        std::move(reporting_callback);
  }

  // Make sure monitoring starts now rather than randomly at the next
  // ScopedMonitoredCall construction.
  internal::IOJankMonitoringWindow::MonitorNextJankWindowIfNecessary(
      TimeTicks::Now());
}

}  // namespace base
