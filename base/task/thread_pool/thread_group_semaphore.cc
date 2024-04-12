// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group_semaphore.h"

#include <algorithm>
#include <string_view>

#include "base/metrics/histogram_macros.h"
#include "base/sequence_token.h"
#include "base/strings/stringprintf.h"
#include "base/task/common/checked_lock.h"
#include "base/task/thread_pool/thread_group.h"
#include "base/task/thread_pool/worker_thread_semaphore.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/thread_checker.h"
#include "base/time/time_override.h"
#include "base/trace_event/base_tracing.h"

namespace base {
namespace internal {

namespace {
constexpr size_t kMaxNumberOfWorkers = 256;
}  // namespace

// Upon destruction, executes actions that control the number of active workers.
// Useful to satisfy locking requirements of these actions.
class ThreadGroupSemaphore::SemaphoreScopedCommandsExecutor
    : public ThreadGroup::BaseScopedCommandsExecutor {
 public:
  explicit SemaphoreScopedCommandsExecutor(ThreadGroupSemaphore* outer)
      : BaseScopedCommandsExecutor(outer) {}

  SemaphoreScopedCommandsExecutor(const SemaphoreScopedCommandsExecutor&) =
      delete;
  SemaphoreScopedCommandsExecutor& operator=(
      const SemaphoreScopedCommandsExecutor&) = delete;
  ~SemaphoreScopedCommandsExecutor() override {
    CheckedLock::AssertNoLockHeldOnCurrentThread();
    for (int i = 0; i < semaphore_signal_count_; ++i) {
      TRACE_EVENT_INSTANT("wakeup.flow", "WorkerThreadSemaphore::Signal",
                          perfetto::Flow::FromPointer(&outer()->semaphore_));
      outer()->semaphore_.Signal();
    }
  }

  void ScheduleSignal() EXCLUSIVE_LOCKS_REQUIRED(outer()->lock_) {
    ++semaphore_signal_count_;
    ++outer()->num_active_signals_;
  }

 private:
  friend class ThreadGroupSemaphore;

  ThreadGroupSemaphore* outer() {
    return static_cast<ThreadGroupSemaphore*>(outer_);
  }

  int semaphore_signal_count_ = 0;
};

class ThreadGroupSemaphore::SemaphoreWorkerDelegate
    : public ThreadGroup::ThreadGroupWorkerDelegate,
      public WorkerThreadSemaphore::Delegate {
 public:
  // `outer` owns the worker for which this delegate is
  // constructed. `join_called_for_testing` is shared amongst workers, and
  // owned by `outer`.
  SemaphoreWorkerDelegate(TrackedRef<ThreadGroup> outer,
                          bool is_excess,
                          AtomicFlag* join_called_for_testing);
  SemaphoreWorkerDelegate(const SemaphoreWorkerDelegate&) = delete;
  SemaphoreWorkerDelegate& operator=(const SemaphoreWorkerDelegate&) = delete;

  // OnMainExit() handles the thread-affine cleanup;
  // SemaphoreWorkerDelegate can thereafter safely be deleted from any thread.
  ~SemaphoreWorkerDelegate() override = default;

  // WorkerThread::Delegate:
  void OnMainEntry(WorkerThread* worker) override;
  void OnMainExit(WorkerThread* worker) override;
  RegisteredTaskSource GetWork(WorkerThread* worker) override;
  RegisteredTaskSource SwapProcessedTask(RegisteredTaskSource task_source,
                                         WorkerThread* worker) override;
  void RecordUnnecessaryWakeup() override;
  TimeDelta GetSleepTimeout() override;

 private:
  const ThreadGroupSemaphore* outer() const {
    return static_cast<ThreadGroupSemaphore*>(outer_.get());
  }
  ThreadGroupSemaphore* outer() {
    return static_cast<ThreadGroupSemaphore*>(outer_.get());
  }

  // ThreadGroup::ThreadGroupWorkerDelegate:
  bool CanGetWorkLockRequired(BaseScopedCommandsExecutor* executor,
                              WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer()->lock_) override;
  void CleanupLockRequired(BaseScopedCommandsExecutor* executor,
                           WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer()->lock_) override;
  void OnWorkerBecomesIdleLockRequired(BaseScopedCommandsExecutor* executor,
                                       WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer()->lock_) override;

  // Returns true if `worker` is allowed to cleanup and remove itself from the
  // thread group. Called from GetWork() when no work is available.
  bool CanCleanupLockRequired(const WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer()->lock_) override;
};

std::unique_ptr<ThreadGroup::BaseScopedCommandsExecutor>
ThreadGroupSemaphore::GetExecutor() {
  return std::make_unique<SemaphoreScopedCommandsExecutor>(this);
}

ThreadGroupSemaphore::ThreadGroupSemaphore(std::string_view histogram_label,
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

void ThreadGroupSemaphore::Start(
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

  SemaphoreScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  DCHECK(workers_.empty());
  EnsureEnoughWorkersLockRequired(&executor);
}

ThreadGroupSemaphore::~ThreadGroupSemaphore() {
  // ThreadGroup should only ever be deleted:
  //  1) In tests, after JoinForTesting().
  //  2) In production, iff initialization failed.
  // In both cases `workers_` should be empty.
  DCHECK(workers_.empty());
}

void ThreadGroupSemaphore::UpdateSortKey(TaskSource::Transaction transaction) {
  SemaphoreScopedCommandsExecutor executor(this);
  UpdateSortKeyImpl(&executor, std::move(transaction));
}

void ThreadGroupSemaphore::PushTaskSourceAndWakeUpWorkers(
    RegisteredTaskSourceAndTransaction transaction_with_task_source) {
  SemaphoreScopedCommandsExecutor executor(this);
  PushTaskSourceAndWakeUpWorkersImpl(&executor,
                                     std::move(transaction_with_task_source));
}

size_t ThreadGroupSemaphore::NumberOfIdleWorkersLockRequiredForTesting() const {
  return ClampSub(workers_.size(), num_active_signals_);
}

ThreadGroupSemaphore::SemaphoreWorkerDelegate::SemaphoreWorkerDelegate(
    TrackedRef<ThreadGroup> outer,
    bool is_excess,
    AtomicFlag* join_called_for_testing)
    : ThreadGroupWorkerDelegate(std::move(outer), is_excess),
      WorkerThreadSemaphore::Delegate(
          &static_cast<ThreadGroupSemaphore*>(outer.get())->semaphore_,
          join_called_for_testing) {}

void ThreadGroupSemaphore::SemaphoreWorkerDelegate::OnMainEntry(
    WorkerThread* worker) {
  OnMainEntryImpl(worker);
}

void ThreadGroupSemaphore::SemaphoreWorkerDelegate::OnMainExit(
    WorkerThread* worker_base) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

#if DCHECK_IS_ON()
  WorkerThreadSemaphore* worker =
      static_cast<WorkerThreadSemaphore*>(worker_base);
  {
    bool shutdown_complete = outer()->task_tracker_->IsShutdownComplete();
    CheckedAutoLock auto_lock(outer()->lock_);

    // `worker` should already have been removed from `workers_` by the time the
    // thread is about to exit. (except in the cases where the thread group is
    // no longer going to be used - in which case, it's fine for there to be
    // invalid workers in the thread group).
    if (!shutdown_complete && !outer()->join_called_for_testing_.IsSet()) {
      DCHECK(!ContainsWorker(outer()->workers_, worker));
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
  CheckedAutoLock auto_lock(outer()->lock_);
  ++outer()->num_workers_cleaned_up_for_testing_;
#if DCHECK_IS_ON()
  outer()->some_workers_cleaned_up_for_testing_ = true;
#endif
  if (outer()->num_workers_cleaned_up_for_testing_cv_) {
    outer()->num_workers_cleaned_up_for_testing_cv_->Signal();
  }
}

bool ThreadGroupSemaphore::SemaphoreWorkerDelegate::CanGetWorkLockRequired(
    BaseScopedCommandsExecutor* executor,
    WorkerThread* worker_base) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  WorkerThreadSemaphore* worker =
      static_cast<WorkerThreadSemaphore*>(worker_base);

  AnnotateAcquiredLockAlias annotate(outer()->lock_, lock());
  // `timed_out_` is set by TimedWait().
  if (timed_out_) {
    if (CanCleanupLockRequired(worker)) {
      CleanupLockRequired(executor, worker);
    }
    return false;
  }

  // If too many workers are currently awake (contrasted with ThreadGroupImpl
  // where this decision is made by the number of workers which were signaled),
  // this worker should not get work, until tasks are no longer in excess
  // (i.e. max tasks increases).
  if (outer()->num_active_signals_ > outer()->max_tasks_) {
    OnWorkerBecomesIdleLockRequired(executor, worker);
    return false;
  }
  return true;
}

RegisteredTaskSource ThreadGroupSemaphore::SemaphoreWorkerDelegate::GetWork(
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!read_worker().current_task_priority);
  DCHECK(!read_worker().current_shutdown_behavior);

  SemaphoreScopedCommandsExecutor executor(outer());
  CheckedAutoLock auto_lock(outer()->lock_);
  AnnotateAcquiredLockAlias alias(
      outer()->lock_, static_cast<ThreadGroupSemaphore*>(outer_.get())->lock_);

  return GetWorkLockRequired(&executor, worker);
}

RegisteredTaskSource
ThreadGroupSemaphore::SemaphoreWorkerDelegate::SwapProcessedTask(
    RegisteredTaskSource task_source,
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(read_worker().current_task_priority);
  DCHECK(read_worker().current_shutdown_behavior);

  // A transaction to the TaskSource to reenqueue, if any. Instantiated here as
  // `TaskSource::lock_` is a UniversalPredecessor and must always be acquired
  // prior to acquiring a second lock
  std::optional<RegisteredTaskSourceAndTransaction>
      transaction_with_task_source;
  if (task_source) {
    transaction_with_task_source.emplace(
        RegisteredTaskSourceAndTransaction::FromTaskSource(
            std::move(task_source)));
  }

  SemaphoreScopedCommandsExecutor workers_executor(outer());
  ScopedReenqueueExecutor reenqueue_executor;
  CheckedAutoLock auto_lock(outer()->lock_);
  AnnotateAcquiredLockAlias annotate(outer()->lock_, lock());

  // During shutdown, max_tasks may have been incremented in
  // OnShutdownStartedLockRequired().
  if (incremented_max_tasks_for_shutdown_) {
    DCHECK(outer()->shutdown_started_);
    outer()->DecrementMaxTasksLockRequired();
    if (*read_worker().current_task_priority == TaskPriority::BEST_EFFORT) {
      outer()->DecrementMaxBestEffortTasksLockRequired();
    }
    incremented_max_tasks_since_blocked_ = false;
    incremented_max_best_effort_tasks_since_blocked_ = false;
    incremented_max_tasks_for_shutdown_ = false;
  }

  DCHECK(read_worker().blocking_start_time.is_null());
  DCHECK(!incremented_max_tasks_since_blocked_);
  DCHECK(!incremented_max_best_effort_tasks_since_blocked_);

  // Running task bookkeeping.
  outer()->DecrementTasksRunningLockRequired(
      *read_worker().current_task_priority);
  write_worker().current_shutdown_behavior = std::nullopt;
  write_worker().current_task_priority = std::nullopt;

  if (transaction_with_task_source) {
    // If there is a task to enqueue, we can swap it for another task without
    // changing DesiredNumAwakeWorkers(), and thus without worrying about
    // signaling/waiting.
    outer()->ReEnqueueTaskSourceLockRequired(
        &workers_executor, &reenqueue_executor,
        std::move(transaction_with_task_source.value()));

    return GetWorkLockRequired(&workers_executor,
                               static_cast<WorkerThreadSemaphore*>(worker));
  } else if (outer()->GetDesiredNumAwakeWorkersLockRequired() >=
             outer()->num_active_signals_) {
    // When the thread pool wants more work to be run but hasn't signaled
    // workers for it yet we can take advantage and grab more work without
    // signal/wait contention.
    return GetWorkLockRequired(&workers_executor,
                               static_cast<WorkerThreadSemaphore*>(worker));
  }

  // In the case where the worker does not have a task source to exchange and
  // the thread group doesn't want more work than the number of workers awake,
  // it must WaitForWork(), to keep `num_active_signals` synchronized with the
  // number of desired awake workers.
  OnWorkerBecomesIdleLockRequired(&workers_executor, worker);
  return nullptr;
}

TimeDelta ThreadGroupSemaphore::SemaphoreWorkerDelegate::GetSleepTimeout() {
  return ThreadPoolSleepTimeout();
}

bool ThreadGroupSemaphore::SemaphoreWorkerDelegate::CanCleanupLockRequired(
    const WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  return is_excess_ && LIKELY(!outer()->worker_cleanup_disallowed_for_testing_);
}

void ThreadGroupSemaphore::SemaphoreWorkerDelegate::CleanupLockRequired(
    BaseScopedCommandsExecutor* executor,
    WorkerThread* worker_base) {
  WorkerThreadSemaphore* worker =
      static_cast<WorkerThreadSemaphore*>(worker_base);
  DCHECK(!outer()->join_called_for_testing_.IsSet());
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  worker->Cleanup();

  // Remove the worker from `workers_`.
  DCHECK(!outer()->after_start().no_worker_reclaim ||
         outer()->workers_.size() > outer()->after_start().initial_max_tasks);
  auto num_erased = std::erase(outer()->workers_, worker);
  CHECK_EQ(num_erased, 1u);
}

void ThreadGroupSemaphore::SemaphoreWorkerDelegate::
    OnWorkerBecomesIdleLockRequired(BaseScopedCommandsExecutor* executor,
                                    WorkerThread* worker_base) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  CHECK_GT(outer()->num_active_signals_, 0u);
  --outer()->num_active_signals_;
  outer()->idle_workers_set_cv_for_testing_.Signal();
}

void ThreadGroupSemaphore::SemaphoreWorkerDelegate::RecordUnnecessaryWakeup() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  RecordUnnecessaryWakeupImpl();
}

void ThreadGroupSemaphore::JoinForTesting() {
  decltype(workers_) workers_copy;
  {
    SemaphoreScopedCommandsExecutor executor(this);
    CheckedAutoLock auto_lock(lock_);
    AnnotateAcquiredLockAlias alias(lock_, executor.outer()->lock_);
    priority_queue_.EnableFlushTaskSourcesOnDestroyForTesting();

    DCHECK_GT(workers_.size(), size_t(0))
        << "Joined an unstarted thread group.";

    join_called_for_testing_.Set();

    // Ensure WorkerThreads in `workers_` do not attempt to cleanup while
    // being joined.
    worker_cleanup_disallowed_for_testing_ = true;

    // Make a copy of the WorkerThreads so that we can call
    // WorkerThread::JoinForTesting() without holding `lock_` since
    // WorkerThreads may need to access `workers_`.
    workers_copy = workers_;

    for (size_t i = 0; i < workers_copy.size(); ++i) {
      executor.ScheduleSignal();
    }
    join_called_for_testing_.Set();
  }
  for (const auto& worker : workers_copy) {
    static_cast<WorkerThreadSemaphore*>(worker.get())->JoinForTesting();
  }

  CheckedAutoLock auto_lock(lock_);
  DCHECK(workers_ == workers_copy);
  // Release `workers_` to clear their TrackedRef against `this`.
  workers_.clear();
}

void ThreadGroupSemaphore::CreateAndRegisterWorkerLockRequired(
    SemaphoreScopedCommandsExecutor* executor) {
  if (workers_.size() == kMaxNumberOfWorkers) {
    return;
  }
  DCHECK_LT(workers_.size(), kMaxNumberOfWorkers);
  if (workers_.size() >= max_tasks_) {
    return;
  }
  DCHECK(!join_called_for_testing_.IsSet());

  // WorkerThread needs `lock_` as a predecessor for its thread lock because in
  // GetWork(), `lock_` is first acquired and then the thread lock is acquired
  // when GetLastUsedTime() is called on the worker by CanGetWorkLockRequired().
  scoped_refptr<WorkerThreadSemaphore> worker =
      MakeRefCounted<WorkerThreadSemaphore>(
          thread_type_hint_,
          std::make_unique<SemaphoreWorkerDelegate>(
              tracked_ref_factory_.GetTrackedRef(),
              /*is_excess=*/after_start().no_worker_reclaim
                  ? workers_.size() >= after_start().initial_max_tasks
                  : true,
              &join_called_for_testing_),
          task_tracker_, worker_sequence_num_++, &lock_, &semaphore_);
  DCHECK(worker);
  workers_.push_back(worker);
  DCHECK_LE(workers_.size(), max_tasks_);
  executor->ScheduleStart(worker);
}

void ThreadGroupSemaphore::DidUpdateCanRunPolicy() {
  SemaphoreScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  EnsureEnoughWorkersLockRequired(&executor);
}

ThreadGroup::ThreadGroupWorkerDelegate* ThreadGroupSemaphore::GetWorkerDelegate(
    WorkerThread* worker) {
  return static_cast<ThreadGroup::ThreadGroupWorkerDelegate*>(
      static_cast<SemaphoreWorkerDelegate*>(worker->delegate()));
}

void ThreadGroupSemaphore::OnShutdownStarted() {
  SemaphoreScopedCommandsExecutor executor(this);
  OnShutDownStartedImpl(&executor);
}

void ThreadGroupSemaphore::EnsureEnoughWorkersLockRequired(
    BaseScopedCommandsExecutor* base_executor) {
  // Don't do anything if the thread group isn't started.
  if (max_tasks_ == 0 || UNLIKELY(join_called_for_testing_.IsSet())) {
    return;
  }

  SemaphoreScopedCommandsExecutor* executor =
      static_cast<SemaphoreScopedCommandsExecutor*>(base_executor);

  const size_t desired_awake_workers = GetDesiredNumAwakeWorkersLockRequired();
  // The +1 here is due to the fact that we always want there to be one idle
  // worker.
  const size_t num_workers_to_create =
      std::min({static_cast<size_t>(after_start().max_num_workers_created),
                static_cast<size_t>(
                    ClampSub(desired_awake_workers + 1, workers_.size()))});
  for (size_t i = 0; i < num_workers_to_create; ++i) {
    CreateAndRegisterWorkerLockRequired(executor);
  }

  const size_t new_signals = std::min(
      // Don't signal more than `workers_.size()` workers.
      {ClampSub(workers_.size(), num_active_signals_),
       ClampSub(desired_awake_workers, num_active_signals_)});
  AnnotateAcquiredLockAlias alias(lock_, executor->outer()->lock_);
  for (size_t i = 0; i < new_signals; ++i) {
    executor->ScheduleSignal();
  }

  // This function is called every time a task source is (re-)enqueued,
  // hence the minimum priority needs to be updated.
  UpdateMinAllowedPriorityLockRequired();

  // Ensure that the number of workers is periodically adjusted if needed.
  MaybeScheduleAdjustMaxTasksLockRequired(executor);
}

}  // namespace internal
}  // namespace base
