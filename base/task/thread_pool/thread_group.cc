// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/lazy_instance.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/threading/thread_local.h"

#if defined(OS_WIN)
#include "base/win/com_init_check_hook.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/windows_version.h"
#endif

namespace base {
namespace internal {

namespace {

// ThreadGroup that owns the current thread, if any.
LazyInstance<ThreadLocalPointer<const ThreadGroup>>::Leaky
    tls_current_thread_group = LAZY_INSTANCE_INITIALIZER;

const ThreadGroup* GetCurrentThreadGroup() {
  return tls_current_thread_group.Get().Get();
}

}  // namespace

constexpr ThreadGroup::YieldSortKey ThreadGroup::kMaxYieldSortKey;

void ThreadGroup::BaseScopedCommandsExecutor::ScheduleReleaseTaskSource(
    RegisteredTaskSource task_source) {
  task_sources_to_release_.push_back(std::move(task_source));
}

ThreadGroup::BaseScopedCommandsExecutor::BaseScopedCommandsExecutor() = default;

ThreadGroup::BaseScopedCommandsExecutor::~BaseScopedCommandsExecutor() {
  CheckedLock::AssertNoLockHeldOnCurrentThread();
}

ThreadGroup::ScopedReenqueueExecutor::ScopedReenqueueExecutor() = default;

ThreadGroup::ScopedReenqueueExecutor::~ScopedReenqueueExecutor() {
  if (destination_thread_group_) {
    destination_thread_group_->PushTaskSourceAndWakeUpWorkers(
        std::move(transaction_with_task_source_.value()));
  }
}

void ThreadGroup::ScopedReenqueueExecutor::
    SchedulePushTaskSourceAndWakeUpWorkers(
        TransactionWithRegisteredTaskSource transaction_with_task_source,
        ThreadGroup* destination_thread_group) {
  DCHECK(destination_thread_group);
  DCHECK(!destination_thread_group_);
  DCHECK(!transaction_with_task_source_);
  transaction_with_task_source_.emplace(
      std::move(transaction_with_task_source));
  destination_thread_group_ = destination_thread_group;
}

ThreadGroup::ThreadGroup(TrackedRef<TaskTracker> task_tracker,
                         TrackedRef<Delegate> delegate,
                         ThreadGroup* predecessor_thread_group)
    : task_tracker_(std::move(task_tracker)),
      delegate_(std::move(delegate)),
      lock_(predecessor_thread_group ? &predecessor_thread_group->lock_
                                     : nullptr) {
  DCHECK(task_tracker_);
}

ThreadGroup::~ThreadGroup() = default;

void ThreadGroup::BindToCurrentThread() {
  DCHECK(!GetCurrentThreadGroup());
  tls_current_thread_group.Get().Set(this);
}

void ThreadGroup::UnbindFromCurrentThread() {
  DCHECK(GetCurrentThreadGroup());
  tls_current_thread_group.Get().Set(nullptr);
}

bool ThreadGroup::IsBoundToCurrentThread() const {
  return GetCurrentThreadGroup() == this;
}

void ThreadGroup::Start() {
  CheckedAutoLock auto_lock(lock_);
  disable_fair_scheduling_ = FeatureList::IsEnabled(kDisableFairJobScheduling);
}

size_t
ThreadGroup::GetNumAdditionalWorkersForBestEffortTaskSourcesLockRequired()
    const {
  // For simplicity, only 1 worker is assigned to each task source regardless of
  // its max concurrency, with the exception of the top task source.
  const size_t num_queued =
      priority_queue_.GetNumTaskSourcesWithPriority(TaskPriority::BEST_EFFORT);
  if (num_queued == 0 ||
      !task_tracker_->CanRunPriority(TaskPriority::BEST_EFFORT)) {
    return 0U;
  }
  if (priority_queue_.PeekSortKey().priority() == TaskPriority::BEST_EFFORT) {
    // Assign the correct number of workers for the top TaskSource (-1 for the
    // worker that is already accounted for in |num_queued|).
    return std::max<size_t>(
        1, num_queued +
               priority_queue_.PeekTaskSource()->GetRemainingConcurrency() - 1);
  }
  return num_queued;
}

size_t
ThreadGroup::GetNumAdditionalWorkersForForegroundTaskSourcesLockRequired()
    const {
  // For simplicity, only 1 worker is assigned to each task source regardless of
  // its max concurrency, with the exception of the top task source.
  const size_t num_queued = priority_queue_.GetNumTaskSourcesWithPriority(
                                TaskPriority::USER_VISIBLE) +
                            priority_queue_.GetNumTaskSourcesWithPriority(
                                TaskPriority::USER_BLOCKING);
  if (num_queued == 0 ||
      !task_tracker_->CanRunPriority(TaskPriority::HIGHEST)) {
    return 0U;
  }
  auto priority = priority_queue_.PeekSortKey().priority();
  if (priority == TaskPriority::USER_VISIBLE ||
      priority == TaskPriority::USER_BLOCKING) {
    // Assign the correct number of workers for the top TaskSource (-1 for the
    // worker that is already accounted for in |num_queued|).
    return std::max<size_t>(
        1, num_queued +
               priority_queue_.PeekTaskSource()->GetRemainingConcurrency() - 1);
  }
  return num_queued;
}

RegisteredTaskSource ThreadGroup::RemoveTaskSource(
    const TaskSource& task_source) {
  CheckedAutoLock auto_lock(lock_);
  return priority_queue_.RemoveTaskSource(task_source);
}

void ThreadGroup::ReEnqueueTaskSourceLockRequired(
    BaseScopedCommandsExecutor* workers_executor,
    ScopedReenqueueExecutor* reenqueue_executor,
    TransactionWithRegisteredTaskSource transaction_with_task_source) {
  // Decide in which thread group the TaskSource should be reenqueued.
  ThreadGroup* destination_thread_group = delegate_->GetThreadGroupForTraits(
      transaction_with_task_source.transaction.traits());

  if (destination_thread_group == this) {
    // Another worker that was running a task from this task source may have
    // reenqueued it already, in which case its heap_handle will be valid. It
    // shouldn't be queued twice so the task source registration is released.
    if (transaction_with_task_source.task_source->heap_handle().IsValid()) {
      workers_executor->ScheduleReleaseTaskSource(
          std::move(transaction_with_task_source.task_source));
    } else {
      // If the TaskSource should be reenqueued in the current thread group,
      // reenqueue it inside the scope of the lock.
      auto sort_key = transaction_with_task_source.task_source->GetSortKey(
          disable_fair_scheduling_);
      priority_queue_.Push(std::move(transaction_with_task_source.task_source),
                           sort_key);
    }
    // This is called unconditionally to ensure there are always workers to run
    // task sources in the queue. Some ThreadGroup implementations only invoke
    // TakeRegisteredTaskSource() once per wake up and hence this is required to
    // avoid races that could leave a task source stranded in the queue with no
    // active workers.
    EnsureEnoughWorkersLockRequired(workers_executor);
  } else {
    // Otherwise, schedule a reenqueue after releasing the lock.
    reenqueue_executor->SchedulePushTaskSourceAndWakeUpWorkers(
        std::move(transaction_with_task_source), destination_thread_group);
  }
}

RegisteredTaskSource ThreadGroup::TakeRegisteredTaskSource(
    BaseScopedCommandsExecutor* executor) {
  DCHECK(!priority_queue_.IsEmpty());

  auto run_status = priority_queue_.PeekTaskSource().WillRunTask();

  if (run_status == TaskSource::RunStatus::kDisallowed) {
    executor->ScheduleReleaseTaskSource(priority_queue_.PopTaskSource());
    return nullptr;
  }

  if (run_status == TaskSource::RunStatus::kAllowedSaturated)
    return priority_queue_.PopTaskSource();

  // If the TaskSource isn't saturated, check whether TaskTracker allows it to
  // remain in the PriorityQueue.
  // The canonical way of doing this is to pop the task source to return, call
  // RegisterTaskSource() to get an additional RegisteredTaskSource, and
  // reenqueue that task source if valid. Instead, it is cheaper and equivalent
  // to peek the task source, call RegisterTaskSource() to get an additional
  // RegisteredTaskSource to replace if valid, and only pop |priority_queue_|
  // otherwise.
  RegisteredTaskSource task_source =
      task_tracker_->RegisterTaskSource(priority_queue_.PeekTaskSource().get());
  if (!task_source)
    return priority_queue_.PopTaskSource();
  // Replace the top task_source and then update the queue.
  std::swap(priority_queue_.PeekTaskSource(), task_source);
  if (!disable_fair_scheduling_) {
    priority_queue_.UpdateSortKey(*task_source.get(),
                                  task_source->GetSortKey(false));
  }
  return task_source;
}

void ThreadGroup::UpdateSortKeyImpl(BaseScopedCommandsExecutor* executor,
                                    TaskSource::Transaction transaction) {
  CheckedAutoLock auto_lock(lock_);
  priority_queue_.UpdateSortKey(
      *transaction.task_source(),
      transaction.task_source()->GetSortKey(disable_fair_scheduling_));
  EnsureEnoughWorkersLockRequired(executor);
}

void ThreadGroup::PushTaskSourceAndWakeUpWorkersImpl(
    BaseScopedCommandsExecutor* executor,
    TransactionWithRegisteredTaskSource transaction_with_task_source) {
  CheckedAutoLock auto_lock(lock_);
  DCHECK(!replacement_thread_group_);
  DCHECK_EQ(delegate_->GetThreadGroupForTraits(
                transaction_with_task_source.transaction.traits()),
            this);
  if (transaction_with_task_source.task_source->heap_handle().IsValid()) {
    // If the task source changed group, it is possible that multiple concurrent
    // workers try to enqueue it. Only the first enqueue should succeed.
    executor->ScheduleReleaseTaskSource(
        std::move(transaction_with_task_source.task_source));
    return;
  }
  auto sort_key = transaction_with_task_source.task_source->GetSortKey(
      disable_fair_scheduling_);
  priority_queue_.Push(std::move(transaction_with_task_source.task_source),
                       sort_key);
  EnsureEnoughWorkersLockRequired(executor);
}

void ThreadGroup::InvalidateAndHandoffAllTaskSourcesToOtherThreadGroup(
    ThreadGroup* destination_thread_group) {
  CheckedAutoLock current_thread_group_lock(lock_);
  CheckedAutoLock destination_thread_group_lock(
      destination_thread_group->lock_);
  destination_thread_group->priority_queue_ = std::move(priority_queue_);
  replacement_thread_group_ = destination_thread_group;
}

bool ThreadGroup::ShouldYield(TaskSourceSortKey sort_key) {
  DCHECK(TS_UNCHECKED_READ(max_allowed_sort_key_).is_lock_free());

  if (!task_tracker_->CanRunPriority(sort_key.priority()))
    return true;
  // It is safe to read |max_allowed_sort_key_| without a lock since this
  // variable is atomic, keeping in mind that threads may not immediately see
  // the new value when it is updated.
  auto max_allowed_sort_key =
      TS_UNCHECKED_READ(max_allowed_sort_key_).load(std::memory_order_relaxed);

  // To reduce unnecessary yielding, a task will never yield to a BEST_EFFORT
  // task regardless of its worker_count.
  if (sort_key.priority() > max_allowed_sort_key.priority ||
      max_allowed_sort_key.priority == TaskPriority::BEST_EFFORT) {
    return false;
  }
  // Otherwise, a task only yields to a task of equal priority if its
  // worker_count would be greater still after yielding, e.g. a job with 1
  // worker doesn't yield to a job with 0 workers.
  if (sort_key.priority() == max_allowed_sort_key.priority &&
      sort_key.worker_count() <= max_allowed_sort_key.worker_count + 1) {
    return false;
  }

  // Reset |max_allowed_sort_key_| so that only one thread should yield at a
  // time for a given task.
  max_allowed_sort_key =
      TS_UNCHECKED_READ(max_allowed_sort_key_)
          .exchange(kMaxYieldSortKey, std::memory_order_relaxed);
  // Another thread might have decided to yield and racily reset
  // |max_allowed_sort_key_|, in which case this thread doesn't yield.
  return max_allowed_sort_key.priority != TaskPriority::BEST_EFFORT;
}

#if defined(OS_WIN)
// static
std::unique_ptr<win::ScopedWindowsThreadEnvironment>
ThreadGroup::GetScopedWindowsThreadEnvironment(WorkerEnvironment environment) {
  std::unique_ptr<win::ScopedWindowsThreadEnvironment> scoped_environment;
  switch (environment) {
    case WorkerEnvironment::COM_MTA: {
      if (win::GetVersion() >= win::Version::WIN8) {
        scoped_environment = std::make_unique<win::ScopedWinrtInitializer>();
      } else {
        scoped_environment = std::make_unique<win::ScopedCOMInitializer>(
            win::ScopedCOMInitializer::kMTA);
      }
      break;
    }
    case WorkerEnvironment::COM_STA: {
      // When defined(COM_INIT_CHECK_HOOK_ENABLED), ignore
      // WorkerEnvironment::COM_STA to find incorrect uses of
      // COM that should be running in a COM STA Task Runner.
#if !defined(COM_INIT_CHECK_HOOK_ENABLED)
      scoped_environment = std::make_unique<win::ScopedCOMInitializer>();
#endif
      break;
    }
    default:
      break;
  }

  DCHECK(!scoped_environment || scoped_environment->Succeeded());
  return scoped_environment;
}
#endif

}  // namespace internal
}  // namespace base
