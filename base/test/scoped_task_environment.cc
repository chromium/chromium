// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_task_environment.h"

#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/post_task.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "base/task/task_scheduler/task_scheduler_impl.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/sequence_local_storage_map.h"
#include "base/threading/thread_local.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"

#if defined(OS_POSIX)
#include "base/files/file_descriptor_watcher_posix.h"
#endif

namespace base {
namespace test {

namespace {

LazyInstance<ThreadLocalPointer<ScopedTaskEnvironment::LifetimeObserver>>::Leaky
    environment_lifetime_observer;

std::unique_ptr<MessageLoop> CreateMessageLoopForMainThreadType(
    ScopedTaskEnvironment::MainThreadType main_thread_type) {
  switch (main_thread_type) {
    case ScopedTaskEnvironment::MainThreadType::DEFAULT:
      return std::make_unique<MessageLoop>(MessageLoop::TYPE_DEFAULT);
    case ScopedTaskEnvironment::MainThreadType::MOCK_TIME:
      return nullptr;
    case ScopedTaskEnvironment::MainThreadType::UI:
      return std::make_unique<MessageLoop>(MessageLoop::TYPE_UI);
    case ScopedTaskEnvironment::MainThreadType::IO:
      return std::make_unique<MessageLoop>(MessageLoop::TYPE_IO);
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace

class ScopedTaskEnvironment::TestTaskTracker
    : public internal::TaskSchedulerImpl::TaskTrackerImpl {
 public:
  TestTaskTracker();

  // Allow running tasks.
  void AllowRunTasks();

  // Disallow running tasks. Returns true on success; success requires there to
  // be no tasks currently running. Returns false if >0 tasks are currently
  // running. Prior to returning false, it will attempt to block until at least
  // one task has completed (in an attempt to avoid callers busy-looping
  // DisallowRunTasks() calls with the same set of slowly ongoing tasks). This
  // block attempt will also have a short timeout (in an attempt to prevent the
  // fallout of blocking: if the only task remaining is blocked on the main
  // thread, waiting for it to complete results in a deadlock...).
  bool DisallowRunTasks();

 private:
  friend class ScopedTaskEnvironment;

  // internal::TaskSchedulerImpl::TaskTrackerImpl:
  void RunOrSkipTask(internal::Task task,
                     internal::Sequence* sequence,
                     bool can_run_task) override;

  // Synchronizes accesses to members below.
  Lock lock_;

  // True if running tasks is allowed.
  bool can_run_tasks_ = true;

  // Signaled when |can_run_tasks_| becomes true.
  ConditionVariable can_run_tasks_cv_;

  // Signaled when a task is completed.
  ConditionVariable task_completed_;

  // Number of tasks that are currently running.
  int num_tasks_running_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestTaskTracker);
};

ScopedTaskEnvironment::ScopedTaskEnvironment(
    MainThreadType main_thread_type,
    ExecutionMode execution_control_mode)
    : execution_control_mode_(execution_control_mode),
      message_loop_(CreateMessageLoopForMainThreadType(main_thread_type)),
      mock_time_task_runner_(
          main_thread_type == MainThreadType::MOCK_TIME
              ? MakeRefCounted<TestMockTimeTaskRunner>(
                    TestMockTimeTaskRunner::Type::kBoundToThread)
              : nullptr),
      slsm_for_mock_time_(
          main_thread_type == MainThreadType::MOCK_TIME
              ? std::make_unique<internal::SequenceLocalStorageMap>()
              : nullptr),
      slsm_registration_for_mock_time_(
          main_thread_type == MainThreadType::MOCK_TIME
              ? std::make_unique<
                    internal::ScopedSetSequenceLocalStorageMapForCurrentThread>(
                    slsm_for_mock_time_.get())
              : nullptr),
#if defined(OS_POSIX)
      file_descriptor_watcher_(main_thread_type == MainThreadType::IO
                                   ? std::make_unique<FileDescriptorWatcher>(
                                         message_loop_->task_runner())
                                   : nullptr),
#endif  // defined(OS_POSIX)
      task_tracker_(new TestTaskTracker()) {
  CHECK(!TaskScheduler::GetInstance())
      << "Someone has already initialized TaskScheduler. If nothing in your "
         "test does so, then a test that ran earlier may have initialized one, "
         "and leaked it. base::TestSuite will trap leaked globals, unless "
         "someone has explicitly disabled it with "
         "DisableCheckForLeakedGlobals().";

  // Instantiate a TaskScheduler with 2 threads in each of its 4 pools. Threads
  // stay alive even when they don't have work.
  // Each pool uses two threads to prevent deadlocks in unit tests that have a
  // sequence that uses WithBaseSyncPrimitives() to wait on the result of
  // another sequence. This isn't perfect (doesn't solve wait chains) but solves
  // the basic use case for now.
  // TODO(fdoray/jeffreyhe): Make the TaskScheduler dynamically replace blocked
  // threads and get rid of this limitation. http://crbug.com/738104
  constexpr int kMaxThreads = 2;
  const TimeDelta kSuggestedReclaimTime = TimeDelta::Max();
  const SchedulerWorkerPoolParams worker_pool_params(kMaxThreads,
                                                     kSuggestedReclaimTime);
  TaskScheduler::SetInstance(std::make_unique<internal::TaskSchedulerImpl>(
      "ScopedTaskEnvironment", WrapUnique(task_tracker_)));
  task_scheduler_ = TaskScheduler::GetInstance();
  TaskScheduler::GetInstance()->Start({worker_pool_params, worker_pool_params,
                                       worker_pool_params, worker_pool_params});

  if (execution_control_mode_ == ExecutionMode::QUEUED)
    CHECK(task_tracker_->DisallowRunTasks());

  LifetimeObserver* observer = environment_lifetime_observer.Get().Get();
  if (observer) {
    observer->OnScopedTaskEnvironmentCreated(main_thread_type,
                                             GetMainThreadTaskRunner());
  }
}

ScopedTaskEnvironment::~ScopedTaskEnvironment() {
  // Ideally this would RunLoop().RunUntilIdle() here to catch any errors or
  // infinite post loop in the remaining work but this isn't possible right now
  // because base::~MessageLoop() didn't use to do this and adding it here would
  // make the migration away from MessageLoop that much harder.
  CHECK_EQ(TaskScheduler::GetInstance(), task_scheduler_);
  // Without FlushForTesting(), DeleteSoon() and ReleaseSoon() tasks could be
  // skipped, resulting in memory leaks.
  task_tracker_->AllowRunTasks();
  TaskScheduler::GetInstance()->FlushForTesting();
  TaskScheduler::GetInstance()->Shutdown();
  TaskScheduler::GetInstance()->JoinForTesting();
  // Destroying TaskScheduler state can result in waiting on worker threads.
  // Make sure this is allowed to avoid flaking tests that have disallowed waits
  // on their main thread.
  ScopedAllowBaseSyncPrimitivesForTesting allow_waits_to_destroy_task_tracker;
  TaskScheduler::SetInstance(nullptr);

  LifetimeObserver* observer = environment_lifetime_observer.Get().Get();
  if (observer)
    observer->OnScopedTaskEnvironmentDestroyed();
}

void ScopedTaskEnvironment::SetLifetimeObserver(
    ScopedTaskEnvironment::LifetimeObserver* lifetime_observer) {
  DCHECK_NE(!!environment_lifetime_observer.Get().Get(), !!lifetime_observer);
  environment_lifetime_observer.Get().Set(lifetime_observer);
}

scoped_refptr<base::SingleThreadTaskRunner>
ScopedTaskEnvironment::GetMainThreadTaskRunner() {
  if (message_loop_)
    return message_loop_->task_runner();
  DCHECK(mock_time_task_runner_);
  return mock_time_task_runner_;
}

bool ScopedTaskEnvironment::MainThreadHasPendingTask() const {
  if (message_loop_)
    return !message_loop_->IsIdleForTesting();
  DCHECK(mock_time_task_runner_);
  return mock_time_task_runner_->HasPendingTask();
}

void ScopedTaskEnvironment::RunUntilIdle() {
  // TODO(gab): This can be heavily simplified to essentially:
  //     bool HasMainThreadTasks() {
  //      if (message_loop_)
  //        return !message_loop_->IsIdleForTesting();
  //      return mock_time_task_runner_->NextPendingTaskDelay().is_zero();
  //     }
  //     while (task_tracker_->HasIncompleteTasks() || HasMainThreadTasks()) {
  //       base::RunLoop().RunUntilIdle();
  //       // Avoid busy-looping.
  //       if (task_tracker_->HasIncompleteTasks())
  //         PlatformThread::Sleep(TimeDelta::FromMilliSeconds(1));
  //     }
  // Update: This can likely be done now that MessageLoop::IsIdleForTesting()
  // checks all queues.
  //
  // Other than that it works because once |task_tracker_->HasIncompleteTasks()|
  // is false we know for sure that the only thing that can make it true is a
  // main thread task (ScopedTaskEnvironment owns all the threads). As such we
  // can't racily see it as false on the main thread and be wrong as if it the
  // main thread sees the atomic count at zero, it's the only one that can make
  // it go up. And the only thing that can make it go up on the main thread are
  // main thread tasks and therefore we're done if there aren't any left.
  //
  // This simplification further allows simplification of DisallowRunTasks().
  //
  // This can also be simplified even further once TaskTracker becomes directly
  // aware of main thread tasks. https://crbug.com/660078.

  for (;;) {
    task_tracker_->AllowRunTasks();

    // First run as many tasks as possible on the main thread in parallel with
    // tasks in TaskScheduler. This increases likelihood of TSAN catching
    // threading errors and eliminates possibility of hangs should a
    // TaskScheduler task synchronously block on a main thread task
    // (TaskScheduler::FlushForTesting() can't be used here for that reason).
    RunLoop().RunUntilIdle();

    // Then halt TaskScheduler. DisallowRunTasks() failing indicates that there
    // were TaskScheduler tasks currently running. In that case, try again from
    // top when DisallowRunTasks() yields control back to this thread as they
    // may have posted main thread tasks.
    if (!task_tracker_->DisallowRunTasks())
      continue;

    // Once TaskScheduler is halted. Run any remaining main thread tasks (which
    // may have been posted by TaskScheduler tasks that completed between the
    // above main thread RunUntilIdle() and TaskScheduler DisallowRunTasks()).
    // Note: this assumes that no main thread task synchronously blocks on a
    // TaskScheduler tasks (it certainly shouldn't); this call could otherwise
    // hang.
    RunLoop().RunUntilIdle();

    // The above RunUntilIdle() guarantees there are no remaining main thread
    // tasks (the TaskScheduler being halted during the last RunUntilIdle() is
    // key as it prevents a task being posted to it racily with it determining
    // it had no work remaining). Therefore, we're done if there is no more work
    // on TaskScheduler either (there can be TaskScheduler work remaining if
    // DisallowRunTasks() preempted work and/or the last RunUntilIdle() posted
    // more TaskScheduler tasks).
    // Note: this last |if| couldn't be turned into a |do {} while();|. A
    // conditional loop makes it such that |continue;| results in checking the
    // condition (not unconditionally loop again) which would be incorrect for
    // the above logic as it'd then be possible for a TaskScheduler task to be
    // running during the DisallowRunTasks() test, causing it to fail, but then
    // post to the main thread and complete before the loop's condition is
    // verified which could result in HasIncompleteUndelayedTasksForTesting()
    // returning false and the loop erroneously exiting with a pending task on
    // the main thread.
    if (!task_tracker_->HasIncompleteUndelayedTasksForTesting())
      break;
  }

  // The above loop always ends with running tasks being disallowed. Re-enable
  // parallel execution before returning unless in ExecutionMode::QUEUED.
  if (execution_control_mode_ != ExecutionMode::QUEUED)
    task_tracker_->AllowRunTasks();
}

void ScopedTaskEnvironment::FastForwardBy(TimeDelta delta) {
  DCHECK(mock_time_task_runner_);
  mock_time_task_runner_->FastForwardBy(delta);
}

void ScopedTaskEnvironment::FastForwardUntilNoTasksRemain() {
  DCHECK(mock_time_task_runner_);
  mock_time_task_runner_->FastForwardUntilNoTasksRemain();
}

const TickClock* ScopedTaskEnvironment::GetMockTickClock() {
  DCHECK(mock_time_task_runner_);
  return mock_time_task_runner_->GetMockTickClock();
}

std::unique_ptr<TickClock> ScopedTaskEnvironment::DeprecatedGetMockTickClock() {
  DCHECK(mock_time_task_runner_);
  return mock_time_task_runner_->DeprecatedGetMockTickClock();
}

base::TimeTicks ScopedTaskEnvironment::NowTicks() const {
  DCHECK(mock_time_task_runner_);
  return mock_time_task_runner_->NowTicks();
}

size_t ScopedTaskEnvironment::GetPendingMainThreadTaskCount() const {
  DCHECK(mock_time_task_runner_);
  return mock_time_task_runner_->GetPendingTaskCount();
}

TimeDelta ScopedTaskEnvironment::NextMainThreadPendingTaskDelay() const {
  DCHECK(mock_time_task_runner_);
  return mock_time_task_runner_->NextPendingTaskDelay();
}

ScopedTaskEnvironment::TestTaskTracker::TestTaskTracker()
    : internal::TaskSchedulerImpl::TaskTrackerImpl("ScopedTaskEnvironment"),
      can_run_tasks_cv_(&lock_),
      task_completed_(&lock_) {}

void ScopedTaskEnvironment::TestTaskTracker::AllowRunTasks() {
  AutoLock auto_lock(lock_);
  can_run_tasks_ = true;
  can_run_tasks_cv_.Broadcast();
}

bool ScopedTaskEnvironment::TestTaskTracker::DisallowRunTasks() {
  AutoLock auto_lock(lock_);

  // Can't disallow run task if there are tasks running.
  if (num_tasks_running_ > 0) {
    // Attempt to wait a bit so that the caller doesn't busy-loop with the same
    // set of pending work. A short wait is required to avoid deadlock
    // scenarios. See DisallowRunTasks()'s declaration for more details.
    task_completed_.TimedWait(TimeDelta::FromMilliseconds(1));
    return false;
  }

  can_run_tasks_ = false;
  return true;
}

void ScopedTaskEnvironment::TestTaskTracker::RunOrSkipTask(
    internal::Task task,
    internal::Sequence* sequence,
    bool can_run_task) {
  {
    AutoLock auto_lock(lock_);

    while (!can_run_tasks_)
      can_run_tasks_cv_.Wait();

    ++num_tasks_running_;
  }

  internal::TaskSchedulerImpl::TaskTrackerImpl::RunOrSkipTask(
      std::move(task), sequence, can_run_task);

  {
    AutoLock auto_lock(lock_);

    CHECK_GT(num_tasks_running_, 0);
    CHECK(can_run_tasks_);

    --num_tasks_running_;

    task_completed_.Broadcast();
  }
}

}  // namespace test
}  // namespace base
