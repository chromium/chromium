// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/scheduler_worker_pool_impl.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/atomicops.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/sequence_token.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_scheduler/scheduler_worker_pool_params.h"
#include "base/task/task_scheduler/task_tracker.h"
#include "base/task/task_traits.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_windows_thread_environment.h"
#include "base/win/scoped_winrt_initializer.h"
#include "base/win/windows_version.h"
#endif  // defined(OS_WIN)

namespace base {
namespace internal {

constexpr TimeDelta SchedulerWorkerPoolImpl::kBlockedWorkersPollPeriod;

namespace {

constexpr char kPoolNameSuffix[] = "Pool";
constexpr char kDetachDurationHistogramPrefix[] =
    "TaskScheduler.DetachDuration.";
constexpr char kNumTasksBeforeDetachHistogramPrefix[] =
    "TaskScheduler.NumTasksBeforeDetach.";
constexpr char kNumTasksBetweenWaitsHistogramPrefix[] =
    "TaskScheduler.NumTasksBetweenWaits.";
constexpr char kNumThreadsHistogramPrefix[] = "TaskScheduler.NumWorkers.";
constexpr size_t kMaxNumberOfWorkers = 256;

// Only used in DCHECKs.
bool ContainsWorker(const std::vector<scoped_refptr<SchedulerWorker>>& workers,
                    const SchedulerWorker* worker) {
  auto it = std::find_if(workers.begin(), workers.end(),
                         [worker](const scoped_refptr<SchedulerWorker>& i) {
                           return i.get() == worker;
                         });
  return it != workers.end();
}

}  // namespace

class SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl
    : public SchedulerWorker::Delegate,
      public BlockingObserver {
 public:
  // |outer| owns the worker for which this delegate is constructed.
  SchedulerWorkerDelegateImpl(TrackedRef<SchedulerWorkerPoolImpl> outer);
  ~SchedulerWorkerDelegateImpl() override;

  // SchedulerWorker::Delegate:
  void OnCanScheduleSequence(scoped_refptr<Sequence> sequence) override;
  SchedulerWorker::ThreadLabel GetThreadLabel() const override;
  void OnMainEntry(const SchedulerWorker* worker) override;
  scoped_refptr<Sequence> GetWork(SchedulerWorker* worker) override;
  void DidRunTask() override;
  void ReEnqueueSequence(scoped_refptr<Sequence> sequence) override;
  TimeDelta GetSleepTimeout() override;
  void OnMainExit(SchedulerWorker* worker) override;

  // BlockingObserver:
  void BlockingStarted(BlockingType blocking_type) override;
  void BlockingTypeUpgraded() override;
  void BlockingEnded() override;

  void MayBlockEntered();
  void WillBlockEntered();

  // Returns true iff this worker has been within a MAY_BLOCK ScopedBlockingCall
  // for more than |outer_->MayBlockThreshold()|. The max tasks must be
  // incremented if this returns true.
  bool MustIncrementMaxTasksLockRequired();

  bool is_running_best_effort_task_lock_required() const {
    outer_->lock_.AssertAcquired();
    return is_running_best_effort_task_;
  }

 private:
  // Returns true if |worker| is allowed to cleanup and remove itself from the
  // pool. Called from GetWork() when no work is available.
  bool CanCleanupLockRequired(const SchedulerWorker* worker) const;

  // Calls cleanup on |worker| and removes it from the pool. Called from
  // GetWork() when no work is available and CanCleanupLockRequired() returns
  // true.
  void CleanupLockRequired(SchedulerWorker* worker);

  // Called in GetWork() when a worker becomes idle.
  void OnWorkerBecomesIdleLockRequired(SchedulerWorker* worker);

  const TrackedRef<SchedulerWorkerPoolImpl> outer_;

  // Time of the last detach.
  TimeTicks last_detach_time_;

  // Number of tasks executed since the last time the
  // TaskScheduler.NumTasksBetweenWaits histogram was recorded.
  size_t num_tasks_since_last_wait_ = 0;

  // Number of tasks executed since the last time the
  // TaskScheduler.NumTasksBeforeDetach histogram was recorded.
  size_t num_tasks_since_last_detach_ = 0;

  // Whether |outer_->max_tasks_| was incremented due to a ScopedBlockingCall on
  // the thread. Access synchronized by |outer_->lock_|.
  bool incremented_max_tasks_since_blocked_ = false;

  // Time when MayBlockScopeEntered() was last called. Reset when
  // BlockingScopeExited() is called. Access synchronized by |outer_->lock_|.
  TimeTicks may_block_start_time_;

  // Whether this worker is currently running a task (i.e. GetWork() has
  // returned a non-empty sequence and DidRunTask() hasn't been called yet).
  bool is_running_task_ = false;

  // Whether this worker is currently running a TaskPriority::BEST_EFFORT task.
  // Writes are made from the worker thread and are protected by
  // |outer_->lock_|. Reads are made from any thread, they are protected by
  // |outer_->lock_| when made outside of the worker thread.
  bool is_running_best_effort_task_ = false;

#if defined(OS_WIN)
  std::unique_ptr<win::ScopedWindowsThreadEnvironment> win_thread_environment_;
#endif  // defined(OS_WIN)

  // Verifies that specific calls are always made from the worker thread.
  THREAD_CHECKER(worker_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SchedulerWorkerDelegateImpl);
};

SchedulerWorkerPoolImpl::SchedulerWorkerPoolImpl(
    StringPiece histogram_label,
    StringPiece pool_label,
    ThreadPriority priority_hint,
    TrackedRef<TaskTracker> task_tracker,
    TrackedRef<Delegate> delegate)
    : SchedulerWorkerPool(std::move(task_tracker),
                          std::move(delegate)),
      pool_label_(pool_label.as_string()),
      priority_hint_(priority_hint),
      lock_(shared_priority_queue_.container_lock()),
      idle_workers_stack_cv_for_testing_(lock_.CreateConditionVariable()),
      // Mimics the UMA_HISTOGRAM_LONG_TIMES macro.
      detach_duration_histogram_(Histogram::FactoryTimeGet(
          JoinString({kDetachDurationHistogramPrefix, histogram_label,
                      kPoolNameSuffix},
                     ""),
          TimeDelta::FromMilliseconds(1),
          TimeDelta::FromHours(1),
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      // Mimics the UMA_HISTOGRAM_COUNTS_1000 macro. When a worker runs more
      // than 1000 tasks before detaching, there is no need to know the exact
      // number of tasks that ran.
      num_tasks_before_detach_histogram_(Histogram::FactoryGet(
          JoinString({kNumTasksBeforeDetachHistogramPrefix, histogram_label,
                      kPoolNameSuffix},
                     ""),
          1,
          1000,
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      // Mimics the UMA_HISTOGRAM_COUNTS_100 macro. A SchedulerWorker is
      // expected to run between zero and a few tens of tasks between waits.
      // When it runs more than 100 tasks, there is no need to know the exact
      // number of tasks that ran.
      num_tasks_between_waits_histogram_(Histogram::FactoryGet(
          JoinString({kNumTasksBetweenWaitsHistogramPrefix, histogram_label,
                      kPoolNameSuffix},
                     ""),
          1,
          100,
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      // Mimics the UMA_HISTOGRAM_COUNTS_100 macro. A SchedulerWorkerPool is
      // expected to run between zero and a few tens of workers.
      // When it runs more than 100 worker, there is no need to know the exact
      // number of workers that ran.
      num_workers_histogram_(Histogram::FactoryGet(
          JoinString(
              {kNumThreadsHistogramPrefix, histogram_label, kPoolNameSuffix},
              ""),
          1,
          100,
          50,
          HistogramBase::kUmaTargetedHistogramFlag)),
      tracked_ref_factory_(this) {
  DCHECK(!histogram_label.empty());
  DCHECK(!pool_label_.empty());
}

void SchedulerWorkerPoolImpl::Start(
    const SchedulerWorkerPoolParams& params,
    int max_best_effort_tasks,
    scoped_refptr<TaskRunner> service_thread_task_runner,
    SchedulerWorkerObserver* scheduler_worker_observer,
    WorkerEnvironment worker_environment) {
  AutoSchedulerLock auto_lock(lock_);

  DCHECK(workers_.empty());

  max_tasks_ = params.max_tasks();
  DCHECK_GE(max_tasks_, 1U);
  initial_max_tasks_ = max_tasks_;
  DCHECK_LE(initial_max_tasks_, kMaxNumberOfWorkers);
  max_best_effort_tasks_ = max_best_effort_tasks;
  suggested_reclaim_time_ = params.suggested_reclaim_time();
  backward_compatibility_ = params.backward_compatibility();
  worker_environment_ = worker_environment;

  service_thread_task_runner_ = std::move(service_thread_task_runner);

  DCHECK(!scheduler_worker_observer_);
  scheduler_worker_observer_ = scheduler_worker_observer;

  // The initial number of workers is |num_wake_ups_before_start_| + 1 to try to
  // keep one at least one standby thread at all times (capacity permitting).
  const int num_initial_workers =
      std::min(num_wake_ups_before_start_ + 1, static_cast<int>(max_tasks_));
  workers_.reserve(num_initial_workers);

  for (int index = 0; index < num_initial_workers; ++index) {
    SchedulerWorker* worker =
        CreateRegisterAndStartSchedulerWorkerLockRequired();

    // CHECK that the first worker can be started (assume that failure means
    // that threads can't be created on this machine).
    CHECK(worker || index > 0);

    if (worker) {
      if (index < num_wake_ups_before_start_) {
        worker->WakeUp();
      } else {
        idle_workers_stack_.Push(worker);
      }
    }
  }
}

SchedulerWorkerPoolImpl::~SchedulerWorkerPoolImpl() {
  // SchedulerWorkerPool should only ever be deleted:
  //  1) In tests, after JoinForTesting().
  //  2) In production, iff initialization failed.
  // In both cases |workers_| should be empty.
  DCHECK(workers_.empty());
}

void SchedulerWorkerPoolImpl::OnCanScheduleSequence(
    scoped_refptr<Sequence> sequence) {
  PushSequenceToPriorityQueue(std::move(sequence));
  WakeUpOneWorker();
}

void SchedulerWorkerPoolImpl::PushSequenceToPriorityQueue(
    scoped_refptr<Sequence> sequence) {
  DCHECK(sequence);
  const auto sequence_sort_key = sequence->GetSortKey();
  shared_priority_queue_.BeginTransaction()->Push(std::move(sequence),
                                                  sequence_sort_key);
}

void SchedulerWorkerPoolImpl::GetHistograms(
    std::vector<const HistogramBase*>* histograms) const {
  histograms->push_back(detach_duration_histogram_);
  histograms->push_back(num_tasks_between_waits_histogram_);
  histograms->push_back(num_workers_histogram_);
}

int SchedulerWorkerPoolImpl::GetMaxConcurrentNonBlockedTasksDeprecated() const {
#if DCHECK_IS_ON()
  AutoSchedulerLock auto_lock(lock_);
  DCHECK_NE(initial_max_tasks_, 0U)
      << "GetMaxConcurrentTasksDeprecated() should only be called after the "
      << "worker pool has started.";
#endif
  return initial_max_tasks_;
}

void SchedulerWorkerPoolImpl::WaitForWorkersIdleForTesting(size_t n) {
  AutoSchedulerLock auto_lock(lock_);

#if DCHECK_IS_ON()
  DCHECK(!some_workers_cleaned_up_for_testing_)
      << "Workers detached prior to waiting for a specific number of idle "
         "workers. Doing the wait under such conditions is flaky. Consider "
         "using |suggested_reclaim_time_ = TimeDelta::Max()| for this test.";
#endif

  WaitForWorkersIdleLockRequiredForTesting(n);
}

void SchedulerWorkerPoolImpl::WaitForAllWorkersIdleForTesting() {
  AutoSchedulerLock auto_lock(lock_);
  WaitForWorkersIdleLockRequiredForTesting(workers_.size());
}

void SchedulerWorkerPoolImpl::WaitForWorkersCleanedUpForTesting(size_t n) {
  AutoSchedulerLock auto_lock(lock_);

  if (!num_workers_cleaned_up_for_testing_cv_)
    num_workers_cleaned_up_for_testing_cv_ = lock_.CreateConditionVariable();

  while (num_workers_cleaned_up_for_testing_ < n)
    num_workers_cleaned_up_for_testing_cv_->Wait();

  num_workers_cleaned_up_for_testing_ = 0;
}

void SchedulerWorkerPoolImpl::JoinForTesting() {
#if DCHECK_IS_ON()
  join_for_testing_started_.Set();
#endif

  decltype(workers_) workers_copy;
  {
    AutoSchedulerLock auto_lock(lock_);

    DCHECK_GT(workers_.size(), size_t(0)) << "Joined an unstarted worker pool.";

    // Ensure SchedulerWorkers in |workers_| do not attempt to cleanup while
    // being joined.
    worker_cleanup_disallowed_for_testing_ = true;

    // Make a copy of the SchedulerWorkers so that we can call
    // SchedulerWorker::JoinForTesting() without holding |lock_| since
    // SchedulerWorkers may need to access |workers_|.
    workers_copy = workers_;
  }
  for (const auto& worker : workers_copy)
    worker->JoinForTesting();

  AutoSchedulerLock auto_lock(lock_);
  DCHECK(workers_ == workers_copy);
  // Release |workers_| to clear their TrackedRef against |this|.
  workers_.clear();
}

void SchedulerWorkerPoolImpl::ReEnqueueSequence(
    scoped_refptr<Sequence> sequence) {
  PushSequenceToPriorityQueue(std::move(sequence));
  if (!IsBoundToCurrentThread())
    WakeUpOneWorker();
}

size_t SchedulerWorkerPoolImpl::NumberOfWorkersForTesting() const {
  AutoSchedulerLock auto_lock(lock_);
  return workers_.size();
}

size_t SchedulerWorkerPoolImpl::GetMaxTasksForTesting() const {
  AutoSchedulerLock auto_lock(lock_);
  return max_tasks_;
}

size_t SchedulerWorkerPoolImpl::NumberOfIdleWorkersForTesting() const {
  AutoSchedulerLock auto_lock(lock_);
  return idle_workers_stack_.Size();
}

void SchedulerWorkerPoolImpl::MaximizeMayBlockThresholdForTesting() {
  maximum_blocked_threshold_for_testing_.Set();
}

void SchedulerWorkerPoolImpl::RecordNumWorkersHistogram() const {
  AutoSchedulerLock auto_lock(lock_);
  num_workers_histogram_->Add(workers_.size());
}

SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    SchedulerWorkerDelegateImpl(TrackedRef<SchedulerWorkerPoolImpl> outer)
    : outer_(std::move(outer)) {
  // Bound in OnMainEntry().
  DETACH_FROM_THREAD(worker_thread_checker_);
}

// OnMainExit() handles the thread-affine cleanup; SchedulerWorkerDelegateImpl
// can thereafter safely be deleted from any thread.
SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    ~SchedulerWorkerDelegateImpl() = default;

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    OnCanScheduleSequence(scoped_refptr<Sequence> sequence) {
  outer_->OnCanScheduleSequence(std::move(sequence));
}

SchedulerWorker::ThreadLabel
SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::GetThreadLabel() const {
  return SchedulerWorker::ThreadLabel::POOLED;
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::OnMainEntry(
    const SchedulerWorker* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  {
#if DCHECK_IS_ON()
    AutoSchedulerLock auto_lock(outer_->lock_);
    DCHECK(ContainsWorker(outer_->workers_, worker));
#endif
  }

#if defined(OS_WIN)
  if (outer_->worker_environment_ == WorkerEnvironment::COM_MTA) {
    if (win::GetVersion() >= win::VERSION_WIN8) {
      win_thread_environment_ = std::make_unique<win::ScopedWinrtInitializer>();
    } else {
      win_thread_environment_ = std::make_unique<win::ScopedCOMInitializer>(
          win::ScopedCOMInitializer::kMTA);
    }
    DCHECK(win_thread_environment_->Succeeded());
  }
#endif  // defined(OS_WIN)

  DCHECK_EQ(num_tasks_since_last_wait_, 0U);

  PlatformThread::SetName(
      StringPrintf("TaskScheduler%sWorker", outer_->pool_label_.c_str()));

  outer_->BindToCurrentThread();
  SetBlockingObserverForCurrentThread(this);
}

scoped_refptr<Sequence>
SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::GetWork(
    SchedulerWorker* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!is_running_task_);
  DCHECK(!is_running_best_effort_task_);

  {
    AutoSchedulerLock auto_lock(outer_->lock_);

    DCHECK(ContainsWorker(outer_->workers_, worker));

    // Calling GetWork() while on the idle worker stack indicates that we
    // must've reached GetWork() because of the WaitableEvent timing out. In
    // which case, we return no work and possibly cleanup the worker. To avoid
    // searching through the idle stack : use GetLastUsedTime() not being null
    // (or being directly on top of the idle stack) as a proxy for being on the
    // idle stack.
    const bool is_on_idle_workers_stack =
        outer_->idle_workers_stack_.Peek() == worker ||
        !worker->GetLastUsedTime().is_null();
    DCHECK_EQ(is_on_idle_workers_stack,
              outer_->idle_workers_stack_.Contains(worker));
    if (is_on_idle_workers_stack) {
      if (CanCleanupLockRequired(worker))
        CleanupLockRequired(worker);
      return nullptr;
    }

    // Excess workers should not get work, until they are no longer excess (i.e.
    // max tasks increases or another worker cleans up). This ensures that if we
    // have excess workers in the pool, they get a chance to no longer be excess
    // before being cleaned up.
    if (outer_->NumberOfExcessWorkersLockRequired() >
        outer_->idle_workers_stack_.Size()) {
      OnWorkerBecomesIdleLockRequired(worker);
      return nullptr;
    }
  }
  scoped_refptr<Sequence> sequence;
  {
    std::unique_ptr<PriorityQueue::Transaction> transaction(
        outer_->shared_priority_queue_.BeginTransaction());

    if (transaction->IsEmpty()) {
      // |transaction| is kept alive while |worker| is added to
      // |idle_workers_stack_| to avoid this race:
      // 1. This thread creates a Transaction, finds |shared_priority_queue_|
      //    empty and ends the Transaction.
      // 2. Other thread creates a Transaction, inserts a Sequence into
      //    |shared_priority_queue_| and ends the Transaction. This can't happen
      //    if the Transaction of step 1 is still active because because there
      //    can only be one active Transaction per PriorityQueue at a time.
      // 3. Other thread calls WakeUpOneWorker(). No thread is woken up because
      //    |idle_workers_stack_| is empty.
      // 4. This thread adds itself to |idle_workers_stack_| and goes to sleep.
      //    No thread runs the Sequence inserted in step 2.
      AutoSchedulerLock auto_lock(outer_->lock_);
      OnWorkerBecomesIdleLockRequired(worker);
      return nullptr;
    }

    // Enforce that no more than |max_best_effort_tasks_| run concurrently.
    const TaskPriority priority = transaction->PeekSortKey().priority();
    if (priority == TaskPriority::BEST_EFFORT) {
      AutoSchedulerLock auto_lock(outer_->lock_);
      if (outer_->num_running_best_effort_tasks_ <
          outer_->max_best_effort_tasks_) {
        ++outer_->num_running_best_effort_tasks_;
        is_running_best_effort_task_ = true;
      } else {
        OnWorkerBecomesIdleLockRequired(worker);
        return nullptr;
      }
    }

    sequence = transaction->PopSequence();
  }
  DCHECK(sequence);
#if DCHECK_IS_ON()
  {
    AutoSchedulerLock auto_lock(outer_->lock_);
    DCHECK(!outer_->idle_workers_stack_.Contains(worker));
  }
#endif

  is_running_task_ = true;
  return sequence;
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::DidRunTask() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(may_block_start_time_.is_null());
  DCHECK(!incremented_max_tasks_since_blocked_);
  DCHECK(is_running_task_);

  is_running_task_ = false;

  if (is_running_best_effort_task_) {
    AutoSchedulerLock auto_lock(outer_->lock_);
    --outer_->num_running_best_effort_tasks_;
    is_running_best_effort_task_ = false;
  }

  ++num_tasks_since_last_wait_;
  ++num_tasks_since_last_detach_;
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::ReEnqueueSequence(
    scoped_refptr<Sequence> sequence) {
  outer_->delegate_->ReEnqueueSequence(std::move(sequence));
}

TimeDelta
SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::GetSleepTimeout() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  // Sleep for an extra 10% to avoid the following pathological case:
  //   0) A task is running on a timer which matches |suggested_reclaim_time_|.
  //   1) The timer fires and this worker is created by
  //      MaintainAtLeastOneIdleWorkerLockRequired() because the last idle
  //      worker was assigned the task.
  //   2) This worker begins sleeping |suggested_reclaim_time_| (on top of the
  //      idle stack).
  //   3) The task assigned to the other worker completes and the worker goes
  //      back on the idle stack (this worker is now second on the idle stack;
  //      its GetLastUsedTime() is set to Now()).
  //   4) The sleep in (2) expires. Since (3) was fast this worker is likely to
  //      have been second on the idle stack long enough for
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
  //   |suggested_reclaim_time_ * 1.1| but it's expected that any timer slower
  //   than |suggested_reclaim_time_| will cause such churn during long idle
  //   periods. If this is a problem in practice, the standby thread
  //   configuration and algorithm should be revisited.
  return outer_->suggested_reclaim_time_ * 1.1;
}

bool SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    CanCleanupLockRequired(const SchedulerWorker* worker) const {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  const TimeTicks last_used_time = worker->GetLastUsedTime();
  return !last_used_time.is_null() &&
         TimeTicks::Now() - last_used_time >= outer_->suggested_reclaim_time_ &&
         LIKELY(!outer_->worker_cleanup_disallowed_for_testing_);
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::CleanupLockRequired(
    SchedulerWorker* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  outer_->lock_.AssertAcquired();
  outer_->num_tasks_before_detach_histogram_->Add(num_tasks_since_last_detach_);
  outer_->cleanup_timestamps_.push(TimeTicks::Now());
  worker->Cleanup();
  outer_->RemoveFromIdleWorkersStackLockRequired(worker);

  // Remove the worker from |workers_|.
  auto worker_iter =
      std::find(outer_->workers_.begin(), outer_->workers_.end(), worker);
  DCHECK(worker_iter != outer_->workers_.end());
  outer_->workers_.erase(worker_iter);

  ++outer_->num_workers_cleaned_up_for_testing_;
#if DCHECK_IS_ON()
  outer_->some_workers_cleaned_up_for_testing_ = true;
#endif
  if (outer_->num_workers_cleaned_up_for_testing_cv_)
    outer_->num_workers_cleaned_up_for_testing_cv_->Signal();
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    OnWorkerBecomesIdleLockRequired(SchedulerWorker* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  outer_->lock_.AssertAcquired();
  // Record the TaskScheduler.NumTasksBetweenWaits histogram. After GetWork()
  // returns nullptr, the SchedulerWorker will perform a wait on its
  // WaitableEvent, so we record how many tasks were ran since the last wait
  // here.
  outer_->num_tasks_between_waits_histogram_->Add(num_tasks_since_last_wait_);
  num_tasks_since_last_wait_ = 0;
  outer_->AddToIdleWorkersStackLockRequired(worker);
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::OnMainExit(
    SchedulerWorker* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

#if DCHECK_IS_ON()
  {
    bool shutdown_complete = outer_->task_tracker_->IsShutdownComplete();
    AutoSchedulerLock auto_lock(outer_->lock_);

    // |worker| should already have been removed from the idle workers stack and
    // |workers_| by the time the thread is about to exit. (except in the cases
    // where the pool is no longer going to be used - in which case, it's fine
    // for there to be invalid workers in the pool.
    if (!shutdown_complete && !outer_->join_for_testing_started_.IsSet()) {
      DCHECK(!outer_->idle_workers_stack_.Contains(worker));
      DCHECK(!ContainsWorker(outer_->workers_, worker));
    }
  }
#endif

#if defined(OS_WIN)
  win_thread_environment_.reset();
#endif  // defined(OS_WIN)
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::BlockingStarted(
    BlockingType blocking_type) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  // Blocking calls made outside of tasks should not influence the max tasks.
  if (!is_running_task_)
    return;

  switch (blocking_type) {
    case BlockingType::MAY_BLOCK:
      MayBlockEntered();
      break;
    case BlockingType::WILL_BLOCK:
      WillBlockEntered();
      break;
  }
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    BlockingTypeUpgraded() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  {
    AutoSchedulerLock auto_lock(outer_->lock_);

    // Don't do anything if a MAY_BLOCK ScopedBlockingCall instantiated in the
    // same scope already caused the max tasks to be incremented.
    if (incremented_max_tasks_since_blocked_)
      return;

    // Cancel the effect of a MAY_BLOCK ScopedBlockingCall instantiated in the
    // same scope.
    if (!may_block_start_time_.is_null()) {
      may_block_start_time_ = TimeTicks();
      --outer_->num_pending_may_block_workers_;
      if (is_running_best_effort_task_)
        --outer_->num_pending_best_effort_may_block_workers_;
    }
  }

  WillBlockEntered();
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::BlockingEnded() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  // Ignore blocking calls made outside of tasks.
  if (!is_running_task_)
    return;

  AutoSchedulerLock auto_lock(outer_->lock_);
  if (incremented_max_tasks_since_blocked_) {
    outer_->DecrementMaxTasksLockRequired(is_running_best_effort_task_);
  } else {
    DCHECK(!may_block_start_time_.is_null());
    --outer_->num_pending_may_block_workers_;
    if (is_running_best_effort_task_)
      --outer_->num_pending_best_effort_may_block_workers_;
  }

  incremented_max_tasks_since_blocked_ = false;
  may_block_start_time_ = TimeTicks();
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::MayBlockEntered() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  {
    AutoSchedulerLock auto_lock(outer_->lock_);

    DCHECK(!incremented_max_tasks_since_blocked_);
    DCHECK(may_block_start_time_.is_null());
    may_block_start_time_ = TimeTicks::Now();
    ++outer_->num_pending_may_block_workers_;
    if (is_running_best_effort_task_)
      ++outer_->num_pending_best_effort_may_block_workers_;
  }
  outer_->ScheduleAdjustMaxTasksIfNeeded();
}

void SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::WillBlockEntered() {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  bool wake_up_allowed = false;
  {
    std::unique_ptr<PriorityQueue::Transaction> transaction(
        outer_->shared_priority_queue_.BeginTransaction());
    AutoSchedulerLock auto_lock(outer_->lock_);

    DCHECK(!incremented_max_tasks_since_blocked_);
    DCHECK(may_block_start_time_.is_null());
    incremented_max_tasks_since_blocked_ = true;
    outer_->IncrementMaxTasksLockRequired(is_running_best_effort_task_);

    // If the number of workers was less than the old max tasks, PostTask
    // would've handled creating extra workers during WakeUpOneWorker.
    // Therefore, we don't need to do anything here.
    if (outer_->workers_.size() < outer_->max_tasks_ - 1)
      return;

    if (transaction->IsEmpty()) {
      outer_->MaintainAtLeastOneIdleWorkerLockRequired();
    } else {
      // TODO(crbug.com/757897): We may create extra workers in this case:
      // |workers.size()| was equal to the old |max_tasks_|, we had multiple
      // ScopedBlockingCalls in parallel and we had work on the PQ.
      wake_up_allowed = outer_->WakeUpOneWorkerLockRequired();
      // |wake_up_allowed| is true when the pool is started, and a WILL_BLOCK
      // scope cannot be entered before the pool starts.
      DCHECK(wake_up_allowed);
    }
  }
  // TODO(crbug.com/813857): This can be better handled in the PostTask()
  // codepath. We really only should do this if there are tasks pending.
  if (wake_up_allowed)
    outer_->ScheduleAdjustMaxTasksIfNeeded();
}

bool SchedulerWorkerPoolImpl::SchedulerWorkerDelegateImpl::
    MustIncrementMaxTasksLockRequired() {
  outer_->lock_.AssertAcquired();

  if (!incremented_max_tasks_since_blocked_ &&
      !may_block_start_time_.is_null() &&
      TimeTicks::Now() - may_block_start_time_ >= outer_->MayBlockThreshold()) {
    incremented_max_tasks_since_blocked_ = true;

    // Reset |may_block_start_time_| so that BlockingScopeExited() knows that it
    // doesn't have to decrement the number of pending MAY_BLOCK workers.
    may_block_start_time_ = TimeTicks();
    --outer_->num_pending_may_block_workers_;
    if (is_running_best_effort_task_)
      --outer_->num_pending_best_effort_may_block_workers_;

    return true;
  }

  return false;
}

void SchedulerWorkerPoolImpl::WaitForWorkersIdleLockRequiredForTesting(
    size_t n) {
  lock_.AssertAcquired();

  // Make sure workers do not cleanup while watching the idle count.
  AutoReset<bool> ban_cleanups(&worker_cleanup_disallowed_for_testing_, true);

  while (idle_workers_stack_.Size() < n)
    idle_workers_stack_cv_for_testing_->Wait();
}

bool SchedulerWorkerPoolImpl::WakeUpOneWorkerLockRequired() {
  lock_.AssertAcquired();

  if (workers_.empty()) {
    ++num_wake_ups_before_start_;
    return false;
  }

  // Ensure that there is one worker that can run tasks on top of the idle
  // stack, capacity permitting.
  MaintainAtLeastOneIdleWorkerLockRequired();

  // If the worker on top of the idle stack can run tasks, wake it up.
  if (NumberOfExcessWorkersLockRequired() < idle_workers_stack_.Size()) {
    SchedulerWorker* worker = idle_workers_stack_.Pop();
    if (worker) {
      worker->WakeUp();
    }
  }

  // Ensure that there is one worker that can run tasks on top of the idle
  // stack, capacity permitting.
  MaintainAtLeastOneIdleWorkerLockRequired();

  return true;
}

void SchedulerWorkerPoolImpl::WakeUpOneWorker() {
  bool wake_up_allowed;
  {
    AutoSchedulerLock auto_lock(lock_);
    wake_up_allowed = WakeUpOneWorkerLockRequired();
  }
  if (wake_up_allowed)
    ScheduleAdjustMaxTasksIfNeeded();
}

void SchedulerWorkerPoolImpl::MaintainAtLeastOneIdleWorkerLockRequired() {
  lock_.AssertAcquired();

  if (workers_.size() == kMaxNumberOfWorkers)
    return;
  DCHECK_LT(workers_.size(), kMaxNumberOfWorkers);

  if (idle_workers_stack_.IsEmpty() && workers_.size() < max_tasks_) {
    SchedulerWorker* new_worker =
        CreateRegisterAndStartSchedulerWorkerLockRequired();
    if (new_worker)
      idle_workers_stack_.Push(new_worker);
  }
}

void SchedulerWorkerPoolImpl::AddToIdleWorkersStackLockRequired(
    SchedulerWorker* worker) {
  lock_.AssertAcquired();

  DCHECK(!idle_workers_stack_.Contains(worker));
  idle_workers_stack_.Push(worker);

  DCHECK_LE(idle_workers_stack_.Size(), workers_.size());

  idle_workers_stack_cv_for_testing_->Broadcast();
}

void SchedulerWorkerPoolImpl::RemoveFromIdleWorkersStackLockRequired(
    SchedulerWorker* worker) {
  lock_.AssertAcquired();
  idle_workers_stack_.Remove(worker);
}

SchedulerWorker*
SchedulerWorkerPoolImpl::CreateRegisterAndStartSchedulerWorkerLockRequired() {
  lock_.AssertAcquired();

  DCHECK_LT(workers_.size(), max_tasks_);
  DCHECK_LT(workers_.size(), kMaxNumberOfWorkers);
  // SchedulerWorker needs |lock_| as a predecessor for its thread lock
  // because in WakeUpOneWorker, |lock_| is first acquired and then
  // the thread lock is acquired when WakeUp is called on the worker.
  scoped_refptr<SchedulerWorker> worker = MakeRefCounted<SchedulerWorker>(
      priority_hint_,
      std::make_unique<SchedulerWorkerDelegateImpl>(
          tracked_ref_factory_.GetTrackedRef()),
      task_tracker_, &lock_, backward_compatibility_);

  if (!worker->Start(scheduler_worker_observer_))
    return nullptr;

  workers_.push_back(worker);
  DCHECK_LE(workers_.size(), max_tasks_);

  if (!cleanup_timestamps_.empty()) {
    detach_duration_histogram_->AddTime(TimeTicks::Now() -
                                        cleanup_timestamps_.top());
    cleanup_timestamps_.pop();
  }
  return worker.get();
}

size_t SchedulerWorkerPoolImpl::NumberOfExcessWorkersLockRequired() const {
  lock_.AssertAcquired();
  return std::max<int>(0, workers_.size() - max_tasks_);
}

void SchedulerWorkerPoolImpl::AdjustMaxTasks() {
  DCHECK(service_thread_task_runner_->RunsTasksInCurrentSequence());

  std::unique_ptr<PriorityQueue::Transaction> transaction(
      shared_priority_queue_.BeginTransaction());
  AutoSchedulerLock auto_lock(lock_);

  const size_t previous_max_tasks = max_tasks_;

  // Increment max tasks for each worker that has been within a MAY_BLOCK
  // ScopedBlockingCall for more than MayBlockThreshold().
  for (scoped_refptr<SchedulerWorker> worker : workers_) {
    // The delegates of workers inside a SchedulerWorkerPoolImpl should be
    // SchedulerWorkerDelegateImpls.
    SchedulerWorkerDelegateImpl* delegate =
        static_cast<SchedulerWorkerDelegateImpl*>(worker->delegate());
    if (delegate->MustIncrementMaxTasksLockRequired()) {
      IncrementMaxTasksLockRequired(
          delegate->is_running_best_effort_task_lock_required());
    }
  }

  // Wake up a worker per pending sequence, capacity permitting.
  const size_t num_pending_sequences = transaction->Size();
  const size_t num_wake_ups_needed =
      std::min(max_tasks_ - previous_max_tasks, num_pending_sequences);

  for (size_t i = 0; i < num_wake_ups_needed; ++i) {
    // No need to call ScheduleAdjustMaxTasksIfNeeded() as the caller will
    // take care of that for us.
    WakeUpOneWorkerLockRequired();
  }

  MaintainAtLeastOneIdleWorkerLockRequired();
}

TimeDelta SchedulerWorkerPoolImpl::MayBlockThreshold() const {
  if (maximum_blocked_threshold_for_testing_.IsSet())
    return TimeDelta::Max();
  // This value was set unscientifically based on intuition and may be adjusted
  // in the future. This value is smaller than |kBlockedWorkersPollPeriod|
  // because we hope than when multiple workers block around the same time, a
  // single AdjustMaxTasks() call will perform all the necessary max tasks
  // adjustments.
  return TimeDelta::FromMilliseconds(10);
}

void SchedulerWorkerPoolImpl::ScheduleAdjustMaxTasksIfNeeded() {
  {
    AutoSchedulerLock auto_lock(lock_);
    if (polling_max_tasks_ || !ShouldPeriodicallyAdjustMaxTasksLockRequired()) {
      return;
    }
    polling_max_tasks_ = true;
  }
  service_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      BindOnce(&SchedulerWorkerPoolImpl::AdjustMaxTasksFunction,
               Unretained(this)),
      kBlockedWorkersPollPeriod);
}

void SchedulerWorkerPoolImpl::AdjustMaxTasksFunction() {
  DCHECK(service_thread_task_runner_->RunsTasksInCurrentSequence());

  AdjustMaxTasks();
  {
    AutoSchedulerLock auto_lock(lock_);
    DCHECK(polling_max_tasks_);

    if (!ShouldPeriodicallyAdjustMaxTasksLockRequired()) {
      polling_max_tasks_ = false;
      return;
    }
  }
  service_thread_task_runner_->PostDelayedTask(
      FROM_HERE,
      BindOnce(&SchedulerWorkerPoolImpl::AdjustMaxTasksFunction,
               Unretained(this)),
      kBlockedWorkersPollPeriod);
}

bool SchedulerWorkerPoolImpl::ShouldPeriodicallyAdjustMaxTasksLockRequired() {
  lock_.AssertAcquired();

  // The maximum number of best-effort tasks that can run concurrently must be
  // adjusted periodically when (1) the number of best-effort tasks that are
  // currently running is equal to it and (2) there are workers running
  // best-effort tasks within the scope of a MAY_BLOCK ScopedBlockingCall but
  // haven't cause a max best-effort tasks increment yet.
  // - When (1) is false: A newly posted best-effort task will be allowed to run
  //   normally. There is no hurry to increase max best-effort tasks.
  // - When (2) is false: AdjustMaxTasks() wouldn't affect
  //   |max_best_effort_tasks_|.
  if (num_running_best_effort_tasks_ >= max_best_effort_tasks_ &&
      num_pending_best_effort_may_block_workers_ > 0) {
    return true;
  }

  // The maximum number of tasks that can run concurrently must be adjusted
  // periodically when (1) there are no idle workers that can do work (2) there
  // are workers that are within the scope of a MAY_BLOCK ScopedBlockingCall but
  // haven't cause a max tasks increment yet.
  // - When (1) is false: A newly posted task will run on one of the idle
  //   workers that are allowed to do work. There is no hurry to increase max
  //   tasks.
  // - When (2) is false: AdjustMaxTasks() wouldn't affect |max_tasks_|.
  const int idle_workers_that_can_do_work =
      idle_workers_stack_.Size() - NumberOfExcessWorkersLockRequired();
  return idle_workers_that_can_do_work <= 0 &&
         num_pending_may_block_workers_ > 0;
}

void SchedulerWorkerPoolImpl::DecrementMaxTasksLockRequired(
    bool is_running_best_effort_task) {
  lock_.AssertAcquired();
  --max_tasks_;
  if (is_running_best_effort_task)
    --max_best_effort_tasks_;
}

void SchedulerWorkerPoolImpl::IncrementMaxTasksLockRequired(
    bool is_running_best_effort_task) {
  lock_.AssertAcquired();
  ++max_tasks_;
  if (is_running_best_effort_task)
    ++max_best_effort_tasks_;
}

}  // namespace internal
}  // namespace base
