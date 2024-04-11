// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_THREAD_POOL_THREAD_GROUP_SEMAPHORE_H_
#define BASE_TASK_THREAD_POOL_THREAD_GROUP_SEMAPHORE_H_

#include <optional>
#include <string_view>

#include "base/base_export.h"
#include "base/task/thread_pool/task_source.h"
#include "base/task/thread_pool/thread_group_impl.h"
#include "base/task/thread_pool/worker_thread_semaphore.h"

namespace base {

class WorkerThreadObserver;

namespace internal {

class TaskTracker;

// A group of `WorkerThreadSemaphore`s that run `Task`s.
class BASE_EXPORT ThreadGroupSemaphore : public ThreadGroup {
 public:
  // Constructs a group without workers.
  //
  // `histogram_label` is used to label the thread group's histograms as
  // "ThreadPool." + histogram_name + "." + `histogram_label` + extra suffixes.
  // It must not be empty. `thread_group_label` is used to label the thread
  // group's threads, it must not be empty. `thread_type_hint` is the preferred
  // thread type; the actual thread type depends on shutdown state and platform
  // capabilities. `task_tracker` keeps track of tasks.
  ThreadGroupSemaphore(std::string_view histogram_label,
                       std::string_view thread_group_label,
                       ThreadType thread_type_hint,
                       TrackedRef<TaskTracker> task_tracker,
                       TrackedRef<Delegate> delegate);

  ThreadGroupSemaphore(const ThreadGroupSemaphore&) = delete;
  ThreadGroupSemaphore& operator=(const ThreadGroupSemaphore&) = delete;
  // Destroying a ThreadGroupSemaphore returned by Create() is not allowed
  // in production; it is always leaked. In tests, it can only be destroyed
  // after JoinForTesting() has returned.
  ~ThreadGroupSemaphore() override;

  // ThreadGroup:
  void Start(size_t max_tasks,
             size_t max_best_effort_tasks,
             TimeDelta suggested_reclaim_time,
             scoped_refptr<SingleThreadTaskRunner> service_thread_task_runner,
             WorkerThreadObserver* worker_thread_observer,
             WorkerEnvironment worker_environment,
             bool synchronous_thread_start_for_testing = false,
             std::optional<TimeDelta> may_block_threshold =
                 std::optional<TimeDelta>()) override;
  void JoinForTesting() override;
  void DidUpdateCanRunPolicy() override;
  void OnShutdownStarted() override;
  std::unique_ptr<BaseScopedCommandsExecutor> GetExecutor() override;
  size_t NumberOfIdleWorkersLockRequiredForTesting() const
      EXCLUSIVE_LOCKS_REQUIRED(lock_) override;

 private:
  class SemaphoreScopedCommandsExecutor;
  class SemaphoreWorkerDelegate;

  // friend tests so that they can access `blocked_workers_poll_period` and
  // may_block_threshold(), both in ThreadGroup.
  friend class ThreadGroupSemaphoreBlockingTest;
  friend class ThreadGroupSemaphoreMayBlockTest;
  FRIEND_TEST_ALL_PREFIXES(ThreadGroupSemaphoreBlockingTest,
                           ThreadBlockUnblockPremature);
  FRIEND_TEST_ALL_PREFIXES(ThreadGroupSemaphoreBlockingTest,
                           ThreadBlockUnblockPrematureBestEffort);

  // ThreadGroup:
  void UpdateSortKey(TaskSource::Transaction transaction) override;
  void PushTaskSourceAndWakeUpWorkers(
      RegisteredTaskSourceAndTransaction transaction_with_task_source) override;
  void EnsureEnoughWorkersLockRequired(BaseScopedCommandsExecutor* executor)
      override EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ThreadGroupWorkerDelegate* GetWorkerDelegate(WorkerThread* worker) override;

  void CreateAndRegisterWorkerLockRequired(
      SemaphoreScopedCommandsExecutor* executor)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Passed to SemaphoreWorkerDelegate.
  Semaphore semaphore_{0};
  AtomicFlag join_called_for_testing_{};

  // A worker (any worker) becomes 'signaled' when the semaphore is incremented,
  // and is no longer considered signaled at
  // OnWorkerBecomesIdleLockRequired(). Will not exceed `workers_.size()` due to
  // being deliberately capped in`EnsureEnoughWorkersLockRequired().
  size_t num_active_signals_ GUARDED_BY(lock_) = 0;

  size_t worker_sequence_num_ GUARDED_BY(lock_) = 0;

  // Ensures recently cleaned up workers (ref.
  // SemaphoreWorkerDelegate::CleanupLockRequired()) had time to exit as
  // they have a raw reference to `this` (and to TaskTracker) which can
  // otherwise result in racy use-after-frees per no longer being part of
  // `workers_` and hence not being explicitly joined in JoinForTesting():
  // https://crbug.com/810464. Uses AtomicRefCount to make its only public
  // method thread-safe.
  TrackedRefFactory<ThreadGroup> tracked_ref_factory_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_THREAD_POOL_THREAD_GROUP_SEMAPHORE_H_
