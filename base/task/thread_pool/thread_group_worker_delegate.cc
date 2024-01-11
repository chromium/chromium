// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group_worker_delegate.h"

#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/time/time_override.h"

namespace base::internal {

ThreadGroup::ThreadGroupWorkerDelegate::ThreadGroupWorkerDelegate(
    TrackedRef<ThreadGroup> outer)
    : outer_(outer) {
  // Bound in OnMainEntry().
  DETACH_FROM_THREAD(worker_thread_checker_);
}

ThreadGroup::ThreadGroupWorkerDelegate::~ThreadGroupWorkerDelegate() = default;

ThreadGroup::ThreadGroupWorkerDelegate::WorkerOnly::WorkerOnly() = default;
ThreadGroup::ThreadGroupWorkerDelegate::WorkerOnly::~WorkerOnly() = default;

TimeDelta ThreadGroup::ThreadGroupWorkerDelegate::ThreadPoolSleepTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  if (!IsExcess()) {
    return TimeDelta::Max();
  }
  // Sleep for an extra 10% to avoid the following pathological case:
  //   0) A task is running on a timer which matches
  //      |after_start().suggested_reclaim_time|.
  //   1) The timer fires and this worker is created by
  //      MaintainAtLeastOneIdleWorkerLockRequired() because the last idle
  //      worker was assigned the task.
  //   2) This worker begins sleeping |after_start().suggested_reclaim_time|
  //      (at the front of the idle set).
  //   3) The task assigned to the other worker completes and the worker goes
  //      back in the idle set (this worker may now second on the idle set;
  //      its GetLastUsedTime() is set to Now()).
  //   4) The sleep in (2) expires. Since (3) was fast this worker is likely
  //      to have been second on the idle set long enough for
  //      CanCleanupLockRequired() to be satisfied in which case this worker
  //      is cleaned up.
  //   5) The timer fires at roughly the same time and we're back to (1) if
  //      (4) resulted in a clean up; causing thread churn.
  //
  //   Sleeping 10% longer in (2) makes it much less likely that (4) occurs
  //   before (5). In that case (5) will cause (3) and refresh this worker's
  //   GetLastUsedTime(), making CanCleanupLockRequired() return false in (4)
  //   and avoiding churn.
  //
  //   Of course the same problem arises if in (0) the timer matches
  //   |after_start().suggested_reclaim_time * 1.1| but it's expected that any
  //   timer slower than |after_start().suggested_reclaim_time| will cause
  //   such churn during long idle periods. If this is a problem in practice,
  //   the standby thread configuration and algorithm should be revisited.
  return outer_->after_start().suggested_reclaim_time * 1.1;
}

// BlockingObserver:
void ThreadGroup::ThreadGroupWorkerDelegate::BlockingStarted(
    BlockingType blocking_type) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(worker_only().worker_thread_);
  // Skip if this blocking scope happened outside of a RunTask.
  if (!read_worker().current_task_priority) {
    return;
  }

  worker_only().worker_thread_->MaybeUpdateThreadType();

  // WillBlock is always used when time overrides is active. crbug.com/1038867
  if (base::subtle::ScopedTimeClockOverrides::overrides_active()) {
    blocking_type = BlockingType::WILL_BLOCK;
  }

  std::unique_ptr<BaseScopedCommandsExecutor> executor = outer_->GetExecutor();
  CheckedAutoLock auto_lock(outer_->lock_);

  DCHECK(!incremented_max_tasks_since_blocked_);
  DCHECK(!incremented_max_best_effort_tasks_since_blocked_);
  DCHECK(read_worker().blocking_start_time.is_null());
  write_worker().blocking_start_time = subtle::TimeTicksNowIgnoringOverride();

  if (incremented_max_tasks_for_shutdown_) {
    return;
  }

  if (*read_any().current_task_priority == TaskPriority::BEST_EFFORT) {
    ++outer_->num_unresolved_best_effort_may_block_;
  }

  if (blocking_type == BlockingType::WILL_BLOCK) {
    incremented_max_tasks_since_blocked_ = true;
    outer_->IncrementMaxTasksLockRequired();
    outer_->EnsureEnoughWorkersLockRequired(executor.get());
  } else {
    ++outer_->num_unresolved_may_block_;
  }

  outer_->MaybeScheduleAdjustMaxTasksLockRequired(executor.get());
}

void ThreadGroup::ThreadGroupWorkerDelegate::BlockingTypeUpgraded() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  // Skip if this blocking scope happened outside of a RunTask.
  if (!read_worker().current_task_priority) {
    return;
  }

  // The blocking type always being WILL_BLOCK in this experiment and with
  // time overrides, it should never be considered "upgraded".
  if (base::subtle::ScopedTimeClockOverrides::overrides_active()) {
    return;
  }

  std::unique_ptr<BaseScopedCommandsExecutor> executor = outer_->GetExecutor();
  CheckedAutoLock auto_lock(outer_->lock_);

  // Don't do anything if a MAY_BLOCK ScopedBlockingCall instantiated in the
  // same scope already caused the max tasks to be incremented.
  if (incremented_max_tasks_since_blocked_) {
    return;
  }

  // Cancel the effect of a MAY_BLOCK ScopedBlockingCall instantiated in the
  // same scope.
  --outer_->num_unresolved_may_block_;

  incremented_max_tasks_since_blocked_ = true;
  outer_->IncrementMaxTasksLockRequired();
  outer_->EnsureEnoughWorkersLockRequired(executor.get());
}

void ThreadGroup::ThreadGroupWorkerDelegate::BlockingEnded() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  // Skip if this blocking scope happened outside of a RunTask.
  if (!read_worker().current_task_priority) {
    return;
  }

  CheckedAutoLock auto_lock(outer_->lock_);
  DCHECK(!read_worker().blocking_start_time.is_null());
  write_worker().blocking_start_time = TimeTicks();
  if (!incremented_max_tasks_for_shutdown_) {
    if (incremented_max_tasks_since_blocked_) {
      outer_->DecrementMaxTasksLockRequired();
    } else {
      --outer_->num_unresolved_may_block_;
    }

    if (*read_worker().current_task_priority == TaskPriority::BEST_EFFORT) {
      if (incremented_max_best_effort_tasks_since_blocked_) {
        outer_->DecrementMaxBestEffortTasksLockRequired();
      } else {
        --outer_->num_unresolved_best_effort_may_block_;
      }
    }
  }

  incremented_max_tasks_since_blocked_ = false;
  incremented_max_best_effort_tasks_since_blocked_ = false;
}

// Notifies the worker of shutdown, possibly marking the running task as
// MAY_BLOCK.
void ThreadGroup::ThreadGroupWorkerDelegate::OnShutdownStartedLockRequired(
    BaseScopedCommandsExecutor* executor) {
  if (!read_any().is_running_task()) {
    return;
  }
  // Workers running a CONTINUE_ON_SHUTDOWN tasks are replaced by incrementing
  // max_tasks/max_best_effort_tasks. The effect is reverted in
  // DidProcessTask().
  if (*read_any().current_shutdown_behavior ==
      TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN) {
    incremented_max_tasks_for_shutdown_ = true;
    IncrementMaxTasksLockRequired();
  }
}

// Increments max [best effort] tasks iff this worker has been within a
// ScopedBlockingCall for more than |may_block_threshold|.
void ThreadGroup::ThreadGroupWorkerDelegate::
    MaybeIncrementMaxTasksLockRequired()
        EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
  if (read_any().blocking_start_time.is_null() ||
      subtle::TimeTicksNowIgnoringOverride() - read_any().blocking_start_time <
          outer_->after_start().may_block_threshold) {
    return;
  }
  IncrementMaxTasksLockRequired();
}

// Increments max [best effort] tasks.
void ThreadGroup::ThreadGroupWorkerDelegate::IncrementMaxTasksLockRequired()
    EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
  if (!incremented_max_tasks_since_blocked_) {
    outer_->IncrementMaxTasksLockRequired();
    // Update state for an unresolved ScopedBlockingCall.
    if (!read_any().blocking_start_time.is_null()) {
      incremented_max_tasks_since_blocked_ = true;
      --outer_->num_unresolved_may_block_;
    }
  }
  if (*read_any().current_task_priority == TaskPriority::BEST_EFFORT &&
      !incremented_max_best_effort_tasks_since_blocked_) {
    outer_->IncrementMaxBestEffortTasksLockRequired();
    // Update state for an unresolved ScopedBlockingCall.
    if (!read_any().blocking_start_time.is_null()) {
      incremented_max_best_effort_tasks_since_blocked_ = true;
      --outer_->num_unresolved_best_effort_may_block_;
    }
  }
}

}  // namespace base::internal
