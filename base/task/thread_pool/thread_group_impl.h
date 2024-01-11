// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_THREAD_GROUP_IMPL_H_
#define BASE_TASK_THREAD_POOL_THREAD_GROUP_IMPL_H_

#include <vector>

#include "base/base_export.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool/task_source.h"
#include "base/task/thread_pool/thread_group.h"
#include "base/task/thread_pool/thread_group_worker_delegate.h"
#include "base/task/thread_pool/tracked_ref.h"
#include "base/task/thread_pool/worker_thread_set.h"
#include "base/task/thread_pool/worker_thread_waitable_event.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

class WorkerThreadObserver;

namespace internal {

class TaskTracker;

// A group of |WorkerThreadWaitableEvent|s that run |Task|s.
//
// The thread group doesn't create threads until Start() is called. Tasks can be
// posted at any time but will not run until after Start() is called.
//
// This class is thread-safe.
class BASE_EXPORT ThreadGroupImpl : public ThreadGroup {
 public:
  // Constructs a group without workers.
  //
  // |histogram_label| is used to label the thread group's histograms as
  // "ThreadPool." + histogram_name + "." + |histogram_label| + extra suffixes.
  // It must not be empty. |thread group_label| is used to label the thread
  // group's threads, it must not be empty. |thread_type_hint| is the preferred
  // thread type; the actual thread type depends on shutdown state and platform
  // capabilities. |task_tracker| keeps track of tasks.
  ThreadGroupImpl(StringPiece histogram_label,
                  StringPiece thread_group_label,
                  ThreadType thread_type_hint,
                  TrackedRef<TaskTracker> task_tracker,
                  TrackedRef<Delegate> delegate);

  // Creates threads, allowing existing and future tasks to run. The thread
  // group runs at most |max_tasks| / `max_best_effort_tasks` unblocked task
  // with any / BEST_EFFORT priority concurrently. It reclaims unused threads
  // after `suggested_reclaim_time`. It uses `service_thread_task_runner` to
  // monitor for blocked tasks, `service_thread_task_runner` is used to setup
  // FileDescriptorWatcher on worker threads. It must refer to a Thread with
  // MessagePumpType::IO. If specified, it notifies |worker_thread_observer|
  // when a worker enters and exits its main function (the observer must not be
  // destroyed before JoinForTesting() has returned). |worker_environment|
  // specifies the environment in which tasks are executed.
  // |may_block_threshold| is the timeout after which a task in a MAY_BLOCK
  // ScopedBlockingCall is considered blocked (the thread group will choose an
  // appropriate value if none is specified).
  // `synchronous_thread_start_for_testing` is true if this ThreadGroup
  // should synchronously wait for OnMainEntry() after starting each worker. Can
  // only be called once. CHECKs on failure.
  void Start(size_t max_tasks,
             size_t max_best_effort_tasks,
             TimeDelta suggested_reclaim_time,
             scoped_refptr<SingleThreadTaskRunner> service_thread_task_runner,
             WorkerThreadObserver* worker_thread_observer,
             WorkerEnvironment worker_environment,
             bool synchronous_thread_start_for_testing = false,
             absl::optional<TimeDelta> may_block_threshold =
                 absl::optional<TimeDelta>());

  ThreadGroupImpl(const ThreadGroupImpl&) = delete;
  ThreadGroupImpl& operator=(const ThreadGroupImpl&) = delete;
  // Destroying a ThreadGroupImpl returned by Create() is not allowed
  // in production; it is always leaked. In tests, it can only be destroyed
  // after JoinForTesting() has returned.
  ~ThreadGroupImpl() override;

  // ThreadGroup:
  void JoinForTesting() override;
  void DidUpdateCanRunPolicy() override;
  void OnShutdownStarted() override;
  std::unique_ptr<BaseScopedCommandsExecutor> GetExecutor() override;

  // Waits until at least |n| workers are idle. Note that while workers are
  // disallowed from cleaning up during this call: tests using a custom
  // |suggested_reclaim_time_| need to be careful to invoke this swiftly after
  // unblocking the waited upon workers as: if a worker is already detached by
  // the time this is invoked, it will never make it onto the idle set and
  // this call will hang.
  void WaitForWorkersIdleForTesting(size_t n);

  // Waits until at least |n| workers are idle.
  void WaitForWorkersIdleLockRequiredForTesting(size_t n)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Waits until all workers are idle.
  void WaitForAllWorkersIdleForTesting();

  // Waits until |n| workers have cleaned up (went through
  // WaitableEventWorkerDelegate::OnMainExit()) since the last call to
  // WaitForWorkersCleanedUpForTesting() (or Start() if that wasn't called yet).
  void WaitForWorkersCleanedUpForTesting(size_t n);

  // Returns the number of workers in this thread group.
  size_t NumberOfWorkersForTesting() const;
  // Returns the number of workers that are idle (i.e. not running tasks).
  size_t NumberOfIdleWorkersForTesting() const;

 private:
  class ScopedCommandsExecutor;
  class WaitableEventWorkerDelegate;

  // friend tests so that they can access |blocked_workers_poll_period| and
  // may_block_threshold(), both in ThreadGroup.
  friend class ThreadGroupImplBlockingTest;
  friend class ThreadGroupImplMayBlockTest;
  FRIEND_TEST_ALL_PREFIXES(ThreadGroupImplBlockingTest,
                           ThreadBlockUnblockPremature);
  FRIEND_TEST_ALL_PREFIXES(ThreadGroupImplBlockingTest,
                           ThreadBlockUnblockPrematureBestEffort);

  // ThreadGroup:
  void UpdateSortKey(TaskSource::Transaction transaction) override;
  void PushTaskSourceAndWakeUpWorkers(
      RegisteredTaskSourceAndTransaction transaction_with_task_source) override;
  void EnsureEnoughWorkersLockRequired(BaseScopedCommandsExecutor* executor)
      override EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void AdjustMaxTasks() override;
  // Creates a worker and schedules its start, if needed, to maintain one idle
  // worker, |max_tasks_| permitting.
  void MaintainAtLeastOneIdleWorkerLockRequired(
      ScopedCommandsExecutor* executor) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Creates a worker, adds it to the thread group, schedules its start and
  // returns it. Cannot be called before Start().
  scoped_refptr<WorkerThreadWaitableEvent> CreateAndRegisterWorkerLockRequired(
      ScopedCommandsExecutor* executor) EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool IsOnIdleSetLockRequired(WorkerThread* worker) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Returns the number of workers that are awake (i.e. not on the idle set).
  size_t GetNumAwakeWorkersLockRequired() const EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool IsOnIdleSetLockRequired(WorkerThreadWaitableEvent* worker) const
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // All workers owned by this thread group.
  std::vector<scoped_refptr<WorkerThreadWaitableEvent>> workers_
      GUARDED_BY(lock_);
  size_t worker_sequence_num_ GUARDED_BY(lock_) = 0;

  // Ordered set of idle workers; the order uses pointer comparison, this is
  // arbitrary but stable. Initially, all workers are on this set. A worker is
  // removed from the set before its WakeUp() function is called and when it
  // receives work from GetWork() (a worker calls GetWork() when its sleep
  // timeout expires, even if its WakeUp() method hasn't been called). A worker
  // is inserted on this set when it receives nullptr from GetWork().
  WorkerThreadSet idle_workers_set_ GUARDED_BY(lock_);

  // Signaled when a worker is added to the idle workers set.
  std::unique_ptr<ConditionVariable> idle_workers_set_cv_for_testing_
      GUARDED_BY(lock_);

  // Signaled, if non-null, when |num_workers_cleaned_up_for_testing_| is
  // incremented.
  std::unique_ptr<ConditionVariable> num_workers_cleaned_up_for_testing_cv_
      GUARDED_BY(lock_);

  // Set at the start of JoinForTesting().
  bool join_for_testing_started_ GUARDED_BY(lock_) = false;

  // Ensures recently cleaned up workers (ref.
  // WaitableEventWorkerDelegate::CleanupLockRequired()) had time to exit as
  // they have a raw reference to |this| (and to TaskTracker) which can
  // otherwise result in racy use-after-frees per no longer being part of
  // |workers_| and hence not being explicitly joined in JoinForTesting():
  // https://crbug.com/810464. Uses AtomicRefCount to make its only public
  // method thread-safe.
  TrackedRefFactory<ThreadGroup> tracked_ref_factory_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_THREAD_GROUP_IMPL_H_
