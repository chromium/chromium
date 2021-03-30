// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool/can_run_policy_test.h"
#include "base/task/thread_pool/delayed_task_manager.h"
#include "base/task/thread_pool/pooled_sequenced_task_runner.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/test_task_factory.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/task/thread_pool/thread_group_impl.h"
#include "base/task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/task/thread_pool/thread_group_native_win.h"
#include "base/win/com_init_check_hook.h"
#include "base/win/com_init_util.h"
#elif defined(OS_APPLE)
#include "base/task/thread_pool/thread_group_native_mac.h"
#endif

namespace base {
namespace internal {

namespace {

#if HAS_NATIVE_THREAD_POOL()
using ThreadGroupNativeType =
#if defined(OS_WIN)
    ThreadGroupNativeWin;
#elif defined(OS_APPLE)
    ThreadGroupNativeMac;
#endif
#endif

constexpr size_t kMaxTasks = 4;
constexpr size_t kTooManyTasks = 1000;
// By default, tests allow half of the thread group to be used by best-effort
// tasks.
constexpr size_t kMaxBestEffortTasks = kMaxTasks / 2;
constexpr size_t kNumThreadsPostingTasks = 4;
constexpr size_t kNumTasksPostedPerThread = 150;

using PostNestedTask = test::TestTaskFactory::PostNestedTask;

class ThreadPostingTasks : public SimpleThread {
 public:
  // Constructs a thread that posts |num_tasks_posted_per_thread| tasks to
  // |thread_group| through an |execution_mode| task runner. If
  // |post_nested_task| is YES, each task posted by this thread posts another
  // task when it runs.
  ThreadPostingTasks(
      test::MockPooledTaskRunnerDelegate* mock_pooled_task_runner_delegate_,
      TaskSourceExecutionMode execution_mode,
      PostNestedTask post_nested_task)
      : SimpleThread("ThreadPostingTasks"),
        post_nested_task_(post_nested_task),
        factory_(test::CreatePooledTaskRunnerWithExecutionMode(
                     execution_mode,
                     mock_pooled_task_runner_delegate_),
                 execution_mode) {}
  ThreadPostingTasks(const ThreadPostingTasks&) = delete;
  ThreadPostingTasks& operator=(const ThreadPostingTasks&) = delete;

  const test::TestTaskFactory* factory() const { return &factory_; }

 private:
  void Run() override {
    for (size_t i = 0; i < kNumTasksPostedPerThread; ++i)
      EXPECT_TRUE(factory_.PostTask(post_nested_task_, OnceClosure()));
  }

  const scoped_refptr<TaskRunner> task_runner_;
  const PostNestedTask post_nested_task_;
  test::TestTaskFactory factory_;
};

class ThreadGroupTestBase : public testing::Test, public ThreadGroup::Delegate {
 public:
  ThreadGroupTestBase(const ThreadGroupTestBase&) = delete;
  ThreadGroupTestBase& operator=(const ThreadGroupTestBase&) = delete;

 protected:
  ThreadGroupTestBase() = default;

  void SetUp() override {
    service_thread_.Start();
    delayed_task_manager_.Start(service_thread_.task_runner());
    CreateThreadGroup();
  }

  void TearDown() override {
    service_thread_.Stop();
    if (thread_group_)
      thread_group_->JoinForTesting();
    thread_group_.reset();
  }

  void CreateThreadGroup() {
    ASSERT_FALSE(thread_group_);
    switch (GetGroupType()) {
      case test::GroupType::GENERIC:
        thread_group_ = std::make_unique<ThreadGroupImpl>(
            "TestThreadGroup", "A", ThreadPriority::NORMAL,
            task_tracker_.GetTrackedRef(),
            tracked_ref_factory_.GetTrackedRef());
        break;
#if HAS_NATIVE_THREAD_POOL()
      case test::GroupType::NATIVE:
        thread_group_ = std::make_unique<ThreadGroupNativeType>(
#if defined(OS_APPLE)
            ThreadPriority::NORMAL,
#endif
            task_tracker_.GetTrackedRef(),
            tracked_ref_factory_.GetTrackedRef());
        break;
#endif
    }
    ASSERT_TRUE(thread_group_);

    mock_pooled_task_runner_delegate_.SetThreadGroup(thread_group_.get());
  }

  void StartThreadGroup(ThreadGroup::WorkerEnvironment worker_environment =
                            ThreadGroup::WorkerEnvironment::NONE) {
    ASSERT_TRUE(thread_group_);
    switch (GetGroupType()) {
      case test::GroupType::GENERIC: {
        ThreadGroupImpl* thread_group_impl =
            static_cast<ThreadGroupImpl*>(thread_group_.get());
        thread_group_impl->Start(
            kMaxTasks, kMaxBestEffortTasks, TimeDelta::Max(),
            service_thread_.task_runner(), nullptr, worker_environment);
        break;
      }
#if HAS_NATIVE_THREAD_POOL()
      case test::GroupType::NATIVE: {
        ThreadGroupNativeType* thread_group_native_impl =
            static_cast<ThreadGroupNativeType*>(thread_group_.get());
        thread_group_native_impl->Start(worker_environment);
        break;
      }
#endif
    }
  }

  virtual test::GroupType GetGroupType() const = 0;

  Thread service_thread_{"ThreadPoolServiceThread"};
  TaskTracker task_tracker_;
  DelayedTaskManager delayed_task_manager_;
  test::MockPooledTaskRunnerDelegate mock_pooled_task_runner_delegate_ = {
      task_tracker_.GetTrackedRef(), &delayed_task_manager_};

  std::unique_ptr<ThreadGroup> thread_group_;

 private:
  // ThreadGroup::Delegate:
  ThreadGroup* GetThreadGroupForTraits(const TaskTraits& traits) override {
    return thread_group_.get();
  }

  TrackedRefFactory<ThreadGroup::Delegate> tracked_ref_factory_{this};
};

class ThreadGroupTest : public ThreadGroupTestBase,
                        public testing::WithParamInterface<test::GroupType> {
 public:
  ThreadGroupTest() = default;
  ThreadGroupTest(const ThreadGroupTest&) = delete;
  ThreadGroupTest& operator=(const ThreadGroupTest&) = delete;

  test::GroupType GetGroupType() const override { return GetParam(); }
};

// TODO(etiennep): Audit tests that don't need TaskSourceExecutionMode
// parameter.
class ThreadGroupTestAllExecutionModes
    : public ThreadGroupTestBase,
      public testing::WithParamInterface<
          std::tuple<test::GroupType, TaskSourceExecutionMode>> {
 public:
  ThreadGroupTestAllExecutionModes() = default;
  ThreadGroupTestAllExecutionModes(const ThreadGroupTestAllExecutionModes&) =
      delete;
  ThreadGroupTestAllExecutionModes& operator=(
      const ThreadGroupTestAllExecutionModes&) = delete;

  test::GroupType GetGroupType() const override {
    return std::get<0>(GetParam());
  }

  TaskSourceExecutionMode execution_mode() const {
    return std::get<1>(GetParam());
  }

  scoped_refptr<TaskRunner> CreatePooledTaskRunner(
      const TaskTraits& traits = {}) {
    return test::CreatePooledTaskRunnerWithExecutionMode(
        execution_mode(), &mock_pooled_task_runner_delegate_, traits);
  }
};

void ShouldNotRun() {
  ADD_FAILURE() << "Ran a task that shouldn't run.";
}

}  // namespace

TEST_P(ThreadGroupTestAllExecutionModes, PostTasks) {
  StartThreadGroup();
  // Create threads to post tasks.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(std::make_unique<ThreadPostingTasks>(
        &mock_pooled_task_runner_delegate_, execution_mode(),
        PostNestedTask::NO));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Flush the task tracker to be sure that no task accesses its TestTaskFactory
  // after |thread_posting_tasks| is destroyed.
  task_tracker_.FlushForTesting();
}

TEST_P(ThreadGroupTestAllExecutionModes, NestedPostTasks) {
  StartThreadGroup();
  // Create threads to post tasks. Each task posted by these threads will post
  // another task when it runs.
  std::vector<std::unique_ptr<ThreadPostingTasks>> threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(std::make_unique<ThreadPostingTasks>(
        &mock_pooled_task_runner_delegate_, execution_mode(),
        PostNestedTask::YES));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Flush the task tracker to be sure that no task accesses its TestTaskFactory
  // after |thread_posting_tasks| is destroyed.
  task_tracker_.FlushForTesting();
}

// Verify that a Task can't be posted after shutdown.
TEST_P(ThreadGroupTestAllExecutionModes, PostTaskAfterShutdown) {
  StartThreadGroup();
  auto task_runner = CreatePooledTaskRunner();
  test::ShutdownTaskTracker(&task_tracker_);
  EXPECT_FALSE(task_runner->PostTask(FROM_HERE, BindOnce(&ShouldNotRun)));
}

// Verify that a Task runs shortly after its delay expires.
TEST_P(ThreadGroupTestAllExecutionModes, PostDelayedTask) {
  StartThreadGroup();
  // kJob doesn't support delays.
  if (execution_mode() == TaskSourceExecutionMode::kJob)
    return;

  TestWaitableEvent task_ran(WaitableEvent::ResetPolicy::AUTOMATIC);
  auto task_runner = CreatePooledTaskRunner();

  // Wait until the task runner is up and running to make sure the test below is
  // solely timing the delayed task, not bringing up a physical thread.
  task_runner->PostTask(
      FROM_HERE, BindOnce(&TestWaitableEvent::Signal, Unretained(&task_ran)));
  task_ran.Wait();
  ASSERT_TRUE(!task_ran.IsSignaled());

  // Post a task with a short delay.
  const TimeTicks start_time = TimeTicks::Now();
  EXPECT_TRUE(task_runner->PostDelayedTask(
      FROM_HERE, BindOnce(&TestWaitableEvent::Signal, Unretained(&task_ran)),
      TestTimeouts::tiny_timeout()));

  // Wait until the task runs.
  task_ran.Wait();

  // Expect the task to run after its delay expires, but no more than a
  // reasonable amount of time after that (overloaded bots can be slow sometimes
  // so give it 10X flexibility).
  const TimeDelta actual_delay = TimeTicks::Now() - start_time;
  EXPECT_GE(actual_delay, TestTimeouts::tiny_timeout());
  EXPECT_LT(actual_delay, 10 * TestTimeouts::tiny_timeout());
}

// Verify that the RunsTasksInCurrentSequence() method of a SEQUENCED TaskRunner
// returns false when called from a task that isn't part of the sequence. Note:
// Tests that use TestTaskFactory already verify that
// RunsTasksInCurrentSequence() returns true when appropriate so this method
// complements it to get full coverage of that method.
TEST_P(ThreadGroupTestAllExecutionModes, SequencedRunsTasksInCurrentSequence) {
  StartThreadGroup();
  auto task_runner = CreatePooledTaskRunner();
  auto sequenced_task_runner = test::CreatePooledSequencedTaskRunner(
      TaskTraits(), &mock_pooled_task_runner_delegate_);

  TestWaitableEvent task_ran;
  task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](scoped_refptr<SequencedTaskRunner> sequenced_task_runner,
             TestWaitableEvent* task_ran) {
            EXPECT_FALSE(sequenced_task_runner->RunsTasksInCurrentSequence());
            task_ran->Signal();
          },
          sequenced_task_runner, Unretained(&task_ran)));
  task_ran.Wait();
}

// Verify that tasks posted before Start run after Start.
TEST_P(ThreadGroupTestAllExecutionModes, PostBeforeStart) {
  TestWaitableEvent task_1_running;
  TestWaitableEvent task_2_running;

  auto task_runner = CreatePooledTaskRunner();
  task_runner->PostTask(FROM_HERE, BindOnce(&TestWaitableEvent::Signal,
                                            Unretained(&task_1_running)));
  task_runner->PostTask(FROM_HERE, BindOnce(&TestWaitableEvent::Signal,
                                            Unretained(&task_2_running)));

  // Workers should not be created and tasks should not run before the thread
  // group is started. The sleep is to give time for the tasks to potentially
  // run.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_FALSE(task_1_running.IsSignaled());
  EXPECT_FALSE(task_2_running.IsSignaled());

  StartThreadGroup();

  // Tasks should run shortly after the thread group is started.
  task_1_running.Wait();
  task_2_running.Wait();

  task_tracker_.FlushForTesting();
}

// Verify that tasks only run when allowed by the CanRunPolicy.
TEST_P(ThreadGroupTestAllExecutionModes, CanRunPolicyBasic) {
  StartThreadGroup();
  test::TestCanRunPolicyBasic(
      thread_group_.get(),
      [this](TaskPriority priority) {
        return CreatePooledTaskRunner({priority});
      },
      &task_tracker_);
}

TEST_P(ThreadGroupTest, CanRunPolicyUpdatedBeforeRun) {
  StartThreadGroup();
  // This test only works with SequencedTaskRunner become it assumes
  // ordered execution of 2 posted tasks.
  test::TestCanRunPolicyChangedBeforeRun(
      thread_group_.get(),
      [this](TaskPriority priority) {
        return test::CreatePooledSequencedTaskRunner(
            {priority}, &mock_pooled_task_runner_delegate_);
      },
      &task_tracker_);
}

TEST_P(ThreadGroupTestAllExecutionModes, CanRunPolicyLoad) {
  StartThreadGroup();
  test::TestCanRunPolicyLoad(
      thread_group_.get(),
      [this](TaskPriority priority) {
        return CreatePooledTaskRunner({priority});
      },
      &task_tracker_);
}

// Verifies that ShouldYield() returns true for a priority that is not allowed
// to run by the CanRunPolicy.
TEST_P(ThreadGroupTest, CanRunPolicyShouldYield) {
  StartThreadGroup();

  task_tracker_.SetCanRunPolicy(CanRunPolicy::kNone);
  thread_group_->DidUpdateCanRunPolicy();
  EXPECT_TRUE(
      thread_group_->ShouldYield({TaskPriority::BEST_EFFORT, TimeTicks()}));
  EXPECT_TRUE(
      thread_group_->ShouldYield({TaskPriority::USER_VISIBLE, TimeTicks()}));

  task_tracker_.SetCanRunPolicy(CanRunPolicy::kForegroundOnly);
  thread_group_->DidUpdateCanRunPolicy();
  EXPECT_TRUE(
      thread_group_->ShouldYield({TaskPriority::BEST_EFFORT, TimeTicks()}));
  EXPECT_FALSE(
      thread_group_->ShouldYield({TaskPriority::USER_VISIBLE, TimeTicks()}));

  task_tracker_.SetCanRunPolicy(CanRunPolicy::kAll);
  thread_group_->DidUpdateCanRunPolicy();
  EXPECT_FALSE(
      thread_group_->ShouldYield({TaskPriority::BEST_EFFORT, TimeTicks()}));
  EXPECT_FALSE(
      thread_group_->ShouldYield({TaskPriority::USER_VISIBLE, TimeTicks()}));
}

// Verify that the maximum number of BEST_EFFORT tasks that can run concurrently
// in a thread group does not affect Sequences with a priority that was
// increased from BEST_EFFORT to USER_BLOCKING.
TEST_P(ThreadGroupTest, UpdatePriorityBestEffortToUserBlocking) {
  StartThreadGroup();

  CheckedLock num_tasks_running_lock;

  std::unique_ptr<ConditionVariable> num_tasks_running_cv =
      num_tasks_running_lock.CreateConditionVariable();
  num_tasks_running_cv->declare_only_used_while_idle();

  size_t num_tasks_running = 0;

  // Post |kMaxTasks| BEST_EFFORT tasks that block until they all start running.
  std::vector<scoped_refptr<PooledSequencedTaskRunner>> task_runners;

  for (size_t i = 0; i < kMaxTasks; ++i) {
    task_runners.push_back(MakeRefCounted<PooledSequencedTaskRunner>(
        TaskTraits(TaskPriority::BEST_EFFORT),
        &mock_pooled_task_runner_delegate_));
    task_runners.back()->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          // Increment the number of tasks running.
          {
            CheckedAutoLock auto_lock(num_tasks_running_lock);
            ++num_tasks_running;
          }
          num_tasks_running_cv->Broadcast();

          // Wait until all posted tasks are running.
          CheckedAutoLock auto_lock(num_tasks_running_lock);
          while (num_tasks_running < kMaxTasks)
            num_tasks_running_cv->Wait();
        }));
  }

  // Wait until |kMaxBestEffort| tasks start running.
  {
    CheckedAutoLock auto_lock(num_tasks_running_lock);
    while (num_tasks_running < kMaxBestEffortTasks)
      num_tasks_running_cv->Wait();
  }

  // Update the priority of all TaskRunners to USER_BLOCKING.
  for (size_t i = 0; i < kMaxTasks; ++i)
    task_runners[i]->UpdatePriority(TaskPriority::USER_BLOCKING);

  // Wait until all posted tasks start running. This should not block forever,
  // even in a thread group that enforces a maximum number of concurrent
  // BEST_EFFORT tasks lower than |kMaxTasks|.
  static_assert(kMaxBestEffortTasks < kMaxTasks, "");
  {
    CheckedAutoLock auto_lock(num_tasks_running_lock);
    while (num_tasks_running < kMaxTasks)
      num_tasks_running_cv->Wait();
  }

  task_tracker_.FlushForTesting();
}

// Regression test for crbug.com/955953.
TEST_P(ThreadGroupTestAllExecutionModes, ScopedBlockingCallTwice) {
  StartThreadGroup();
  auto task_runner = test::CreatePooledTaskRunnerWithExecutionMode(
      execution_mode(), &mock_pooled_task_runner_delegate_, {MayBlock()});

  TestWaitableEvent task_ran;
  task_runner->PostTask(FROM_HERE,
                        BindOnce(
                            [](TestWaitableEvent* task_ran) {
                              {
                                ScopedBlockingCall scoped_blocking_call(
                                    FROM_HERE, BlockingType::MAY_BLOCK);
                              }
                              {
                                ScopedBlockingCall scoped_blocking_call(
                                    FROM_HERE, BlockingType::MAY_BLOCK);
                              }
                              task_ran->Signal();
                            },
                            Unretained(&task_ran)));
  task_ran.Wait();
}

#if defined(OS_WIN)
TEST_P(ThreadGroupTestAllExecutionModes, COMMTAWorkerEnvironment) {
  StartThreadGroup(ThreadGroup::WorkerEnvironment::COM_MTA);
  auto task_runner = test::CreatePooledTaskRunnerWithExecutionMode(
      execution_mode(), &mock_pooled_task_runner_delegate_);

  TestWaitableEvent task_ran;
  task_runner->PostTask(
      FROM_HERE, BindOnce(
                     [](TestWaitableEvent* task_ran) {
                       win::AssertComApartmentType(win::ComApartmentType::MTA);
                       task_ran->Signal();
                     },
                     Unretained(&task_ran)));
  task_ran.Wait();
}

TEST_P(ThreadGroupTestAllExecutionModes, COMSTAWorkerEnvironment) {
  StartThreadGroup(ThreadGroup::WorkerEnvironment::COM_STA);
  auto task_runner = test::CreatePooledTaskRunnerWithExecutionMode(
      execution_mode(), &mock_pooled_task_runner_delegate_);

  TestWaitableEvent task_ran;
  task_runner->PostTask(
      FROM_HERE, BindOnce(
                     [](TestWaitableEvent* task_ran) {
  // COM STA is ignored when defined(COM_INIT_CHECK_HOOK_ENABLED). See comment
  // in ThreadGroup::GetScopedWindowsThreadEnvironment().
#if defined(COM_INIT_CHECK_HOOK_ENABLED)
                       win::AssertComApartmentType(win::ComApartmentType::NONE);
#else
                       win::AssertComApartmentType(win::ComApartmentType::STA);
#endif
                       task_ran->Signal();
                     },
                     Unretained(&task_ran)));
  task_ran.Wait();
}

TEST_P(ThreadGroupTestAllExecutionModes, NoWorkerEnvironment) {
  StartThreadGroup(ThreadGroup::WorkerEnvironment::NONE);
  auto task_runner = test::CreatePooledTaskRunnerWithExecutionMode(
      execution_mode(), &mock_pooled_task_runner_delegate_);

  TestWaitableEvent task_ran;
  task_runner->PostTask(
      FROM_HERE, BindOnce(
                     [](TestWaitableEvent* task_ran) {
                       win::AssertComApartmentType(win::ComApartmentType::NONE);
                       task_ran->Signal();
                     },
                     Unretained(&task_ran)));
  task_ran.Wait();
}
#endif

// Verifies that ShouldYield() returns false when there is no pending task.
TEST_P(ThreadGroupTest, ShouldYieldSingleTask) {
  StartThreadGroup();

  test::CreatePooledTaskRunner({TaskPriority::USER_BLOCKING},
                               &mock_pooled_task_runner_delegate_)
      ->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                   EXPECT_FALSE(thread_group_->ShouldYield(
                       {TaskPriority::BEST_EFFORT, TimeTicks::Now()}));
                   EXPECT_FALSE(thread_group_->ShouldYield(
                       {TaskPriority::USER_VISIBLE, TimeTicks::Now()}));
                   EXPECT_FALSE(thread_group_->ShouldYield(
                       {TaskPriority::USER_VISIBLE, TimeTicks::Now()}));
                 }));

  task_tracker_.FlushForTesting();
}

// Verify that tasks from a JobTaskSource run at the intended concurrency.
TEST_P(ThreadGroupTest, ScheduleJobTaskSource) {
  StartThreadGroup();

  TestWaitableEvent threads_running;
  TestWaitableEvent threads_continue;

  RepeatingClosure threads_running_barrier = BarrierClosure(
      kMaxTasks,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running)));

  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      BindLambdaForTesting(
          [&threads_running_barrier, &threads_continue](JobDelegate*) {
            threads_running_barrier.Run();
            threads_continue.Wait();
          }),
      /* num_tasks_to_run */ kMaxTasks);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {}, &mock_pooled_task_runner_delegate_);

  auto registered_task_source =
      task_tracker_.RegisterTaskSource(std::move(task_source));
  EXPECT_TRUE(registered_task_source);
  thread_group_->PushTaskSourceAndWakeUpWorkers(
      TransactionWithRegisteredTaskSource::FromTaskSource(
          std::move(registered_task_source)));

  threads_running.Wait();
  threads_continue.Signal();

  // Flush the task tracker to be sure that no local variables are accessed by
  // tasks after the end of the scope.
  task_tracker_.FlushForTesting();
}

// Verify that tasks from a JobTaskSource run at the intended concurrency.
TEST_P(ThreadGroupTest, ScheduleJobTaskSourceMultipleTime) {
  StartThreadGroup();

  TestWaitableEvent thread_running;
  TestWaitableEvent thread_continue;
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      BindLambdaForTesting([&thread_running, &thread_continue](JobDelegate*) {
        DCHECK(!thread_running.IsSignaled());
        thread_running.Signal();
        thread_continue.Wait();
      }),
      /* num_tasks_to_run */ 1);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {}, &mock_pooled_task_runner_delegate_);

  thread_group_->PushTaskSourceAndWakeUpWorkers(
      TransactionWithRegisteredTaskSource::FromTaskSource(
          task_tracker_.RegisterTaskSource(task_source)));

  // Enqueuing the task source again shouldn't affect the number of time it's
  // run.
  thread_group_->PushTaskSourceAndWakeUpWorkers(
      TransactionWithRegisteredTaskSource::FromTaskSource(
          task_tracker_.RegisterTaskSource(task_source)));

  thread_running.Wait();
  thread_continue.Signal();

  // Once the worker task ran, enqueuing the task source has no effect.
  thread_group_->PushTaskSourceAndWakeUpWorkers(
      TransactionWithRegisteredTaskSource::FromTaskSource(
          task_tracker_.RegisterTaskSource(task_source)));

  // Flush the task tracker to be sure that no local variables are accessed by
  // tasks after the end of the scope.
  task_tracker_.FlushForTesting();
}

// Verify that Cancel() on a job stops running the worker task and causes
// current workers to yield.
TEST_P(ThreadGroupTest, CancelJobTaskSource) {
  StartThreadGroup();

  CheckedLock tasks_running_lock;
  std::unique_ptr<ConditionVariable> tasks_running_cv =
      tasks_running_lock.CreateConditionVariable();
  bool tasks_running = false;

  // Schedule a big number of tasks.
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      BindLambdaForTesting([&](JobDelegate* delegate) {
        {
          CheckedAutoLock auto_lock(tasks_running_lock);
          tasks_running = true;
        }
        tasks_running_cv->Signal();

        while (!delegate->ShouldYield()) {
        }
      }),
      /* num_tasks_to_run */ kTooManyTasks);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {}, &mock_pooled_task_runner_delegate_);

  mock_pooled_task_runner_delegate_.EnqueueJobTaskSource(task_source);
  JobHandle job_handle = internal::JobTaskSource::CreateJobHandle(task_source);

  // Wait for at least 1 task to start running.
  {
    CheckedAutoLock auto_lock(tasks_running_lock);
    while (!tasks_running)
      tasks_running_cv->Wait();
  }

  // Cancels pending tasks and unblocks running ones.
  job_handle.Cancel();

  // This should not block since the job got cancelled.
  task_tracker_.FlushForTesting();
}

// Verify that calling JobTaskSource::NotifyConcurrencyIncrease() (re-)schedule
// tasks with the intended concurrency.
TEST_P(ThreadGroupTest, JobTaskSourceConcurrencyIncrease) {
  StartThreadGroup();

  TestWaitableEvent threads_running_a;
  TestWaitableEvent threads_continue;

  // Initially schedule half the tasks.
  RepeatingClosure threads_running_barrier = BarrierClosure(
      kMaxTasks / 2,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running_a)));

  auto job_state = base::MakeRefCounted<test::MockJobTask>(
      BindLambdaForTesting(
          [&threads_running_barrier, &threads_continue](JobDelegate*) {
            threads_running_barrier.Run();
            threads_continue.Wait();
          }),
      /* num_tasks_to_run */ kMaxTasks / 2);
  auto task_source = job_state->GetJobTaskSource(
      FROM_HERE, {}, &mock_pooled_task_runner_delegate_);

  auto registered_task_source = task_tracker_.RegisterTaskSource(task_source);
  EXPECT_TRUE(registered_task_source);
  thread_group_->PushTaskSourceAndWakeUpWorkers(
      TransactionWithRegisteredTaskSource::FromTaskSource(
          std::move(registered_task_source)));

  threads_running_a.Wait();
  // Reset |threads_running_barrier| for the remaining tasks.
  TestWaitableEvent threads_running_b;
  threads_running_barrier = BarrierClosure(
      kMaxTasks / 2,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running_b)));
  job_state->SetNumTasksToRun(kMaxTasks);

  // Unblocks tasks to let them racily wait for NotifyConcurrencyIncrease() to
  // be called.
  threads_continue.Signal();
  task_source->NotifyConcurrencyIncrease();
  // Wait for the remaining tasks. This should not block forever.
  threads_running_b.Wait();

  // Flush the task tracker to be sure that no local variables are accessed by
  // tasks after the end of the scope.
  task_tracker_.FlushForTesting();
}

// Verify that a JobTaskSource that becomes empty while in the queue eventually
// gets discarded.
TEST_P(ThreadGroupTest, ScheduleEmptyJobTaskSource) {
  StartThreadGroup();

  task_tracker_.SetCanRunPolicy(CanRunPolicy::kNone);

  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      BindRepeating([](JobDelegate*) { ShouldNotRun(); }),
      /* num_tasks_to_run */ 1);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {}, &mock_pooled_task_runner_delegate_);

  auto registered_task_source =
      task_tracker_.RegisterTaskSource(std::move(task_source));
  EXPECT_TRUE(registered_task_source);
  thread_group_->PushTaskSourceAndWakeUpWorkers(
      TransactionWithRegisteredTaskSource::FromTaskSource(
          std::move(registered_task_source)));

  // The worker task will never run.
  job_task->SetNumTasksToRun(0);

  task_tracker_.SetCanRunPolicy(CanRunPolicy::kAll);
  thread_group_->DidUpdateCanRunPolicy();

  // This should not block since there's no task to run.
  task_tracker_.FlushForTesting();
}

// Verify that Join() on a job contributes to max concurrency and waits for all
// workers to return.
TEST_P(ThreadGroupTest, JoinJobTaskSource) {
  StartThreadGroup();

  TestWaitableEvent threads_continue;
  RepeatingClosure threads_continue_barrier = BarrierClosure(
      kMaxTasks + 1,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_continue)));

  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      BindLambdaForTesting([&](JobDelegate*) {
        threads_continue_barrier.Run();
        threads_continue.Wait();
      }),
      /* num_tasks_to_run */ kMaxTasks + 1);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {}, &mock_pooled_task_runner_delegate_);

  mock_pooled_task_runner_delegate_.EnqueueJobTaskSource(task_source);
  JobHandle job_handle = internal::JobTaskSource::CreateJobHandle(task_source);
  job_handle.Join();
  // All worker tasks should complete before Join() returns.
  EXPECT_EQ(0U, job_task->GetMaxConcurrency(0));
  thread_group_->JoinForTesting();
  EXPECT_EQ(1U, task_source->HasOneRef());
  // Prevent TearDown() from calling JoinForTesting() again.
  thread_group_ = nullptr;
}

// Verify that finishing work outside of a job unblocks workers with a stale
// max concurrency.
TEST_P(ThreadGroupTest, JoinJobTaskSourceStaleConcurrency) {
  StartThreadGroup();

  TestWaitableEvent thread_running;
  std::atomic_size_t max_concurrency(1);
  auto task_source = MakeRefCounted<JobTaskSource>(
      FROM_HERE, TaskTraits{},
      BindLambdaForTesting([&](JobDelegate*) { thread_running.Signal(); }),
      BindLambdaForTesting(
          [&](size_t /*worker_count*/) -> size_t { return max_concurrency; }),
      &mock_pooled_task_runner_delegate_);

  mock_pooled_task_runner_delegate_.EnqueueJobTaskSource(task_source);
  JobHandle job_handle = internal::JobTaskSource::CreateJobHandle(task_source);
  thread_running.Wait();

  // Racily update max concurrency to unblock the task that was waiting on
  // NotifyMaxConcurrency().
  max_concurrency = 0;
  job_handle.Join();

  // This should not block since the job was joined.
  task_tracker_.FlushForTesting();
}

// Verify that cancelling a job unblocks workers with a stale max concurrency.
TEST_P(ThreadGroupTest, CancelJobTaskSourceWithStaleConcurrency) {
  StartThreadGroup();

  TestWaitableEvent thread_running;
  auto task_source = MakeRefCounted<JobTaskSource>(
      FROM_HERE, TaskTraits{},
      BindLambdaForTesting([&](JobDelegate*) { thread_running.Signal(); }),
      BindRepeating([](size_t /*worker_count*/) -> size_t { return 1; }),
      &mock_pooled_task_runner_delegate_);

  mock_pooled_task_runner_delegate_.EnqueueJobTaskSource(task_source);
  JobHandle job_handle = internal::JobTaskSource::CreateJobHandle(task_source);
  thread_running.Wait();
  job_handle.Cancel();

  // This should not block since the job got cancelled.
  task_tracker_.FlushForTesting();
}

// Verify that the maximum number of BEST_EFFORT tasks that can run concurrently
// in a thread group does not affect JobTaskSource with a priority that was
// increased from BEST_EFFORT to USER_BLOCKING.
TEST_P(ThreadGroupTest, JobTaskSourceUpdatePriority) {
  StartThreadGroup();

  CheckedLock num_tasks_running_lock;

  std::unique_ptr<ConditionVariable> num_tasks_running_cv =
      num_tasks_running_lock.CreateConditionVariable();
  num_tasks_running_cv->declare_only_used_while_idle();

  size_t num_tasks_running = 0;

  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      BindLambdaForTesting([&](JobDelegate*) {
        // Increment the number of tasks running.
        {
          CheckedAutoLock auto_lock(num_tasks_running_lock);
          ++num_tasks_running;
        }
        num_tasks_running_cv->Broadcast();

        // Wait until all posted tasks are running.
        CheckedAutoLock auto_lock(num_tasks_running_lock);
        while (num_tasks_running < kMaxTasks)
          num_tasks_running_cv->Wait();
      }),
      /* num_tasks_to_run */ kMaxTasks);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {TaskPriority::BEST_EFFORT},
                                 &mock_pooled_task_runner_delegate_);

  auto registered_task_source = task_tracker_.RegisterTaskSource(task_source);
  EXPECT_TRUE(registered_task_source);
  thread_group_->PushTaskSourceAndWakeUpWorkers(
      TransactionWithRegisteredTaskSource::FromTaskSource(
          std::move(registered_task_source)));

  // Wait until |kMaxBestEffort| tasks start running.
  {
    CheckedAutoLock auto_lock(num_tasks_running_lock);
    while (num_tasks_running < kMaxBestEffortTasks)
      num_tasks_running_cv->Wait();
  }

  // Update the priority to USER_BLOCKING.
  auto transaction = task_source->BeginTransaction();
  transaction.UpdatePriority(TaskPriority::USER_BLOCKING);
  thread_group_->UpdateSortKey(std::move(transaction));

  // Wait until all posted tasks start running. This should not block forever,
  // even in a thread group that enforces a maximum number of concurrent
  // BEST_EFFORT tasks lower than |kMaxTasks|.
  static_assert(kMaxBestEffortTasks < kMaxTasks, "");
  {
    CheckedAutoLock auto_lock(num_tasks_running_lock);
    while (num_tasks_running < kMaxTasks)
      num_tasks_running_cv->Wait();
  }

  // Flush the task tracker to be sure that no local variables are accessed by
  // tasks after the end of the scope.
  task_tracker_.FlushForTesting();
}

INSTANTIATE_TEST_SUITE_P(Generic,
                         ThreadGroupTest,
                         ::testing::Values(test::GroupType::GENERIC));
INSTANTIATE_TEST_SUITE_P(
    GenericParallel,
    ThreadGroupTestAllExecutionModes,
    ::testing::Combine(::testing::Values(test::GroupType::GENERIC),
                       ::testing::Values(TaskSourceExecutionMode::kParallel)));
INSTANTIATE_TEST_SUITE_P(
    GenericSequenced,
    ThreadGroupTestAllExecutionModes,
    ::testing::Combine(::testing::Values(test::GroupType::GENERIC),
                       ::testing::Values(TaskSourceExecutionMode::kSequenced)));
INSTANTIATE_TEST_SUITE_P(
    GenericJob,
    ThreadGroupTestAllExecutionModes,
    ::testing::Combine(::testing::Values(test::GroupType::GENERIC),
                       ::testing::Values(TaskSourceExecutionMode::kJob)));

#if HAS_NATIVE_THREAD_POOL()
INSTANTIATE_TEST_SUITE_P(Native,
                         ThreadGroupTest,
                         ::testing::Values(test::GroupType::NATIVE));
INSTANTIATE_TEST_SUITE_P(
    NativeParallel,
    ThreadGroupTestAllExecutionModes,
    ::testing::Combine(::testing::Values(test::GroupType::NATIVE),
                       ::testing::Values(TaskSourceExecutionMode::kParallel)));
INSTANTIATE_TEST_SUITE_P(
    NativeSequenced,
    ThreadGroupTestAllExecutionModes,
    ::testing::Combine(::testing::Values(test::GroupType::NATIVE),
                       ::testing::Values(TaskSourceExecutionMode::kSequenced)));
INSTANTIATE_TEST_SUITE_P(
    NativeJob,
    ThreadGroupTestAllExecutionModes,
    ::testing::Combine(::testing::Values(test::GroupType::NATIVE),
                       ::testing::Values(TaskSourceExecutionMode::kJob)));
#endif

}  // namespace internal
}  // namespace base
