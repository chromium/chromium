// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group_impl.h"

#include <stddef.h>

#include <algorithm>
#include <type_traits>
#include <utility>

#include "base/atomicops.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/containers/stack_container.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/clamped_math.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_token.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_windows_thread_environment.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/windows_version.h"
#endif  // BUILDFLAG(IS_WIN)

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
// TODO(https://crbug.com/927755): Fix racy condition when MayBlockThreshold ==
// BlockedWorkersPoll.
constexpr TimeDelta kForegroundMayBlockThreshold = Milliseconds(1000);
constexpr TimeDelta kForegroundBlockedWorkersPoll = Milliseconds(1200);
constexpr TimeDelta kBackgroundMayBlockThreshold = Seconds(10);
constexpr TimeDelta kBackgroundBlockedWorkersPoll = Seconds(12);

// Only used in DCHECKs.
bool ContainsWorker(const std::vector<scoped_refptr<WorkerThread>>& workers,
                    const WorkerThread* worker) {
  auto it =
      ranges::find_if(workers, [worker](const scoped_refptr<WorkerThread>& i) {
        return i.get() == worker;
      });
  return it != workers.end();
}

}  // namespace

// Upon destruction, executes actions that control the number of active workers.
// Useful to satisfy locking requirements of these actions.
class ThreadGroupImpl::ScopedCommandsExecutor
    : public ThreadGroup::BaseScopedCommandsExecutor {
 public:
  explicit ScopedCommandsExecutor(ThreadGroupImpl* outer) : outer_(outer) {}

  ScopedCommandsExecutor(const ScopedCommandsExecutor&) = delete;
  ScopedCommandsExecutor& operator=(const ScopedCommandsExecutor&) = delete;
  ~ScopedCommandsExecutor() { FlushImpl(); }

  void ScheduleWakeUp(scoped_refptr<WorkerThread> worker) {
    workers_to_wake_up_.AddWorker(std::move(worker));
  }

  void ScheduleStart(scoped_refptr<WorkerThread> worker) {
    workers_to_start_.AddWorker(std::move(worker));
  }

  void FlushWorkerCreation(CheckedLock* held_lock) {
    if (workers_to_wake_up_.empty() && workers_to_start_.empty())
      return;
    CheckedAutoUnlock auto_unlock(*held_lock);
    FlushImpl();
    workers_to_wake_up_.clear();
    workers_to_start_.clear();
    must_schedule_adjust_max_tasks_ = false;
  }

  void ScheduleAdjustMaxTasks() {
    DCHECK(!must_schedule_adjust_max_tasks_);
    must_schedule_adjust_max_tasks_ = true;
  }

 private:
  class WorkerContainer {
   public:
    WorkerContainer() = default;
    WorkerContainer(const WorkerContainer&) = delete;
    WorkerContainer& operator=(const WorkerContainer&) = delete;

    void AddWorker(scoped_refptr<WorkerThread> worker) {
      if (!worker)
        return;
      if (!first_worker_)
        first_worker_ = std::move(worker);
      else
        additional_workers_.push_back(std::move(worker));
    }

    template <typename Action>
    void ForEachWorker(Action action) {
      if (first_worker_) {
        action(first_worker_.get());
        for (scoped_refptr<WorkerThread> worker : additional_workers_)
          action(worker.get());
      } else {
        DCHECK(additional_workers_.empty());
      }
    }

    bool empty() const { return first_worker_ == nullptr; }

    void clear() {
      first_worker_.reset();
      additional_workers_.clear();
    }

   private:
    // The purpose of |first_worker| is to avoid a heap allocation by the vector
    // in the case where there is only one worker in the container.
    scoped_refptr<WorkerThread> first_worker_;
    std::vector<scoped_refptr<WorkerThread>> additional_workers_;
  };

  void FlushImpl() {
    CheckedLock::AssertNoLockHeldOnCurrentThread();

    // Wake up workers.
    workers_to_wake_up_.ForEachWorker(
        [](WorkerThread* worker) { worker->WakeUp(); });

    // Start workers. Happens after wake ups to prevent the case where a worker
    // enters its main function, is descheduled because it wasn't woken up yet,
    // and is woken up immediately after.
    workers_to_start_.ForEachWorker([&](WorkerThread* worker) {
      worker->Start(outer_->after_start().service_thread_task_runner,
                    outer_->after_start().worker_thread_observer);
      if (outer_->worker_started_for_testing_)
        outer_->worker_started_for_testing_->Wait();
    });

    if (must_schedule_adjust_max_tasks_)
      outer_->ScheduleAdjustMaxTasks();
  }

  const raw_ptr<ThreadGroupImpl> outer_;

  WorkerContainer workers_to_wake_up_;
  WorkerContainer workers_to_start_;
  bool must_schedule_adjust_max_tasks_ = false;
};

class ThreadGroupImpl::WorkerThreadDelegateImpl : public WorkerThread::Delegate,
                                                  public BlockingObserver {
 public:
  // |outer| owns the worker for which this delegate is constructed. If
  // |is_excess| is true, this worker will be eligible for reclaim.
  explicit WorkerThreadDelegateImpl(TrackedRef<ThreadGroupImpl> outer,
                                    bool is_excess);
  WorkerThreadDelegateImpl(const WorkerThreadDelegateImpl&) = delete;
  WorkerThreadDelegateImpl& operator=(const WorkerThreadDelegateImpl&) = delete;

  // OnMainExit() handles the thread-affine cleanup; WorkerThreadDelegateImpl
  // can thereafter safely be deleted from any thread.
  ~WorkerThreadDelegateImpl() override = default;

  // WorkerThread::Delegate:
  WorkerThread::ThreadLabel GetThreadLabel() const override;
  void OnMainEntry(WorkerThread* worker) override;
  RegisteredTaskSource GetWork(WorkerThread* worker) override;
  void DidProcessTask(RegisteredTaskSource task_source) override;
  TimeDelta GetSleepTimeout() override;
  void OnMainExit(WorkerThread* worker) override;
  void RecordUnnecessaryWakeup() override;

  // BlockingObserver:
  void BlockingStarted(BlockingType blocking_type) override;
  void BlockingTypeUpgraded() override;
  void BlockingEnded() override;

  // Notifies the worker of shutdown, possibly marking the running task as
  // MAY_BLOCK.
  void OnShutdownStartedLockRequired(ScopedCommandsExecutor* executor)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Returns true iff the worker can get work. Cleans up the worker or puts it
  // on the idle set if it can't get work.
  bool CanGetWorkLockRequired(ScopedCommandsExecutor* executor,
                              WorkerThread* worker)
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

  // True if this worker is be eligible for reclaim.
  bool is_excess() const { return is_excess_; }

  // Exposed for AnnotateAcquiredLockAlias
  const CheckedLock& lock() const LOCK_RETURNED(outer_->lock_) {
    return outer_->lock_;
  }

 private:
  // Returns true if |worker| is allowed to cleanup and remove itself from the
  // thread group. Called from GetWork() when no work is available.
  bool CanCleanupLockRequired(const WorkerThread* worker) const
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Calls cleanup on |worker| and removes it from the thread group. Called from
  // GetWork() when no work is available and CanCleanupLockRequired() returns
  // true.
  void CleanupLockRequired(ScopedCommandsExecutor* executor,
                           WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Called in GetWork() when a worker becomes idle.
  void OnWorkerBecomesIdleLockRequired(ScopedCommandsExecutor* executor,
                                       WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer_->lock_);

  // Accessed only from the worker thread.
  struct WorkerOnly {
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
    absl::optional<TaskPriority> current_task_priority;
    // The shutdown behavior of the task the worker is currently running if any.
    absl::optional<TaskShutdownBehavior> current_shutdown_behavior;

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

ThreadGroupImpl::ThreadGroupImpl(StringPiece histogram_label,
                                 StringPiece thread_group_label,
                                 ThreadType thread_type_hint,
                                 TrackedRef<TaskTracker> task_tracker,
                                 TrackedRef<Delegate> delegate,
                                 ThreadGroup* predecessor_thread_group)
    : ThreadGroup(std::move(task_tracker),
                  std::move(delegate),
                  predecessor_thread_group),
      histogram_label_(histogram_label),
      thread_group_label_(thread_group_label),
      thread_type_hint_(thread_type_hint),
      idle_workers_set_cv_for_testing_(lock_.CreateConditionVariable()),
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
    absl::optional<TimeDelta> may_block_threshold) {
  ThreadGroup::Start();

  DCHECK(!replacement_thread_group_);

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

  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);

  DCHECK(workers_.empty());
  max_tasks_ = max_tasks;
  DCHECK_GE(max_tasks_, 1U);
  in_start().initial_max_tasks = max_tasks_;
  DCHECK_LE(in_start().initial_max_tasks, kMaxNumberOfWorkers);
  max_best_effort_tasks_ = max_best_effort_tasks;
  in_start().suggested_reclaim_time = suggested_reclaim_time;
  in_start().worker_environment = worker_environment;
  in_start().service_thread_task_runner = std::move(service_thread_task_runner);
  in_start().worker_thread_observer = worker_thread_observer;

#if DCHECK_IS_ON()
  in_start().initialized = true;
#endif

  if (synchronous_thread_start_for_testing) {
    worker_started_for_testing_.emplace(WaitableEvent::ResetPolicy::AUTOMATIC);
    // Don't emit a ScopedBlockingCallWithBaseSyncPrimitives from this
    // WaitableEvent or it defeats the purpose of having threads start without
    // externally visible side-effects.
    worker_started_for_testing_->declare_only_used_while_idle();
  }

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
    TransactionWithRegisteredTaskSource transaction_with_task_source) {
  ScopedCommandsExecutor executor(this);
  PushTaskSourceAndWakeUpWorkersImpl(&executor,
                                     std::move(transaction_with_task_source));
}

size_t ThreadGroupImpl::GetMaxConcurrentNonBlockedTasksDeprecated() const {
#if DCHECK_IS_ON()
  CheckedAutoLock auto_lock(lock_);
  DCHECK_NE(after_start().initial_max_tasks, 0U)
      << "GetMaxConcurrentTasksDeprecated() should only be called after the "
      << "thread group has started.";
#endif
  return after_start().initial_max_tasks;
}

void ThreadGroupImpl::WaitForWorkersIdleForTesting(size_t n) {
  CheckedAutoLock auto_lock(lock_);

#if DCHECK_IS_ON()
  DCHECK(!some_workers_cleaned_up_for_testing_)
      << "Workers detached prior to waiting for a specific number of idle "
         "workers. Doing the wait under such conditions is flaky. Consider "
         "setting the suggested reclaim time to TimeDelta::Max() in Start().";
#endif

  WaitForWorkersIdleLockRequiredForTesting(n);
}

void ThreadGroupImpl::WaitForAllWorkersIdleForTesting() {
  CheckedAutoLock auto_lock(lock_);
  WaitForWorkersIdleLockRequiredForTesting(workers_.size());
}

void ThreadGroupImpl::WaitForWorkersCleanedUpForTesting(size_t n) {
  CheckedAutoLock auto_lock(lock_);

  if (!num_workers_cleaned_up_for_testing_cv_)
    num_workers_cleaned_up_for_testing_cv_ = lock_.CreateConditionVariable();

  while (num_workers_cleaned_up_for_testing_ < n)
    num_workers_cleaned_up_for_testing_cv_->Wait();

  num_workers_cleaned_up_for_testing_ = 0;
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
  for (const auto& worker : workers_copy)
    worker->JoinForTesting();

  CheckedAutoLock auto_lock(lock_);
  DCHECK(workers_ == workers_copy);
  // Release |workers_| to clear their TrackedRef against |this|.
  workers_.clear();
}

size_t ThreadGroupImpl::NumberOfWorkersForTesting() const {
  CheckedAutoLock auto_lock(lock_);
  return workers_.size();
}

size_t ThreadGroupImpl::GetMaxTasksForTesting() const {
  CheckedAutoLock auto_lock(lock_);
  return max_tasks_;
}

size_t ThreadGroupImpl::GetMaxBestEffortTasksForTesting() const {
  CheckedAutoLock auto_lock(lock_);
  return max_best_effort_tasks_;
}

size_t ThreadGroupImpl::NumberOfIdleWorkersForTesting() const {
  CheckedAutoLock auto_lock(lock_);
  return idle_workers_set_.Size();
}

ThreadGroupImpl::WorkerThreadDelegateImpl::WorkerThreadDelegateImpl(
    TrackedRef<ThreadGroupImpl> outer,
    bool is_excess)
    : outer_(std::move(outer)), is_excess_(is_excess) {
  // Bound in OnMainEntry().
  DETACH_FROM_THREAD(worker_thread_checker_);
}

WorkerThread::ThreadLabel
ThreadGroupImpl::WorkerThreadDelegateImpl::GetThreadLabel() const {
  return WorkerThread::ThreadLabel::POOLED;
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::OnMainEntry(
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  {
#if DCHECK_IS_ON()
    CheckedAutoLock auto_lock(outer_->lock_);
    DCHECK(ContainsWorker(outer_->workers_, worker));
#endif
  }

#if BUILDFLAG(IS_WIN)
  worker_only().win_thread_environment = GetScopedWindowsThreadEnvironment(
      outer_->after_start().worker_environment);
#endif  // BUILDFLAG(IS_WIN)

  PlatformThread::SetName(
      StringPrintf("ThreadPool%sWorker", outer_->thread_group_label_.c_str()));

  outer_->BindToCurrentThread();
  worker_only().worker_thread_ = worker;
  SetBlockingObserverForCurrentThread(this);

  if (outer_->worker_started_for_testing_) {
    // When |worker_started_for_testing_| is set, the thread that starts workers
    // should wait for a worker to have started before starting the next one,
    // and there should only be one thread that wakes up workers at a time.
    DCHECK(!outer_->worker_started_for_testing_->IsSignaled());
    outer_->worker_started_for_testing_->Signal();
  }
}

RegisteredTaskSource ThreadGroupImpl::WorkerThreadDelegateImpl::GetWork(
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!read_worker().current_task_priority);
  DCHECK(!read_worker().current_shutdown_behavior);

  ScopedCommandsExecutor executor(outer_.get());
  CheckedAutoLock auto_lock(outer_->lock_);

  DCHECK(ContainsWorker(outer_->workers_, worker));

  // Use this opportunity, before assigning work to this worker, to create/wake
  // additional workers if needed (doing this here allows us to reduce
  // potentially expensive create/wake directly on PostTask()).
  //
  // Note: FlushWorkerCreation() below releases |outer_->lock_|. It is thus
  // important that all other operations come after it to keep this method
  // transactional.
  outer_->EnsureEnoughWorkersLockRequired(&executor);
  executor.FlushWorkerCreation(&outer_->lock_);

  if (!CanGetWorkLockRequired(&executor, worker))
    return nullptr;

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

    task_source = outer_->TakeRegisteredTaskSource(&executor);
  }
  if (!task_source) {
    OnWorkerBecomesIdleLockRequired(&executor, worker);
    return nullptr;
  }

  // Running task bookkeeping.
  outer_->IncrementTasksRunningLockRequired(priority);
  DCHECK(!outer_->idle_workers_set_.Contains(worker));
  write_worker().current_task_priority = priority;
  write_worker().current_shutdown_behavior = task_source->shutdown_behavior();

  return task_source;
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::DidProcessTask(
    RegisteredTaskSource task_source) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(read_worker().current_task_priority);
  DCHECK(read_worker().current_shutdown_behavior);

  // A transaction to the TaskSource to reenqueue, if any. Instantiated here as
  // |TaskSource::lock_| is a UniversalPredecessor and must always be acquired
  // prior to acquiring a second lock
  absl::optional<TransactionWithRegisteredTaskSource>
      transaction_with_task_source;
  if (task_source) {
    transaction_with_task_source.emplace(
        TransactionWithRegisteredTaskSource::FromTaskSource(
            std::move(task_source)));
  }

  ScopedCommandsExecutor workers_executor(outer_.get());
  ScopedReenqueueExecutor reenqueue_executor;
  CheckedAutoLock auto_lock(outer_->lock_);

  // During shutdown, max_tasks may have been incremented in StartShutdown().
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
  write_worker().current_shutdown_behavior = absl::nullopt;
  write_worker().current_task_priority = absl::nullopt;

  if (transaction_with_task_source) {
    outer_->ReEnqueueTaskSourceLockRequired(
        &workers_executor, &reenqueue_executor,
        std::move(transaction_with_task_source.value()));
  }
}

TimeDelta ThreadGroupImpl::WorkerThreadDelegateImpl::GetSleepTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  if (!is_excess())
    return TimeDelta::Max();
  // Sleep for an extra 10% to avoid the following pathological case:
  //   0) A task is running on a timer which matches
  //      |after_start().suggested_reclaim_time|.
  //   1) The timer fires and this worker is created by
  //      MaintainAtLeastOneIdleWorkerLockRequired() because the last idle
  //      worker was assigned the task.
  //   2) This worker begins sleeping |after_start().suggested_reclaim_time| (at
  //      the front of the idle set).
  //   3) The task assigned to the other worker completes and the worker goes
  //      back in the idle set (this worker may now second on the idle set;
  //      its GetLastUsedTime() is set to Now()).
  //   4) The sleep in (2) expires. Since (3) was fast this worker is likely to
  //      have been second on the idle set long enough for
  //      CanCleanupLockRequired() to be satisfied in which case this worker is
  //      cleaned up.
  //   5) The timer fires at roughly the same time and we're back to (1) if (4)
  //      resulted in a clean up; causing thread churn.
  //
  //   Sleeping 10% longer in (2) makes it much less likely that (4) occurs
  //   before (5). In that case (5) will cause (3) and refresh this worker's
  //   GetLastUsedTime(), making CanCleanupLockRequired() return false in (4)
  //   and avoiding churn.
  //
  //   Of course the same problem arises if in (0) the timer matches
  //   |after_start().suggested_reclaim_time * 1.1| but it's expected that any
  //   timer slower than |after_start().suggested_reclaim_time| will cause such
  //   churn during long idle periods. If this is a problem in practice, the
  //   standby thread configuration and algorithm should be revisited.
  return outer_->after_start().suggested_reclaim_time * 1.1;
}

bool ThreadGroupImpl::WorkerThreadDelegateImpl::CanCleanupLockRequired(
    const WorkerThread* worker) const {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  if (!is_excess())
    return false;

  const TimeTicks last_used_time = worker->GetLastUsedTime();
  return !last_used_time.is_null() &&
         subtle::TimeTicksNowIgnoringOverride() - last_used_time >=
             outer_->after_start().suggested_reclaim_time &&
         LIKELY(!outer_->worker_cleanup_disallowed_for_testing_);
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::CleanupLockRequired(
    ScopedCommandsExecutor* executor,
    WorkerThread* worker) {
  DCHECK(!outer_->join_for_testing_started_);
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  worker->Cleanup();

  if (outer_->IsOnIdleSetLockRequired(worker))
    outer_->idle_workers_set_.Remove(worker);

  // Remove the worker from |workers_|.
  auto worker_iter = ranges::find(outer_->workers_, worker);
  DCHECK(worker_iter != outer_->workers_.end());
  outer_->workers_.erase(worker_iter);
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::OnWorkerBecomesIdleLockRequired(
    ScopedCommandsExecutor* executor,
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!outer_->idle_workers_set_.Contains(worker));

  // Add the worker to the idle set.
  outer_->idle_workers_set_.Insert(worker);
  DCHECK_LE(outer_->idle_workers_set_.Size(), outer_->workers_.size());
  outer_->idle_workers_set_cv_for_testing_->Broadcast();
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::OnMainExit(
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

#if DCHECK_IS_ON()
  {
    bool shutdown_complete = outer_->task_tracker_->IsShutdownComplete();
    CheckedAutoLock auto_lock(outer_->lock_);

    // |worker| should already have been removed from the idle workers set and
    // |workers_| by the time the thread is about to exit. (except in the cases
    // where the thread group is no longer going to be used - in which case,
    // it's fine for there to be invalid workers in the thread group.
    if (!shutdown_complete && !outer_->join_for_testing_started_) {
      DCHECK(!outer_->idle_workers_set_.Contains(worker));
      DCHECK(!ContainsWorker(outer_->workers_, worker));
    }
  }
#endif

#if BUILDFLAG(IS_WIN)
  worker_only().win_thread_environment.reset();
#endif  // BUILDFLAG(IS_WIN)

  // Count cleaned up workers for tests. It's important to do this here instead
  // of at the end of CleanupLockRequired() because some side-effects of
  // cleaning up happen outside the lock (e.g. recording histograms) and
  // resuming from tests must happen-after that point or checks on the main
  // thread will be flaky (crbug.com/1047733).
  CheckedAutoLock auto_lock(outer_->lock_);
  ++outer_->num_workers_cleaned_up_for_testing_;
#if DCHECK_IS_ON()
  outer_->some_workers_cleaned_up_for_testing_ = true;
#endif
  if (outer_->num_workers_cleaned_up_for_testing_cv_)
    outer_->num_workers_cleaned_up_for_testing_cv_->Signal();
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::RecordUnnecessaryWakeup() {
  base::BooleanHistogram::FactoryGet(
      std::string("ThreadPool.UnnecessaryWakeup.") + outer_->histogram_label_,
      base::Histogram::kUmaTargetedHistogramFlag)
      ->Add(true);

  TRACE_EVENT_INSTANT("wakeup.flow", "ThreadPool.UnnecessaryWakeup");
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::BlockingStarted(
    BlockingType blocking_type) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(read_worker().current_task_priority);
  DCHECK(worker_only().worker_thread_);

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

  if (incremented_max_tasks_for_shutdown_)
    return;

  if (*read_any().current_task_priority == TaskPriority::BEST_EFFORT)
    ++outer_->num_unresolved_best_effort_may_block_;

  if (blocking_type == BlockingType::WILL_BLOCK) {
    incremented_max_tasks_since_blocked_ = true;
    outer_->IncrementMaxTasksLockRequired();
    outer_->EnsureEnoughWorkersLockRequired(&executor);
  } else {
    ++outer_->num_unresolved_may_block_;
  }

  outer_->MaybeScheduleAdjustMaxTasksLockRequired(&executor);
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::BlockingTypeUpgraded() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(read_worker().current_task_priority);

  // The blocking type always being WILL_BLOCK in this experiment and with time
  // overrides, it should never be considered "upgraded".
  if (base::subtle::ScopedTimeClockOverrides::overrides_active())
    return;

  ScopedCommandsExecutor executor(outer_.get());
  CheckedAutoLock auto_lock(outer_->lock_);

  // Don't do anything if a MAY_BLOCK ScopedBlockingCall instantiated in the
  // same scope already caused the max tasks to be incremented.
  if (incremented_max_tasks_since_blocked_)
    return;

  // Cancel the effect of a MAY_BLOCK ScopedBlockingCall instantiated in the
  // same scope.
  --outer_->num_unresolved_may_block_;

  incremented_max_tasks_since_blocked_ = true;
  outer_->IncrementMaxTasksLockRequired();
  outer_->EnsureEnoughWorkersLockRequired(&executor);
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::BlockingEnded() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  CheckedAutoLock auto_lock(outer_->lock_);
  DCHECK(read_worker().current_task_priority);
  DCHECK(!read_worker().blocking_start_time.is_null());
  write_worker().blocking_start_time = TimeTicks();
  if (!incremented_max_tasks_for_shutdown_) {
    if (incremented_max_tasks_since_blocked_)
      outer_->DecrementMaxTasksLockRequired();
    else
      --outer_->num_unresolved_may_block_;

    if (*read_worker().current_task_priority == TaskPriority::BEST_EFFORT) {
      if (incremented_max_best_effort_tasks_since_blocked_)
        outer_->DecrementMaxBestEffortTasksLockRequired();
      else
        --outer_->num_unresolved_best_effort_may_block_;
    }
  }

  incremented_max_tasks_since_blocked_ = false;
  incremented_max_best_effort_tasks_since_blocked_ = false;
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::OnShutdownStartedLockRequired(
    ScopedCommandsExecutor* executor) {
  if (!read_any().is_running_task())
    return;
  // Workers running a CONTINUE_ON_SHUTDOWN tasks are replaced by incrementing
  // max_tasks/max_best_effort_tasks. The effect is reverted in
  // DidProcessTask().
  if (*read_any().current_shutdown_behavior ==
      TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN) {
    incremented_max_tasks_for_shutdown_ = true;
    IncrementMaxTasksLockRequired();
  }
}

bool ThreadGroupImpl::WorkerThreadDelegateImpl::CanGetWorkLockRequired(
    ScopedCommandsExecutor* executor,
    WorkerThread* worker) {
  const bool is_on_idle_workers_set = outer_->IsOnIdleSetLockRequired(worker);
  DCHECK_EQ(is_on_idle_workers_set, outer_->idle_workers_set_.Contains(worker));

  if (is_on_idle_workers_set) {
    if (CanCleanupLockRequired(worker))
      CleanupLockRequired(executor, worker);
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

void ThreadGroupImpl::WorkerThreadDelegateImpl::
    MaybeIncrementMaxTasksLockRequired() {
  if (read_any().blocking_start_time.is_null() ||
      subtle::TimeTicksNowIgnoringOverride() - read_any().blocking_start_time <
          outer_->after_start().may_block_threshold) {
    return;
  }
  IncrementMaxTasksLockRequired();
}

void ThreadGroupImpl::WorkerThreadDelegateImpl::
    IncrementMaxTasksLockRequired() {
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

void ThreadGroupImpl::WaitForWorkersIdleLockRequiredForTesting(size_t n) {
  // Make sure workers do not cleanup while watching the idle count.
  AutoReset<bool> ban_cleanups(&worker_cleanup_disallowed_for_testing_, true);

  while (idle_workers_set_.Size() < n)
    idle_workers_set_cv_for_testing_->Wait();
}

void ThreadGroupImpl::MaintainAtLeastOneIdleWorkerLockRequired(
    ScopedCommandsExecutor* executor) {
  if (workers_.size() == kMaxNumberOfWorkers)
    return;
  DCHECK_LT(workers_.size(), kMaxNumberOfWorkers);

  if (!idle_workers_set_.IsEmpty())
    return;

  if (workers_.size() >= max_tasks_)
    return;

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

  // WorkerThread needs |lock_| as a predecessor for its thread lock
  // because in WakeUpOneWorker, |lock_| is first acquired and then
  // the thread lock is acquired when WakeUp is called on the worker.
  scoped_refptr<WorkerThread> worker = MakeRefCounted<WorkerThread>(
      thread_type_hint_,
      std::make_unique<WorkerThreadDelegateImpl>(
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

size_t ThreadGroupImpl::GetDesiredNumAwakeWorkersLockRequired() const {
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

void ThreadGroupImpl::DidUpdateCanRunPolicy() {
  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  EnsureEnoughWorkersLockRequired(&executor);
}

void ThreadGroupImpl::OnShutdownStarted() {
  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);

  // Don't do anything if the thread group isn't started.
  if (max_tasks_ == 0 || UNLIKELY(join_for_testing_started_))
    return;

  // Start a MAY_BLOCK scope on each worker that is already running a task.
  for (scoped_refptr<WorkerThread>& worker : workers_) {
    // The delegates of workers inside a ThreadGroupImpl should be
    // WorkerThreadDelegateImpls.
    WorkerThreadDelegateImpl* delegate =
        static_cast<WorkerThreadDelegateImpl*>(worker->delegate());
    AnnotateAcquiredLockAlias annotate(lock_, delegate->lock());
    delegate->OnShutdownStartedLockRequired(&executor);
  }
  EnsureEnoughWorkersLockRequired(&executor);

  shutdown_started_ = true;
}

void ThreadGroupImpl::EnsureEnoughWorkersLockRequired(
    BaseScopedCommandsExecutor* base_executor) {
  // Don't do anything if the thread group isn't started.
  if (max_tasks_ == 0 || UNLIKELY(join_for_testing_started_))
    return;

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
  if (desired_num_awake_workers == num_awake_workers)
    MaintainAtLeastOneIdleWorkerLockRequired(executor);

  // This function is called every time a task source is (re-)enqueued,
  // hence the minimum priority needs to be updated.
  UpdateMinAllowedPriorityLockRequired();

  // Ensure that the number of workers is periodically adjusted if needed.
  MaybeScheduleAdjustMaxTasksLockRequired(executor);
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
    // WorkerThreadDelegateImpls.
    WorkerThreadDelegateImpl* delegate =
        static_cast<WorkerThreadDelegateImpl*>(worker->delegate());
    AnnotateAcquiredLockAlias annotate(lock_, delegate->lock());
    delegate->MaybeIncrementMaxTasksLockRequired();
  }

  // Wake up workers according to the updated |max_tasks_|. This will also
  // reschedule AdjustMaxTasks() if necessary.
  EnsureEnoughWorkersLockRequired(&executor);
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

void ThreadGroupImpl::MaybeScheduleAdjustMaxTasksLockRequired(
    ScopedCommandsExecutor* executor) {
  if (!adjust_max_tasks_posted_ &&
      ShouldPeriodicallyAdjustMaxTasksLockRequired()) {
    executor->ScheduleAdjustMaxTasks();
    adjust_max_tasks_posted_ = true;
  }
}

bool ThreadGroupImpl::ShouldPeriodicallyAdjustMaxTasksLockRequired() {
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

void ThreadGroupImpl::UpdateMinAllowedPriorityLockRequired() {
  if (priority_queue_.IsEmpty() || num_running_tasks_ < max_tasks_) {
    max_allowed_sort_key_.store(kMaxYieldSortKey, std::memory_order_relaxed);
  } else {
    max_allowed_sort_key_.store({priority_queue_.PeekSortKey().priority(),
                                 priority_queue_.PeekSortKey().worker_count()},
                                std::memory_order_relaxed);
  }
}

bool ThreadGroupImpl::IsOnIdleSetLockRequired(WorkerThread* worker) const {
  // To avoid searching through the idle set : use GetLastUsedTime() not being
  // null (or being directly on top of the idle set) as a proxy for being on
  // the idle set.
  return idle_workers_set_.Peek() == worker ||
         !worker->GetLastUsedTime().is_null();
}

void ThreadGroupImpl::DecrementTasksRunningLockRequired(TaskPriority priority) {
  DCHECK_GT(num_running_tasks_, 0U);
  --num_running_tasks_;
  if (priority == TaskPriority::BEST_EFFORT) {
    DCHECK_GT(num_running_best_effort_tasks_, 0U);
    --num_running_best_effort_tasks_;
  }
  UpdateMinAllowedPriorityLockRequired();
}

void ThreadGroupImpl::IncrementTasksRunningLockRequired(TaskPriority priority) {
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

void ThreadGroupImpl::DecrementMaxTasksLockRequired() {
  DCHECK_GT(num_running_tasks_, 0U);
  DCHECK_GT(max_tasks_, 0U);
  --max_tasks_;
  UpdateMinAllowedPriorityLockRequired();
}

void ThreadGroupImpl::IncrementMaxTasksLockRequired() {
  DCHECK_GT(num_running_tasks_, 0U);
  ++max_tasks_;
  UpdateMinAllowedPriorityLockRequired();
}

void ThreadGroupImpl::DecrementMaxBestEffortTasksLockRequired() {
  DCHECK_GT(num_running_tasks_, 0U);
  DCHECK_GT(max_best_effort_tasks_, 0U);
  --max_best_effort_tasks_;
  UpdateMinAllowedPriorityLockRequired();
}

void ThreadGroupImpl::IncrementMaxBestEffortTasksLockRequired() {
  DCHECK_GT(num_running_tasks_, 0U);
  ++max_best_effort_tasks_;
  UpdateMinAllowedPriorityLockRequired();
}

ThreadGroupImpl::InitializedInStart::InitializedInStart() = default;
ThreadGroupImpl::InitializedInStart::~InitializedInStart() = default;

}  // namespace internal
}  // namespace base
