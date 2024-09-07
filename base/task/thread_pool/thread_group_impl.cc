// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group_impl.h"

#include <optional>
#include <string_view>

#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_token.h"
#include "base/strings/stringprintf.h"
#include "base/task/common/checked_lock.h"
#include "base/task/thread_pool/worker_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/thread_checker.h"
#include "base/time/time_override.h"
#include "base/trace_event/base_tracing.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace base {
namespace internal {

namespace {

constexpr size_t kMaxNumberOfWorkers = 256;

}  // namespace

// Upon destruction, executes actions that control the number of active workers.
// Useful to satisfy locking requirements of these actions.
class ThreadGroupImpl::ScopedCommandsExecutor
    : public ThreadGroup::BaseScopedCommandsExecutor {
 public:
  explicit ScopedCommandsExecutor(ThreadGroupImpl* outer)
      : BaseScopedCommandsExecutor(outer) {}

  ScopedCommandsExecutor(const ScopedCommandsExecutor&) = delete;
  ScopedCommandsExecutor& operator=(const ScopedCommandsExecutor&) = delete;
  ~ScopedCommandsExecutor() override {
    CheckedLock::AssertNoLockHeldOnCurrentThread();

    // Wake up workers.
    for (auto worker : workers_to_wake_up_) {
      worker->WakeUp();
    }
  }

  void ScheduleWakeUp(scoped_refptr<WorkerThread> worker) {
    workers_to_wake_up_.emplace_back(std::move(worker));
  }

 private:
  absl::InlinedVector<scoped_refptr<WorkerThread>, 2> workers_to_wake_up_;
};

class ThreadGroupImpl::WorkerDelegate : public WorkerThread::Delegate,
                                        public BlockingObserver {
 public:
  // |outer| owns the worker for which this delegate is constructed. If
  // |is_excess| is true, this worker will be eligible for reclaim.
  explicit WorkerDelegate(TrackedRef<ThreadGroupImpl> outer, bool is_excess);
  WorkerDelegate(const WorkerDelegate&) = delete;
  WorkerDelegate& operator=(const WorkerDelegate&) = delete;

  // WorkerThread::Delegate:
  void OnMainEntry(WorkerThread* worker) override;
  void OnMainExit(WorkerThread* worker) override;
  RegisteredTaskSource GetWork(WorkerThread* worker) override;
  RegisteredTaskSource SwapProcessedTask(RegisteredTaskSource task_source,
                                         WorkerThread* worker) override;
  void RecordUnnecessaryWakeup() override;
  TimeDelta GetSleepTimeout() override;

  // BlockingObserver:
  void BlockingStarted(BlockingType blocking_type) override;
  void BlockingTypeUpgraded() override;
  void BlockingEnded() override;

  // WorkerThread::Delegate:

  // Notifies the worker of shutdown, possibly marking the running task as
  // MAY_BLOCK.
  void OnShutdownStartedLockRequired(BaseScopedCommandsExecutor* executor)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Increments max [best effort] tasks iff this worker has been within a
  // ScopedBlockingCall for more than |may_block_threshold|.
  void MaybeIncrementMaxTasksLockRequired()
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Increments max [best effort] tasks.
  void IncrementMaxTasksLockRequired() EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  TaskPriority current_task_priority_lock_required() const
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
    return *read_any().current_task_priority;
  }

  // Exposed for AnnotateAcquiredLockAlias.
  const CheckedLock& lock() const LOCK_RETURNED(outer_->lock_) {
    return outer_->lock_;
  }

 private:
  // Returns true iff the worker can get work. Cleans up the worker or puts it
  // on the idle set if it can't get work.
  bool CanGetWorkLockRequired(BaseScopedCommandsExecutor* executor,
                              WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Calls cleanup on |worker| and removes it from the thread group. Called from
  // GetWork() when no work is available and CanCleanupLockRequired() returns
  // true.
  void CleanupLockRequired(BaseScopedCommandsExecutor* executor,
                           WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Called in GetWork() when a worker becomes idle.
  void OnWorkerBecomesIdleLockRequired(BaseScopedCommandsExecutor* executor,
                                       WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  RegisteredTaskSource GetWorkLockRequired(BaseScopedCommandsExecutor* executor,
                                           WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Returns true if |worker| is allowed to cleanup and remove itself from the
  // thread group. Called from GetWork() when no work is available.
  bool CanCleanupLockRequired(const WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Only used in DCHECKs.
  template <typename Worker>
  bool ContainsWorker(const std::vector<scoped_refptr<Worker>>& workers,
                      const WorkerThread* worker) {
    auto it = ranges::find_if(
        workers,
        [worker](const scoped_refptr<Worker>& i) { return i.get() == worker; });
    return it != workers.end();
  }

  // Accessed only from the worker thread.
  struct WorkerOnly {
    WorkerOnly();
    ~WorkerOnly();
    // Associated WorkerThread, if any, initialized in OnMainEntry().
    raw_ptr<WorkerThread> worker_thread_;

#if BUILDFLAG(IS_WIN)
    std::unique_ptr<win::ScopedWindowsThreadEnvironment> win_thread_environment;
#endif  // BUILDFLAG(IS_WIN)
  } worker_only_;

  // Writes from the worker thread protected by |outer_->lock_|. Reads from any
  // thread, protected by |outer_->lock_| when not on the worker thread.
  struct WriteWorkerReadAny {
    // The priority of the task the worker is currently running if any.
    std::optional<TaskPriority> current_task_priority;
    // The shutdown behavior of the task the worker is currently running if any.
    std::optional<TaskShutdownBehavior> current_shutdown_behavior;

    // Time when MayBlockScopeEntered() was last called. Reset when
    // BlockingScopeExited() is called.
    TimeTicks blocking_start_time;

    // Whether the worker is currently running a task (i.e. GetWork() has
    // returned a non-empty task source and DidProcessTask() hasn't been called
    // yet).
    bool is_running_task() const { return !!current_shutdown_behavior; }
  } write_worker_read_any_;

  WorkerOnly& worker_only() {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return worker_only_;
  }

  WriteWorkerReadAny& write_worker() EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return write_worker_read_any_;
  }

  const WriteWorkerReadAny& read_any() const
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
    return write_worker_read_any_;
  }

  const WriteWorkerReadAny& read_worker() const {
    DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
    return write_worker_read_any_;
  }

  const TrackedRef<ThreadGroupImpl> outer_;

  // Whether the worker is in excess. This must be decided at worker creation
  // time to prevent unnecessarily discarding TLS state, as well as any behavior
  // the OS has learned about a given thread.
  const bool is_excess_;

  // Whether |outer_->max_tasks_|/|outer_->max_best_effort_tasks_| were
  // incremented due to a ScopedBlockingCall on the thread.
  bool incremented_max_tasks_since_blocked_ GUARDED_BY(outer_->lock_) = false;
  bool incremented_max_best_effort_tasks_since_blocked_
      GUARDED_BY(outer_->lock_) = false;
  // Whether |outer_->max_tasks_| and |outer_->max_best_effort_tasks_| was
  // incremented due to running CONTINUE_ON_SHUTDOWN on the thread during
  // shutdown.
  bool incremented_max_tasks_for_shutdown_ GUARDED_BY(outer_->lock_) = false;

  // Verifies that specific calls are always made from the worker thread.
  THREAD_CHECKER(worker_thread_checker_);
};

ThreadGroupImpl::ThreadGroupImpl(std::string_view histogram_label,
                                 std::string_view thread_group_label,
                                 ThreadType thread_type_hint,
                                 TrackedRef<TaskTracker> task_tracker,
                                 TrackedRef<Delegate> delegate)
    : ThreadGroup(histogram_label,
                  thread_group_label,
                  thread_type_hint,
                  std::move(task_tracker),
                  std::move(delegate)),
      tracked_ref_factory_(this) {
  DCHECK(!thread_group_label_.empty());
}

void ThreadGroupImpl::Start(
    size_t max_tasks,
    size_t max_best_effort_tasks,
    TimeDelta suggested_reclaim_time,
    scoped_refptr<SingleThreadTaskRunner> service_thread_task_runner,
    WorkerThreadObserver* worker_thread_observer,
    WorkerEnvironment worker_environment,
    bool synchronous_thread_start_for_testing,
    std::optional<TimeDelta> may_block_threshold) {
  ThreadGroup::StartImpl(
      max_tasks, max_best_effort_tasks, suggested_reclaim_time,
      service_thread_task_runner, worker_thread_observer, worker_environment,
      synchronous_thread_start_for_testing, may_block_threshold);

  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  DCHECK(workers_.empty());
  EnsureEnoughWorkersLockRequired(&executor);
}

ThreadGroupImpl::~ThreadGroupImpl() {
  // ThreadGroup should only ever be deleted:
  //  1) In tests, after JoinForTesting().
  //  2) In production, iff initialization failed.
  // In both cases |workers_| should be empty.
  DCHECK(workers_.empty());
}

void ThreadGroupImpl::UpdateSortKey(TaskSource::Transaction transaction) {
  ScopedCommandsExecutor executor(this);
  UpdateSortKeyImpl(&executor, std::move(transaction));
}

void ThreadGroupImpl::PushTaskSourceAndWakeUpWorkers(
    RegisteredTaskSourceAndTransaction transaction_with_task_source) {
  ScopedCommandsExecutor executor(this);
  PushTaskSourceAndWakeUpWorkersImpl(&executor,
                                     std::move(transaction_with_task_source));
}

ThreadGroupImpl::WorkerDelegate::WorkerDelegate(
    TrackedRef<ThreadGroupImpl> outer,
    bool is_excess)
    : outer_(outer), is_excess_(is_excess) {
  // Bound in OnMainEntry().
  DETACH_FROM_THREAD(worker_thread_checker_);
}

ThreadGroupImpl::WorkerDelegate::WorkerOnly::WorkerOnly() = default;
ThreadGroupImpl::WorkerDelegate::WorkerOnly::~WorkerOnly() = default;

TimeDelta ThreadGroupImpl::WorkerDelegate::GetSleepTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  if (!is_excess_) {
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

void ThreadGroupImpl::WorkerDelegate::OnMainEntry(WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  {
#if DCHECK_IS_ON()
    CheckedAutoLock auto_lock(outer_->lock_);
    DCHECK(
        ContainsWorker(outer_->workers_, static_cast<WorkerThread*>(worker)));
#endif
  }

#if BUILDFLAG(IS_WIN)
  worker_only().win_thread_environment = GetScopedWindowsThreadEnvironment(
      outer_->after_start().worker_environment);
#endif  // BUILDFLAG(IS_WIN)

  PlatformThread::SetName(
      StringPrintf("ThreadPool%sWorker", outer_->thread_group_label_.c_str()));

  outer_->BindToCurrentThread();
  worker_only().worker_thread_ = static_cast<WorkerThread*>(worker);
  SetBlockingObserverForCurrentThread(this);

  if (outer_->worker_started_for_testing_) {
    // When |worker_started_for_testing_| is set, the thread that starts workers
    // should wait for a worker to have started before starting the next one,
    // and there should only be one thread that wakes up workers at a time.
    DCHECK(!outer_->worker_started_for_testing_->IsSignaled());
    outer_->worker_started_for_testing_->Signal();
  }
}

void ThreadGroupImpl::WorkerDelegate::OnMainExit(WorkerThread* worker_base) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

#if DCHECK_IS_ON()
  WorkerThread* worker = static_cast<WorkerThread*>(worker_base);
  {
    bool shutdown_complete = outer_->task_tracker_->IsShutdownComplete();
    CheckedAutoLock auto_lock(outer_->lock_);

    // |worker| should already have been removed from the idle workers set and
    // |workers_| by the time the thread is about to exit. (except in the
    // cases where the thread group is no longer going to be used - in which
    // case, it's fine for there to be invalid workers in the thread group).
    if (!shutdown_complete && !outer_->join_for_testing_started_) {
      DCHECK(!outer_->idle_workers_set_.Contains(worker));
      DCHECK(!ContainsWorker(outer_->workers_, worker));
    }
  }
#endif

#if BUILDFLAG(IS_WIN)
  worker_only().win_thread_environment.reset();
#endif  // BUILDFLAG(IS_WIN)

  // Count cleaned up workers for tests. It's important to do this here
  // instead of at the end of CleanupLockRequired() because some side-effects
  // of cleaning up happen outside the lock (e.g. recording histograms) and
  // resuming from tests must happen-after that point or checks on the main
  // thread will be flaky (crbug.com/1047733).
  CheckedAutoLock auto_lock(outer_->lock_);
  ++outer_->num_workers_cleaned_up_for_testing_;
#if DCHECK_IS_ON()
  outer_->some_workers_cleaned_up_for_testing_ = true;
#endif
  if (outer_->num_workers_cleaned_up_for_testing_cv_) {
    outer_->num_workers_cleaned_up_for_testing_cv_->Signal();
  }
}

bool ThreadGroupImpl::WorkerDelegate::CanGetWorkLockRequired(
    BaseScopedCommandsExecutor* executor,
    WorkerThread* worker_base) {
  WorkerThread* worker = static_cast<WorkerThread*>(worker_base);

  const bool is_on_idle_workers_set = outer_->IsOnIdleSetLockRequired(worker);
  DCHECK_EQ(is_on_idle_workers_set, outer_->idle_workers_set_.Contains(worker));

  // This occurs when the when WorkerThread::Delegate::WaitForWork() times out
  // (i.e. when the worker's wakes up after GetSleepTimeout()).
  if (is_on_idle_workers_set) {
    if (CanCleanupLockRequired(worker)) {
      CleanupLockRequired(executor, worker);
    }
    return false;
  }

  // If too many workers are running, this worker should not get work, until
  // tasks are no longer in excess (i.e. max tasks increases). This ensures that
  // if this worker is in excess, it gets a chance to being cleaned up.
  if (outer_->GetNumAwakeWorkersLockRequired() > outer_->max_tasks_) {
    OnWorkerBecomesIdleLockRequired(executor, worker);
    return false;
  }

  return true;
}

RegisteredTaskSource ThreadGroupImpl::WorkerDelegate::GetWork(
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!read_worker().current_task_priority);
  DCHECK(!read_worker().current_shutdown_behavior);

  ScopedCommandsExecutor executor(outer_.get());
  CheckedAutoLock auto_lock(outer_->lock_);
  return GetWorkLockRequired(&executor, worker);
}

RegisteredTaskSource ThreadGroupImpl::WorkerDelegate::GetWorkLockRequired(
    BaseScopedCommandsExecutor* executor,
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(ContainsWorker(outer_->workers_, worker));

  if (!CanGetWorkLockRequired(executor, worker)) {
    return nullptr;
  }

  RegisteredTaskSource task_source;
  TaskPriority priority;
  while (!task_source && !outer_->priority_queue_.IsEmpty()) {
    // Enforce the CanRunPolicy and that no more than |max_best_effort_tasks_|
    // BEST_EFFORT tasks run concurrently.
    priority = outer_->priority_queue_.PeekSortKey().priority();
    if (!outer_->task_tracker_->CanRunPriority(priority) ||
        (priority == TaskPriority::BEST_EFFORT &&
         outer_->num_running_best_effort_tasks_ >=
             outer_->max_best_effort_tasks_)) {
      break;
    }

    task_source = outer_->TakeRegisteredTaskSource(executor);
  }
  if (!task_source) {
    OnWorkerBecomesIdleLockRequired(executor, worker);
    return nullptr;
  }

  // Running task bookkeeping.
  outer_->IncrementTasksRunningLockRequired(priority);

  write_worker().current_task_priority = priority;
  write_worker().current_shutdown_behavior = task_source->shutdown_behavior();

  // Subtle: This must be after the call to WillRunTask() inside
  // TakeRegisteredTaskSource(), so that any state used by WillRunTask() to
  // determine that the task source must remain in the TaskQueue is also used
  // to determine the desired number of workers. Concretely, this wouldn't
  // work:
  //
  //   Thread 1: GetWork() calls EnsureEnoughWorkers(). No worker woken up
  //             because the queue contains a job with max concurrency = 1 and
  //             the current worker is awake.
  //   Thread 2: Increases the job's max concurrency.
  //             ShouldQueueUponCapacityIncrease() returns false because the
  //             job is already queued.
  //   Thread 1: Calls WillRunTask() on the job. It returns
  //             kAllowedNotSaturated because max concurrency is not reached.
  //             But no extra worker is woken up to run the job!
  outer_->EnsureEnoughWorkersLockRequired(executor);

  return task_source;
}

RegisteredTaskSource ThreadGroupImpl::WorkerDelegate::SwapProcessedTask(
    RegisteredTaskSource task_source,
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(read_worker().current_task_priority);
  DCHECK(read_worker().current_shutdown_behavior);

  // A transaction to the TaskSource to reenqueue, if any. Instantiated here as
  // |TaskSource::lock_| is a UniversalPredecessor and must always be acquired
  // prior to acquiring a second lock
  std::optional<RegisteredTaskSourceAndTransaction>
      transaction_with_task_source;
  if (task_source) {
    transaction_with_task_source.emplace(
        RegisteredTaskSourceAndTransaction::FromTaskSource(
            std::move(task_source)));
  }

  // Calling WakeUp() guarantees that this WorkerThread will run Tasks from
  // TaskSources returned by the GetWork() method of |delegate_| until it
  // returns nullptr. Resetting |wake_up_event_| here doesn't break this
  // invariant and avoids a useless loop iteration before going to sleep if
  // WakeUp() is called while this WorkerThread is awake.
  wake_up_event_.Reset();

  ScopedCommandsExecutor workers_executor(outer_.get());
  ScopedReenqueueExecutor reenqueue_executor;
  CheckedAutoLock auto_lock(outer_->lock_);

  // During shutdown, max_tasks may have been incremented in
  // OnShutdownStartedLockRequired().
  if (incremented_max_tasks_for_shutdown_) {
    DCHECK(outer_->shutdown_started_);
    outer_->DecrementMaxTasksLockRequired();
    if (*read_worker().current_task_priority == TaskPriority::BEST_EFFORT) {
      outer_->DecrementMaxBestEffortTasksLockRequired();
    }
    incremented_max_tasks_since_blocked_ = false;
    incremented_max_best_effort_tasks_since_blocked_ = false;
    incremented_max_tasks_for_shutdown_ = false;
  }

  DCHECK(read_worker().blocking_start_time.is_null());
  DCHECK(!incremented_max_tasks_since_blocked_);
  DCHECK(!incremented_max_best_effort_tasks_since_blocked_);

  // Running task bookkeeping.
  outer_->DecrementTasksRunningLockRequired(
      *read_worker().current_task_priority);
  write_worker().current_shutdown_behavior = std::nullopt;
  write_worker().current_task_priority = std::nullopt;

  if (transaction_with_task_source) {
    outer_->ReEnqueueTaskSourceLockRequired(
        &workers_executor, &reenqueue_executor,
        std::move(transaction_with_task_source.value()));
  }

  return GetWorkLockRequired(&workers_executor,
                             static_cast<WorkerThread*>(worker));
}

bool ThreadGroupImpl::WorkerDelegate::CanCleanupLockRequired(
    const WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  if (!is_excess_) {
    return false;
  }

  const TimeTicks last_used_time = worker->GetLastUsedTime();
  if (last_used_time.is_null() ||
      subtle::TimeTicksNowIgnoringOverride() - last_used_time <
          outer_->after_start().suggested_reclaim_time) {
    return false;
  }
  if (!outer_->worker_cleanup_disallowed_for_testing_) [[likely]] {
    return true;
  }
  return false;
}

void ThreadGroupImpl::WorkerDelegate::CleanupLockRequired(
    BaseScopedCommandsExecutor* executor,
    WorkerThread* worker_base) {
  WorkerThread* worker = static_cast<WorkerThread*>(worker_base);
  DCHECK(!outer_->join_for_testing_started_);
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  worker->Cleanup();

  if (outer_->IsOnIdleSetLockRequired(worker)) {
    outer_->idle_workers_set_.Remove(worker);
  }

  // Remove the worker from |workers_|.
  auto worker_iter = ranges::find(outer_->workers_, worker);
  CHECK(worker_iter != outer_->workers_.end(), base::NotFatalUntil::M125);
  outer_->workers_.erase(worker_iter);
}

void ThreadGroupImpl::WorkerDelegate::OnWorkerBecomesIdleLockRequired(
    BaseScopedCommandsExecutor* executor,
    WorkerThread* worker_base) {
  WorkerThread* worker = static_cast<WorkerThread*>(worker_base);

  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!outer_->idle_workers_set_.Contains(worker));

  // Add the worker to the idle set.
  outer_->idle_workers_set_.Insert(worker);
  DCHECK_LE(outer_->idle_workers_set_.Size(), outer_->workers_.size());
  outer_->idle_workers_set_cv_for_testing_.Broadcast();
}

void ThreadGroupImpl::WorkerDelegate::RecordUnnecessaryWakeup() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  base::BooleanHistogram::FactoryGet(
      std::string("ThreadPool.UnnecessaryWakeup.") + outer_->histogram_label_,
      base::Histogram::kUmaTargetedHistogramFlag)
      ->Add(true);

  TRACE_EVENT_INSTANT("wakeup.flow", "ThreadPool.UnnecessaryWakeup");
}

void ThreadGroupImpl::WorkerDelegate::BlockingStarted(
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

  ScopedCommandsExecutor executor(outer_.get());
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
    outer_->EnsureEnoughWorkersLockRequired(&executor);
  } else {
    ++outer_->num_unresolved_may_block_;
  }

  outer_->MaybeScheduleAdjustMaxTasksLockRequired(&executor);
}

void ThreadGroupImpl::WorkerDelegate::BlockingTypeUpgraded() {
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

  ScopedCommandsExecutor executor(outer_.get());
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
  outer_->EnsureEnoughWorkersLockRequired(&executor);
}

void ThreadGroupImpl::WorkerDelegate::BlockingEnded() {
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

// BlockingObserver:
// Notifies the worker of shutdown, possibly marking the running task as
// MAY_BLOCK.
void ThreadGroupImpl::WorkerDelegate::OnShutdownStartedLockRequired(
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
void ThreadGroupImpl::WorkerDelegate::MaybeIncrementMaxTasksLockRequired()
    EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_) {
  if (read_any().blocking_start_time.is_null() ||
      subtle::TimeTicksNowIgnoringOverride() - read_any().blocking_start_time <
          outer_->after_start().may_block_threshold) {
    return;
  }
  IncrementMaxTasksLockRequired();
}

// Increments max [best effort] tasks.
void ThreadGroupImpl::WorkerDelegate::IncrementMaxTasksLockRequired()
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

void ThreadGroupImpl::JoinForTesting() {
  decltype(workers_) workers_copy;
  {
    CheckedAutoLock auto_lock(lock_);
    priority_queue_.EnableFlushTaskSourcesOnDestroyForTesting();

    DCHECK_GT(workers_.size(), size_t(0))
        << "Joined an unstarted thread group.";

    join_for_testing_started_ = true;

    // Ensure WorkerThreads in |workers_| do not attempt to cleanup while
    // being joined.
    worker_cleanup_disallowed_for_testing_ = true;

    // Make a copy of the WorkerThreads so that we can call
    // WorkerThread::JoinForTesting() without holding |lock_| since
    // WorkerThreads may need to access |workers_|.
    workers_copy = workers_;
  }
  for (const auto& worker : workers_copy) {
    static_cast<WorkerThread*>(worker.get())->JoinForTesting();
  }

  CheckedAutoLock auto_lock(lock_);
  DCHECK(workers_ == workers_copy);
  // Release |workers_| to clear their TrackedRef against |this|.
  workers_.clear();
}

size_t ThreadGroupImpl::NumberOfIdleWorkersLockRequiredForTesting() const {
  return idle_workers_set_.Size();
}

void ThreadGroupImpl::MaintainAtLeastOneIdleWorkerLockRequired(
    ScopedCommandsExecutor* executor) {
  if (workers_.size() == kMaxNumberOfWorkers) {
    return;
  }
  DCHECK_LT(workers_.size(), kMaxNumberOfWorkers);

  if (!idle_workers_set_.IsEmpty()) {
    return;
  }

  if (workers_.size() >= max_tasks_) {
    return;
  }

  scoped_refptr<WorkerThread> new_worker =
      CreateAndRegisterWorkerLockRequired(executor);
  DCHECK(new_worker);
  idle_workers_set_.Insert(new_worker.get());
}

scoped_refptr<WorkerThread>
ThreadGroupImpl::CreateAndRegisterWorkerLockRequired(
    ScopedCommandsExecutor* executor) {
  DCHECK(!join_for_testing_started_);
  DCHECK_LT(workers_.size(), max_tasks_);
  DCHECK_LT(workers_.size(), kMaxNumberOfWorkers);
  DCHECK(idle_workers_set_.IsEmpty());

  // WorkerThread needs |lock_| as a predecessor for its thread lock because in
  // GetWork(), |lock_| is first acquired and then the thread lock is acquired
  // when GetLastUsedTime() is called on the worker by CanGetWorkLockRequired().
  scoped_refptr<WorkerThread> worker = MakeRefCounted<WorkerThread>(
      thread_type_hint_,
      std::make_unique<WorkerDelegate>(
          tracked_ref_factory_.GetTrackedRef(),
          /* is_excess=*/after_start().no_worker_reclaim
              ? workers_.size() >= after_start().initial_max_tasks
              : true),
      task_tracker_, worker_sequence_num_++, &lock_);

  workers_.push_back(worker);
  executor->ScheduleStart(worker);
  DCHECK_LE(workers_.size(), max_tasks_);

  return worker;
}

size_t ThreadGroupImpl::GetNumAwakeWorkersLockRequired() const {
  DCHECK_GE(workers_.size(), idle_workers_set_.Size());
  size_t num_awake_workers = workers_.size() - idle_workers_set_.Size();
  DCHECK_GE(num_awake_workers, num_running_tasks_);
  return num_awake_workers;
}

void ThreadGroupImpl::DidUpdateCanRunPolicy() {
  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  EnsureEnoughWorkersLockRequired(&executor);
}

void ThreadGroupImpl::OnShutdownStarted() {
  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);

  // Don't do anything if the thread group isn't started.
  if (max_tasks_ == 0) {
    return;
  }
  if (join_for_testing_started_) [[unlikely]] {
    return;
  }

  // Start a MAY_BLOCK scope on each worker that is already running a task.
  for (scoped_refptr<WorkerThread>& worker : workers_) {
    // The delegates of workers inside a ThreadGroupImpl should be
    // `WorkerDelegate`s.
    WorkerDelegate* delegate = static_cast<WorkerDelegate*>(worker->delegate());
    AnnotateAcquiredLockAlias annotate(lock_, delegate->lock());
    delegate->OnShutdownStartedLockRequired(&executor);
  }
  EnsureEnoughWorkersLockRequired(&executor);

  shutdown_started_ = true;
}

void ThreadGroupImpl::EnsureEnoughWorkersLockRequired(
    BaseScopedCommandsExecutor* base_executor) {
  // Don't do anything if the thread group isn't started.
  if (max_tasks_ == 0) {
    return;
  }
  if (join_for_testing_started_) [[unlikely]] {
    return;
  }

  ScopedCommandsExecutor* executor =
      static_cast<ScopedCommandsExecutor*>(base_executor);

  const size_t desired_num_awake_workers =
      GetDesiredNumAwakeWorkersLockRequired();
  const size_t num_awake_workers = GetNumAwakeWorkersLockRequired();

  size_t num_workers_to_wake_up =
      ClampSub(desired_num_awake_workers, num_awake_workers);
  num_workers_to_wake_up = std::min(num_workers_to_wake_up, size_t(2U));

  // Wake up the appropriate number of workers.
  for (size_t i = 0; i < num_workers_to_wake_up; ++i) {
    MaintainAtLeastOneIdleWorkerLockRequired(executor);
    WorkerThread* worker_to_wakeup = idle_workers_set_.Take();
    DCHECK(worker_to_wakeup);
    executor->ScheduleWakeUp(worker_to_wakeup);
  }

  // In the case where the loop above didn't wake up any worker and we don't
  // have excess workers, the idle worker should be maintained. This happens
  // when called from the last worker awake, or a recent increase in |max_tasks|
  // now makes it possible to keep an idle worker.
  if (desired_num_awake_workers == num_awake_workers) {
    MaintainAtLeastOneIdleWorkerLockRequired(executor);
  }

  // This function is called every time a task source is (re-)enqueued,
  // hence the minimum priority needs to be updated.
  UpdateMinAllowedPriorityLockRequired();

  // Ensure that the number of workers is periodically adjusted if needed.
  MaybeScheduleAdjustMaxTasksLockRequired(executor);
}

bool ThreadGroupImpl::IsOnIdleSetLockRequired(WorkerThread* worker) const {
  // To avoid searching through the idle set : use GetLastUsedTime() not being
  // null (or being directly on top of the idle set) as a proxy for being on
  // the idle set.
  return idle_workers_set_.Peek() == worker ||
         !worker->GetLastUsedTime().is_null();
}

void ThreadGroupImpl::ScheduleAdjustMaxTasks() {
  // |adjust_max_tasks_posted_| can't change before the task posted below runs.
  // Skip check on NaCl to avoid unsafe reference acquisition warning.
#if !BUILDFLAG(IS_NACL)
  DCHECK(TS_UNCHECKED_READ(adjust_max_tasks_posted_));
#endif

  after_start().service_thread_task_runner->PostDelayedTask(
      FROM_HERE, BindOnce(&ThreadGroupImpl::AdjustMaxTasks, Unretained(this)),
      after_start().blocked_workers_poll_period);
}

void ThreadGroupImpl::AdjustMaxTasks() {
  DCHECK(
      after_start().service_thread_task_runner->RunsTasksInCurrentSequence());

  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  DCHECK(adjust_max_tasks_posted_);
  adjust_max_tasks_posted_ = false;

  // Increment max tasks for each worker that has been within a MAY_BLOCK
  // ScopedBlockingCall for more than may_block_threshold.
  for (scoped_refptr<WorkerThread> worker : workers_) {
    // The delegates of workers inside a ThreadGroupImpl should be
    // `WorkerDelegate`s.
    WorkerDelegate* delegate = static_cast<WorkerDelegate*>(worker->delegate());
    AnnotateAcquiredLockAlias annotate(lock_, delegate->lock());
    delegate->MaybeIncrementMaxTasksLockRequired();
  }

  // Wake up workers according to the updated |max_tasks_|. This will also
  // reschedule AdjustMaxTasks() if necessary.
  EnsureEnoughWorkersLockRequired(&executor);
}

}  // namespace internal
}  // namespace base
