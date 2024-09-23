// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/task_features.h"
#include "base/task/thread_pool/task_tracker.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/com_init_check_hook.h"
#include "base/win/scoped_winrt_initializer.h"
#endif

namespace base {
namespace internal {

namespace {

constexpr size_t kMaxNumberOfWorkers = 256;

// In a background thread group:
// - Blocking calls take more time than in a foreground thread group.
// - We want to minimize impact on foreground work, not maximize execution
//   throughput.
// For these reasons, the timeout to increase the maximum number of concurrent
// tasks when there is a MAY_BLOCK ScopedBlockingCall is *long*. It is not
// infinite because execution throughput should not be reduced forever if a task
// blocks forever.
//
// TODO(fdoray): On platforms without background thread groups, blocking in a
// BEST_EFFORT task should:
// 1. Increment the maximum number of concurrent tasks after a *short* timeout,
//    to allow scheduling of USER_VISIBLE/USER_BLOCKING tasks.
// 2. Increment the maximum number of concurrent BEST_EFFORT tasks after a
//    *long* timeout, because we only want to allow more BEST_EFFORT tasks to be
//    be scheduled concurrently when we believe that a BEST_EFFORT task is
//    blocked forever.
// Currently, only 1. is true as the configuration is per thread group.
// TODO(crbug.com/40612168): Fix racy condition when MayBlockThreshold ==
// BlockedWorkersPoll.
constexpr TimeDelta kForegroundMayBlockThreshold = Milliseconds(1000);
constexpr TimeDelta kForegroundBlockedWorkersPoll = Milliseconds(1200);
constexpr TimeDelta kBackgroundMayBlockThreshold = Seconds(10);
constexpr TimeDelta kBackgroundBlockedWorkersPoll = Seconds(12);

// ThreadGroup that owns the current thread, if any.
constinit thread_local const ThreadGroup* current_thread_group = nullptr;

}  // namespace

constexpr ThreadGroup::YieldSortKey ThreadGroup::kMaxYieldSortKey;

void ThreadGroup::BaseScopedCommandsExecutor::ScheduleReleaseTaskSource(
    RegisteredTaskSource task_source) {
  task_sources_to_release_.push_back(std::move(task_source));
}

void ThreadGroup::BaseScopedCommandsExecutor::ScheduleAdjustMaxTasks() {
  DCHECK(!must_schedule_adjust_max_tasks_);
  must_schedule_adjust_max_tasks_ = true;
}

void ThreadGroup::BaseScopedCommandsExecutor::ScheduleStart(
    scoped_refptr<WorkerThread> worker) {
  workers_to_start_.emplace_back(std::move(worker));
}

ThreadGroup::BaseScopedCommandsExecutor::BaseScopedCommandsExecutor(
    ThreadGroup* outer)
    : outer_(outer) {}

ThreadGroup::BaseScopedCommandsExecutor::~BaseScopedCommandsExecutor() {
  CheckedLock::AssertNoLockHeldOnCurrentThread();
  Flush();
}

void ThreadGroup::BaseScopedCommandsExecutor::Flush() {
  // Start workers. Happens after wake ups (implemented by children and thus
  // called on their destructor, i.e. before this) to prevent the case where a
  // worker enters its main function, is descheduled because it wasn't woken up
  // yet, and is woken up immediately after.
  for (auto worker : workers_to_start_) {
    worker->Start(outer_->after_start().service_thread_task_runner,
                  outer_->after_start().worker_thread_observer);
    if (outer_->worker_started_for_testing_) {
      outer_->worker_started_for_testing_->Wait();
    }
  }
  workers_to_start_.clear();

  if (must_schedule_adjust_max_tasks_) {
    outer_->ScheduleAdjustMaxTasks();
  }
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
        RegisteredTaskSourceAndTransaction transaction_with_task_source,
        ThreadGroup* destination_thread_group) {
  DCHECK(destination_thread_group);
  DCHECK(!destination_thread_group_);
  DCHECK(!transaction_with_task_source_);
  transaction_with_task_source_.emplace(
      std::move(transaction_with_task_source));
  destination_thread_group_ = destination_thread_group;
}

ThreadGroup::ThreadGroup(std::string_view histogram_label,
                         std::string_view thread_group_label,
                         ThreadType thread_type_hint,
                         TrackedRef<TaskTracker> task_tracker,
                         TrackedRef<Delegate> delegate)
    : task_tracker_(std::move(task_tracker)),
      delegate_(std::move(delegate)),
      histogram_label_(histogram_label),
      thread_group_label_(thread_group_label),
      thread_type_hint_(thread_type_hint),
      idle_workers_set_cv_for_testing_(lock_.CreateConditionVariable()) {
  DCHECK(!thread_group_label_.empty());
}

void ThreadGroup::StartImpl(
    size_t max_tasks,
    size_t max_best_effort_tasks,
    TimeDelta suggested_reclaim_time,
    scoped_refptr<SingleThreadTaskRunner> service_thread_task_runner,
    WorkerThreadObserver* worker_thread_observer,
    WorkerEnvironment worker_environment,
    bool synchronous_thread_start_for_testing,
    std::optional<TimeDelta> may_block_threshold) {
  if (synchronous_thread_start_for_testing) {
    worker_started_for_testing_.emplace(WaitableEvent::ResetPolicy::AUTOMATIC);
    // Don't emit a ScopedBlockingCallWithBaseSyncPrimitives from this
    // WaitableEvent or it defeats the purpose of having threads start without
    // externally visible side-effects.
    worker_started_for_testing_->declare_only_used_while_idle();
  }

  in_start().no_worker_reclaim = FeatureList::IsEnabled(kNoWorkerThreadReclaim);
  in_start().may_block_threshold =
      may_block_threshold ? may_block_threshold.value()
                          : (thread_type_hint_ != ThreadType::kBackground
                                 ? kForegroundMayBlockThreshold
                                 : kBackgroundMayBlockThreshold);
  in_start().blocked_workers_poll_period =
      thread_type_hint_ != ThreadType::kBackground
          ? kForegroundBlockedWorkersPoll
          : kBackgroundBlockedWorkersPoll;
  in_start().max_num_workers_created = base::kMaxNumWorkersCreated.Get();

  CheckedAutoLock auto_lock(lock_);

  max_tasks_ = max_tasks;
  baseline_max_tasks_ = max_tasks;
  DCHECK_GE(max_tasks_, 1U);
  in_start().initial_max_tasks = std::min(max_tasks_, kMaxNumberOfWorkers);
  max_best_effort_tasks_ = max_best_effort_tasks;
  in_start().suggested_reclaim_time = suggested_reclaim_time;
  in_start().worker_environment = worker_environment;
  in_start().service_thread_task_runner = std::move(service_thread_task_runner);
  in_start().worker_thread_observer = worker_thread_observer;

#if DCHECK_IS_ON()
  in_start().initialized = true;
#endif
}

ThreadGroup::~ThreadGroup() = default;

void ThreadGroup::BindToCurrentThread() {
  DCHECK(!CurrentThreadHasGroup());
  current_thread_group = this;
}

void ThreadGroup::UnbindFromCurrentThread() {
  DCHECK(IsBoundToCurrentThread());
  current_thread_group = nullptr;
}

bool ThreadGroup::IsBoundToCurrentThread() const {
  return current_thread_group == this;
}

void ThreadGroup::SetMaxTasks(size_t max_tasks) {
  CheckedAutoLock auto_lock(lock_);
  size_t extra_tasks = max_tasks_ - baseline_max_tasks_;
  baseline_max_tasks_ = std::min(max_tasks, after_start().initial_max_tasks);
  max_tasks_ = baseline_max_tasks_ + extra_tasks;
}

void ThreadGroup::ResetMaxTasks() {
  SetMaxTasks(after_start().initial_max_tasks);
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
    RegisteredTaskSourceAndTransaction transaction_with_task_source) {
  // Decide in which thread group the TaskSource should be reenqueued.
  ThreadGroup* destination_thread_group = delegate_->GetThreadGroupForTraits(
      transaction_with_task_source.transaction.traits());

  bool push_to_immediate_queue =
      transaction_with_task_source.task_source.WillReEnqueue(
          TimeTicks::Now(), &transaction_with_task_source.transaction);

  if (destination_thread_group == this) {
    // Another worker that was running a task from this task source may have
    // reenqueued it already, in which case its heap_handle will be valid. It
    // shouldn't be queued twice so the task source registration is released.
    if (transaction_with_task_source.task_source->immediate_heap_handle()
            .IsValid()) {
      workers_executor->ScheduleReleaseTaskSource(
          std::move(transaction_with_task_source.task_source));
    } else {
      // If the TaskSource should be reenqueued in the current thread group,
      // reenqueue it inside the scope of the lock.
      if (push_to_immediate_queue) {
        auto sort_key = transaction_with_task_source.task_source->GetSortKey();
        // When moving |task_source| into |priority_queue_|, it may be destroyed
        // on another thread as soon as |lock_| is released, since we're no
        // longer holding a reference to it. To prevent UAF, release
        // |transaction| before moving |task_source|. Ref. crbug.com/1412008
        transaction_with_task_source.transaction.Release();
        priority_queue_.Push(
            std::move(transaction_with_task_source.task_source), sort_key);
      }
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

  if (run_status == TaskSource::RunStatus::kAllowedSaturated) {
    return priority_queue_.PopTaskSource();
  }

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
  if (!task_source) {
    return priority_queue_.PopTaskSource();
  }
  // Replace the top task_source and then update the queue.
  std::swap(priority_queue_.PeekTaskSource(), task_source);
  priority_queue_.UpdateSortKey(*task_source.get(), task_source->GetSortKey());
  return task_source;
}

void ThreadGroup::UpdateSortKeyImpl(BaseScopedCommandsExecutor* executor,
                                    TaskSource::Transaction transaction) {
  CheckedAutoLock auto_lock(lock_);
  priority_queue_.UpdateSortKey(*transaction.task_source(),
                                transaction.task_source()->GetSortKey());
  EnsureEnoughWorkersLockRequired(executor);
}

void ThreadGroup::PushTaskSourceAndWakeUpWorkersImpl(
    BaseScopedCommandsExecutor* executor,
    RegisteredTaskSourceAndTransaction transaction_with_task_source) {
  DCHECK_EQ(delegate_->GetThreadGroupForTraits(
                transaction_with_task_source.transaction.traits()),
            this);
  CheckedAutoLock lock(lock_);
  if (transaction_with_task_source.task_source->immediate_heap_handle()
          .IsValid()) {
    // If the task source changed group, it is possible that multiple concurrent
    // workers try to enqueue it. Only the first enqueue should succeed.
    executor->ScheduleReleaseTaskSource(
        std::move(transaction_with_task_source.task_source));
    return;
  }
  auto sort_key = transaction_with_task_source.task_source->GetSortKey();
  // When moving |task_source| into |priority_queue_|, it may be destroyed
  // on another thread as soon as |lock_| is released, since we're no longer
  // holding a reference to it. To prevent UAF, release |transaction| before
  // moving |task_source|. Ref. crbug.com/1412008
  transaction_with_task_source.transaction.Release();
  priority_queue_.Push(std::move(transaction_with_task_source.task_source),
                       sort_key);
  EnsureEnoughWorkersLockRequired(executor);
}

void ThreadGroup::EnqueueAllTaskSources(PriorityQueue* new_priority_queue) {
  CheckedAutoLock lock(lock_);
  while (!new_priority_queue->IsEmpty()) {
    TaskSourceSortKey top_sort_key = new_priority_queue->PeekSortKey();
    RegisteredTaskSource task_source = new_priority_queue->PopTaskSource();
    priority_queue_.Push(std::move(task_source), top_sort_key);
  }
}

void ThreadGroup::HandoffAllTaskSourcesToOtherThreadGroup(
    ThreadGroup* destination_thread_group) {
  PriorityQueue new_priority_queue;
  TaskSourceSortKey top_sort_key;
  {
    CheckedAutoLock current_thread_group_lock(lock_);
    new_priority_queue.swap(priority_queue_);
  }
  destination_thread_group->EnqueueAllTaskSources(&new_priority_queue);
}

void ThreadGroup::HandoffNonUserBlockingTaskSourcesToOtherThreadGroup(
    ThreadGroup* destination_thread_group) {
  PriorityQueue new_priority_queue;
  TaskSourceSortKey top_sort_key;
  {
    // This works because all USER_BLOCKING tasks are at the front of the queue.
    CheckedAutoLock current_thread_group_lock(lock_);
    while (!priority_queue_.IsEmpty() &&
           (top_sort_key = priority_queue_.PeekSortKey()).priority() ==
               TaskPriority::USER_BLOCKING) {
      new_priority_queue.Push(priority_queue_.PopTaskSource(), top_sort_key);
    }
    new_priority_queue.swap(priority_queue_);
  }
  destination_thread_group->EnqueueAllTaskSources(&new_priority_queue);
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

#if BUILDFLAG(IS_WIN)
// static
std::unique_ptr<win::ScopedWindowsThreadEnvironment>
ThreadGroup::GetScopedWindowsThreadEnvironment(WorkerEnvironment environment) {
  std::unique_ptr<win::ScopedWindowsThreadEnvironment> scoped_environment;
  if (environment == WorkerEnvironment::COM_MTA) {
    scoped_environment = std::make_unique<win::ScopedWinrtInitializer>();
  }
  // Continuing execution with an uninitialized apartment may lead to broken
  // program invariants later on.
  CHECK(!scoped_environment || scoped_environment->Succeeded());
  return scoped_environment;
}
#endif

// static
bool ThreadGroup::CurrentThreadHasGroup() {
  return current_thread_group != nullptr;
}

size_t ThreadGroup::GetMaxTasksForTesting() const {
  CheckedAutoLock auto_lock(lock_);
  return max_tasks_;
}

size_t ThreadGroup::GetMaxBestEffortTasksForTesting() const {
  CheckedAutoLock auto_lock(lock_);
  return max_best_effort_tasks_;
}

void ThreadGroup::WaitForWorkersIdleLockRequiredForTesting(size_t n) {
  // Make sure workers do not cleanup while watching the idle count.
  AutoReset<bool> ban_cleanups(&worker_cleanup_disallowed_for_testing_, true);

  while (NumberOfIdleWorkersLockRequiredForTesting() < n) {
    idle_workers_set_cv_for_testing_.Wait();
  }
}

void ThreadGroup::WaitForWorkersIdleForTesting(size_t n) {
  CheckedAutoLock auto_lock(lock_);

#if DCHECK_IS_ON()
  DCHECK(!some_workers_cleaned_up_for_testing_)
      << "Workers detached prior to waiting for a specific number of idle "
         "workers. Doing the wait under such conditions is flaky. Consider "
         "setting the suggested reclaim time to TimeDelta::Max() in Start().";
#endif

  WaitForWorkersIdleLockRequiredForTesting(n);
}

void ThreadGroup::WaitForAllWorkersIdleForTesting() {
  CheckedAutoLock auto_lock(lock_);
  WaitForWorkersIdleLockRequiredForTesting(workers_.size());
}

void ThreadGroup::WaitForWorkersCleanedUpForTesting(size_t n) {
  CheckedAutoLock auto_lock(lock_);

  if (!num_workers_cleaned_up_for_testing_cv_) {
    lock_.CreateConditionVariableAndEmplace(
        num_workers_cleaned_up_for_testing_cv_);
  }

  while (num_workers_cleaned_up_for_testing_ < n) {
    num_workers_cleaned_up_for_testing_cv_->Wait();
  }

  num_workers_cleaned_up_for_testing_ = 0;
}

size_t ThreadGroup::GetMaxConcurrentNonBlockedTasksDeprecated() const {
#if DCHECK_IS_ON()
  CheckedAutoLock auto_lock(lock_);
  DCHECK_NE(after_start().initial_max_tasks, 0U)
      << "GetMaxConcurrentTasksDeprecated() should only be called after the "
      << "thread group has started.";
#endif
  return after_start().initial_max_tasks;
}

size_t ThreadGroup::NumberOfWorkersForTesting() const {
  CheckedAutoLock auto_lock(lock_);
  return workers_.size();
}

size_t ThreadGroup::NumberOfIdleWorkersForTesting() const {
  CheckedAutoLock auto_lock(lock_);
  return NumberOfIdleWorkersLockRequiredForTesting();
}

size_t ThreadGroup::GetDesiredNumAwakeWorkersLockRequired() const {
  // Number of BEST_EFFORT task sources that are running or queued and allowed
  // to run by the CanRunPolicy.
  const size_t num_running_or_queued_can_run_best_effort_task_sources =
      num_running_best_effort_tasks_ +
      GetNumAdditionalWorkersForBestEffortTaskSourcesLockRequired();

  const size_t workers_for_best_effort_task_sources =
      std::max(std::min(num_running_or_queued_can_run_best_effort_task_sources,
                        max_best_effort_tasks_),
               num_running_best_effort_tasks_);

  // Number of USER_{VISIBLE|BLOCKING} task sources that are running or queued.
  const size_t num_running_or_queued_foreground_task_sources =
      (num_running_tasks_ - num_running_best_effort_tasks_) +
      GetNumAdditionalWorkersForForegroundTaskSourcesLockRequired();

  const size_t workers_for_foreground_task_sources =
      num_running_or_queued_foreground_task_sources;

  return std::min({workers_for_best_effort_task_sources +
                       workers_for_foreground_task_sources,
                   max_tasks_, kMaxNumberOfWorkers});
}

void ThreadGroup::MaybeScheduleAdjustMaxTasksLockRequired(
    BaseScopedCommandsExecutor* executor) {
  if (!adjust_max_tasks_posted_ &&
      ShouldPeriodicallyAdjustMaxTasksLockRequired()) {
    executor->ScheduleAdjustMaxTasks();
    adjust_max_tasks_posted_ = true;
  }
}

bool ThreadGroup::ShouldPeriodicallyAdjustMaxTasksLockRequired() {
  // AdjustMaxTasks() should be scheduled to periodically adjust |max_tasks_|
  // and |max_best_effort_tasks_| when (1) the concurrency limits are not large
  // enough to accommodate all queued and running task sources and an idle
  // worker and (2) there are unresolved MAY_BLOCK ScopedBlockingCalls.
  // - When (1) is false: No worker would be created or woken up if the
  //   concurrency limits were increased, so there is no hurry to increase them.
  // - When (2) is false: The concurrency limits could not be increased by
  //   AdjustMaxTasks().

  const size_t num_running_or_queued_best_effort_task_sources =
      num_running_best_effort_tasks_ +
      GetNumAdditionalWorkersForBestEffortTaskSourcesLockRequired();
  if (num_running_or_queued_best_effort_task_sources > max_best_effort_tasks_ &&
      num_unresolved_best_effort_may_block_ > 0) {
    return true;
  }

  const size_t num_running_or_queued_task_sources =
      num_running_tasks_ +
      GetNumAdditionalWorkersForBestEffortTaskSourcesLockRequired() +
      GetNumAdditionalWorkersForForegroundTaskSourcesLockRequired();
  constexpr size_t kIdleWorker = 1;
  return num_running_or_queued_task_sources + kIdleWorker > max_tasks_ &&
         num_unresolved_may_block_ > 0;
}

void ThreadGroup::UpdateMinAllowedPriorityLockRequired() {
  if (priority_queue_.IsEmpty() || num_running_tasks_ < max_tasks_) {
    max_allowed_sort_key_.store(kMaxYieldSortKey, std::memory_order_relaxed);
  } else {
    max_allowed_sort_key_.store({priority_queue_.PeekSortKey().priority(),
                                 priority_queue_.PeekSortKey().worker_count()},
                                std::memory_order_relaxed);
  }
}

void ThreadGroup::DecrementTasksRunningLockRequired(TaskPriority priority) {
  DCHECK_GT(num_running_tasks_, 0U);
  --num_running_tasks_;
  if (priority == TaskPriority::BEST_EFFORT) {
    DCHECK_GT(num_running_best_effort_tasks_, 0U);
    --num_running_best_effort_tasks_;
  }
  UpdateMinAllowedPriorityLockRequired();
}

void ThreadGroup::IncrementTasksRunningLockRequired(TaskPriority priority) {
  ++num_running_tasks_;
  DCHECK_LE(num_running_tasks_, max_tasks_);
  DCHECK_LE(num_running_tasks_, kMaxNumberOfWorkers);
  if (priority == TaskPriority::BEST_EFFORT) {
    ++num_running_best_effort_tasks_;
    DCHECK_LE(num_running_best_effort_tasks_, num_running_tasks_);
    DCHECK_LE(num_running_best_effort_tasks_, max_best_effort_tasks_);
  }
  UpdateMinAllowedPriorityLockRequired();
}

void ThreadGroup::DecrementMaxTasksLockRequired() {
  DCHECK_GT(num_running_tasks_, 0U);
  DCHECK_GT(max_tasks_, 0U);
  --max_tasks_;
  UpdateMinAllowedPriorityLockRequired();
}

void ThreadGroup::IncrementMaxTasksLockRequired() {
  DCHECK_GT(num_running_tasks_, 0U);
  ++max_tasks_;
  UpdateMinAllowedPriorityLockRequired();
}

void ThreadGroup::DecrementMaxBestEffortTasksLockRequired() {
  DCHECK_GT(num_running_tasks_, 0U);
  DCHECK_GT(max_best_effort_tasks_, 0U);
  --max_best_effort_tasks_;
  UpdateMinAllowedPriorityLockRequired();
}

void ThreadGroup::IncrementMaxBestEffortTasksLockRequired() {
  DCHECK_GT(num_running_tasks_, 0U);
  ++max_best_effort_tasks_;
  UpdateMinAllowedPriorityLockRequired();
}

ThreadGroup::InitializedInStart::InitializedInStart() = default;
ThreadGroup::InitializedInStart::~InitializedInStart() = default;

}  // namespace internal
}  // namespace base
