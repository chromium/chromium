// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group_impl.h"

#include <stddef.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/atomicops.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/statistics_recorder.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/task_features.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool/delayed_task_manager.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/pooled_task_runner_delegate.h"
#include "base/task/thread_pool/sequence.h"
#include "base/task/thread_pool/task_source_sort_key.h"
#include "base/task/thread_pool/task_tracker.h"
#include "base/task/thread_pool/test_task_factory.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/task/thread_pool/worker_thread_observer.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker_impl.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace {

constexpr size_t kMaxTasks = 4;
constexpr size_t kNumThreadsPostingTasks = 4;
constexpr size_t kNumTasksPostedPerThread = 150;
// This can't be lower because Windows' TestWaitableEvent wakes up too early
// when a small timeout is used. This results in many spurious wake ups before a
// worker is allowed to cleanup.
constexpr TimeDelta kReclaimTimeForCleanupTests = Milliseconds(500);
constexpr size_t kLargeNumber = 512;

class ThreadGroupImplImplTestBase : public ThreadGroup::Delegate {
 public:
  ThreadGroupImplImplTestBase(const ThreadGroupImplImplTestBase&) = delete;
  ThreadGroupImplImplTestBase& operator=(const ThreadGroupImplImplTestBase&) =
      delete;

 protected:
  ThreadGroupImplImplTestBase()
      : service_thread_("ThreadPoolServiceThread"),
        tracked_ref_factory_(this) {}

  void CommonTearDown() {
    delayed_task_manager_.Shutdown();
    service_thread_.Stop();
    task_tracker_.FlushForTesting();
    if (thread_group_)
      thread_group_->JoinForTesting();
    mock_pooled_task_runner_delegate_.SetThreadGroup(nullptr);
    thread_group_.reset();
  }

  void CreateThreadGroup(ThreadType thread_type = ThreadType::kDefault) {
    ASSERT_FALSE(thread_group_);
    service_thread_.Start();
    delayed_task_manager_.Start(service_thread_.task_runner());
    thread_group_ = std::make_unique<ThreadGroupImpl>(
        "TestThreadGroup", "A", thread_type, task_tracker_.GetTrackedRef(),
        tracked_ref_factory_.GetTrackedRef());
    ASSERT_TRUE(thread_group_);

    mock_pooled_task_runner_delegate_.SetThreadGroup(thread_group_.get());
  }

  void StartThreadGroup(
      TimeDelta suggested_reclaim_time,
      size_t max_tasks,
      std::optional<int> max_best_effort_tasks = std::nullopt,
      WorkerThreadObserver* worker_observer = nullptr,
      std::optional<TimeDelta> may_block_threshold = std::nullopt) {
    ASSERT_TRUE(thread_group_);
    thread_group_->Start(
        max_tasks,
        max_best_effort_tasks ? max_best_effort_tasks.value() : max_tasks,
        suggested_reclaim_time, service_thread_.task_runner(), worker_observer,
        ThreadGroup::WorkerEnvironment::NONE,
        /* synchronous_thread_start_for_testing=*/false, may_block_threshold);
  }

  void CreateAndStartThreadGroup(
      TimeDelta suggested_reclaim_time = TimeDelta::Max(),
      size_t max_tasks = kMaxTasks,
      std::optional<int> max_best_effort_tasks = std::nullopt,
      WorkerThreadObserver* worker_observer = nullptr,
      std::optional<TimeDelta> may_block_threshold = std::nullopt) {
    CreateThreadGroup();
    StartThreadGroup(suggested_reclaim_time, max_tasks, max_best_effort_tasks,
                     worker_observer, may_block_threshold);
  }

  Thread service_thread_;
  TaskTracker task_tracker_;
  std::unique_ptr<ThreadGroupImpl> thread_group_;
  DelayedTaskManager delayed_task_manager_;
  TrackedRefFactory<ThreadGroup::Delegate> tracked_ref_factory_;
  test::MockPooledTaskRunnerDelegate mock_pooled_task_runner_delegate_ = {
      task_tracker_.GetTrackedRef(), &delayed_task_manager_};

 private:
  // ThreadGroup::Delegate:
  ThreadGroup* GetThreadGroupForTraits(const TaskTraits& traits) override {
    return thread_group_.get();
  }
};

class ThreadGroupImplImplTest : public ThreadGroupImplImplTestBase,
                                public testing::Test {
 public:
  ThreadGroupImplImplTest(const ThreadGroupImplImplTest&) = delete;
  ThreadGroupImplImplTest& operator=(const ThreadGroupImplImplTest&) = delete;

 protected:
  ThreadGroupImplImplTest() = default;

  void SetUp() override { CreateAndStartThreadGroup(); }

  void TearDown() override { ThreadGroupImplImplTestBase::CommonTearDown(); }
};

class ThreadGroupImplImplTestParam
    : public ThreadGroupImplImplTestBase,
      public testing::TestWithParam<TaskSourceExecutionMode> {
 public:
  ThreadGroupImplImplTestParam(const ThreadGroupImplImplTestParam&) = delete;
  ThreadGroupImplImplTestParam& operator=(const ThreadGroupImplImplTestParam&) =
      delete;

 protected:
  ThreadGroupImplImplTestParam() = default;

  void SetUp() override { CreateAndStartThreadGroup(); }

  void TearDown() override { ThreadGroupImplImplTestBase::CommonTearDown(); }
};

using PostNestedTask = test::TestTaskFactory::PostNestedTask;

class ThreadPostingTasksWaitIdle : public SimpleThread {
 public:
  // Constructs a thread that posts tasks to |thread_group| through an
  // |execution_mode| task runner. The thread waits until all workers in
  // |thread_group| are idle before posting a new task.
  ThreadPostingTasksWaitIdle(
      ThreadGroupImpl* thread_group,
      test::MockPooledTaskRunnerDelegate* mock_pooled_task_runner_delegate_,
      TaskSourceExecutionMode execution_mode)
      : SimpleThread("ThreadPostingTasksWaitIdle"),
        thread_group_(thread_group),
        factory_(CreatePooledTaskRunnerWithExecutionMode(
                     execution_mode,
                     mock_pooled_task_runner_delegate_),
                 execution_mode) {
    DCHECK(thread_group_);
  }
  ThreadPostingTasksWaitIdle(const ThreadPostingTasksWaitIdle&) = delete;
  ThreadPostingTasksWaitIdle& operator=(const ThreadPostingTasksWaitIdle&) =
      delete;

  const test::TestTaskFactory* factory() const { return &factory_; }

 private:
  void Run() override {
    for (size_t i = 0; i < kNumTasksPostedPerThread; ++i) {
      thread_group_->WaitForAllWorkersIdleForTesting();
      EXPECT_TRUE(factory_.PostTask(PostNestedTask::NO, OnceClosure()));
    }
  }

  const raw_ptr<ThreadGroupImpl> thread_group_;
  const scoped_refptr<TaskRunner> task_runner_;
  test::TestTaskFactory factory_;
};

}  // namespace

TEST_P(ThreadGroupImplImplTestParam, PostTasksWaitAllWorkersIdle) {
  // Create threads to post tasks. To verify that workers can sleep and be woken
  // up when new tasks are posted, wait for all workers to become idle before
  // posting a new task.
  std::vector<std::unique_ptr<ThreadPostingTasksWaitIdle>>
      threads_posting_tasks;
  for (size_t i = 0; i < kNumThreadsPostingTasks; ++i) {
    threads_posting_tasks.push_back(
        std::make_unique<ThreadPostingTasksWaitIdle>(
            thread_group_.get(), &mock_pooled_task_runner_delegate_,
            GetParam()));
    threads_posting_tasks.back()->Start();
  }

  // Wait for all tasks to run.
  for (const auto& thread_posting_tasks : threads_posting_tasks) {
    thread_posting_tasks->Join();
    thread_posting_tasks->factory()->WaitForAllTasksToRun();
  }

  // Wait until all workers are idle to be sure that no task accesses its
  // TestTaskFactory after |thread_posting_tasks| is destroyed.
  thread_group_->WaitForAllWorkersIdleForTesting();
}

TEST_P(ThreadGroupImplImplTestParam, PostTasksWithOneAvailableWorker) {
  // Post blocking tasks to keep all workers busy except one until |event| is
  // signaled. Use different factories so that tasks are added to different
  // sequences and can run simultaneously when the execution mode is SEQUENCED.
  TestWaitableEvent event;
  std::vector<std::unique_ptr<test::TestTaskFactory>> blocked_task_factories;
  for (size_t i = 0; i < (kMaxTasks - 1); ++i) {
    blocked_task_factories.push_back(std::make_unique<test::TestTaskFactory>(
        CreatePooledTaskRunnerWithExecutionMode(
            GetParam(), &mock_pooled_task_runner_delegate_),
        GetParam()));
    EXPECT_TRUE(blocked_task_factories.back()->PostTask(
        PostNestedTask::NO,
        BindOnce(&TestWaitableEvent::Wait, Unretained(&event))));
    blocked_task_factories.back()->WaitForAllTasksToRun();
  }

  // Post |kNumTasksPostedPerThread| tasks that should all run despite the fact
  // that only one worker in |thread_group_| isn't busy.
  test::TestTaskFactory short_task_factory(
      CreatePooledTaskRunnerWithExecutionMode(
          GetParam(), &mock_pooled_task_runner_delegate_),
      GetParam());
  for (size_t i = 0; i < kNumTasksPostedPerThread; ++i)
    EXPECT_TRUE(short_task_factory.PostTask(PostNestedTask::NO, OnceClosure()));
  short_task_factory.WaitForAllTasksToRun();

  // Release tasks waiting on |event|.
  event.Signal();

  // Wait until all workers are idle to be sure that no task accesses
  // its TestTaskFactory after it is destroyed.
  thread_group_->WaitForAllWorkersIdleForTesting();
}

TEST_P(ThreadGroupImplImplTestParam, Saturate) {
  // Verify that it is possible to have |kMaxTasks| tasks/sequences running
  // simultaneously. Use different factories so that the blocking tasks are
  // added to different sequences and can run simultaneously when the execution
  // mode is SEQUENCED.
  TestWaitableEvent event;
  std::vector<std::unique_ptr<test::TestTaskFactory>> factories;
  for (size_t i = 0; i < kMaxTasks; ++i) {
    factories.push_back(std::make_unique<test::TestTaskFactory>(
        CreatePooledTaskRunnerWithExecutionMode(
            GetParam(), &mock_pooled_task_runner_delegate_),
        GetParam()));
    EXPECT_TRUE(factories.back()->PostTask(
        PostNestedTask::NO,
        BindOnce(&TestWaitableEvent::Wait, Unretained(&event))));
    factories.back()->WaitForAllTasksToRun();
  }

  // Release tasks waiting on |event|.
  event.Signal();

  // Wait until all workers are idle to be sure that no task accesses
  // its TestTaskFactory after it is destroyed.
  thread_group_->WaitForAllWorkersIdleForTesting();
}

// Verifies that ShouldYield() returns true for priorities lower than the
// highest priority pending while the thread group is flooded with USER_VISIBLE
// tasks.
TEST_F(ThreadGroupImplImplTest, ShouldYieldFloodedUserVisible) {
  TestWaitableEvent threads_running;
  TestWaitableEvent threads_continue;

  // Saturate workers with USER_VISIBLE tasks to ensure ShouldYield() returns
  // true when a tasks of higher priority is posted.
  RepeatingClosure threads_running_barrier = BarrierClosure(
      kMaxTasks,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running)));

  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      BindLambdaForTesting(
          [&threads_running_barrier, &threads_continue](JobDelegate* delegate) {
            threads_running_barrier.Run();
            threads_continue.Wait();
          }),
      /* num_tasks_to_run */ kMaxTasks);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {TaskPriority::USER_VISIBLE},
                                 &mock_pooled_task_runner_delegate_);

  auto registered_task_source = task_tracker_.RegisterTaskSource(task_source);
  ASSERT_TRUE(registered_task_source);
  static_cast<ThreadGroup*>(thread_group_.get())
      ->PushTaskSourceAndWakeUpWorkers(
          RegisteredTaskSourceAndTransaction::FromTaskSource(
              std::move(registered_task_source)));

  threads_running.Wait();

  // Posting a BEST_EFFORT task should not cause any other tasks to yield.
  // Once this task gets to run, no other task needs to yield.
  // Note: This is only true because this test is using a single ThreadGroup.
  //       Under the ThreadPool this wouldn't be racy because BEST_EFFORT tasks
  //       run in an independent ThreadGroup.
  test::CreatePooledTaskRunner({TaskPriority::BEST_EFFORT},
                               &mock_pooled_task_runner_delegate_)
      ->PostTask(
          FROM_HERE, BindLambdaForTesting([&] {
            EXPECT_FALSE(thread_group_->ShouldYield(
                {TaskPriority::BEST_EFFORT, TimeTicks(), /* worker_count=*/1}));
          }));
  // A BEST_EFFORT task with more workers shouldn't have to yield.
  EXPECT_FALSE(thread_group_->ShouldYield(
      {TaskPriority::BEST_EFFORT, TimeTicks(), /* worker_count=*/2}));
  EXPECT_FALSE(thread_group_->ShouldYield(
      {TaskPriority::BEST_EFFORT, TimeTicks(), /* worker_count=*/0}));
  EXPECT_FALSE(thread_group_->ShouldYield(
      {TaskPriority::USER_VISIBLE, TimeTicks(), /* worker_count=*/0}));
  EXPECT_FALSE(thread_group_->ShouldYield(
      {TaskPriority::USER_BLOCKING, TimeTicks(), /* worker_count=*/0}));

  // Posting a USER_VISIBLE task should cause BEST_EFFORT and USER_VISIBLE with
  // higher worker_count tasks to yield.
  auto post_user_visible = [&] {
    test::CreatePooledTaskRunner({TaskPriority::USER_VISIBLE},
                                 &mock_pooled_task_runner_delegate_)
        ->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                     EXPECT_FALSE(thread_group_->ShouldYield(
                         {TaskPriority::USER_VISIBLE, TimeTicks(),
                          /* worker_count=*/1}));
                   }));
  };
  // A USER_VISIBLE task with too many workers should yield.
  post_user_visible();
  EXPECT_TRUE(thread_group_->ShouldYield(
      {TaskPriority::USER_VISIBLE, TimeTicks(), /* worker_count=*/2}));
  post_user_visible();
  EXPECT_TRUE(thread_group_->ShouldYield(
      {TaskPriority::BEST_EFFORT, TimeTicks(), /* worker_count=*/0}));
  post_user_visible();
  EXPECT_FALSE(thread_group_->ShouldYield(
      {TaskPriority::USER_VISIBLE, TimeTicks(), /* worker_count=*/1}));
  EXPECT_FALSE(thread_group_->ShouldYield(
      {TaskPriority::USER_BLOCKING, TimeTicks(), /* worker_count=*/0}));

  // Posting a USER_BLOCKING task should cause BEST_EFFORT, USER_VISIBLE and
  // USER_BLOCKING with higher worker_count tasks to yield.
  auto post_user_blocking = [&] {
    test::CreatePooledTaskRunner({TaskPriority::USER_BLOCKING},
                                 &mock_pooled_task_runner_delegate_)
        ->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                     // Once this task got to start, no other task needs to
                     // yield.
                     EXPECT_FALSE(thread_group_->ShouldYield(
                         {TaskPriority::USER_BLOCKING, TimeTicks(),
                          /* worker_count=*/1}));
                   }));
  };
  // A USER_BLOCKING task with too many workers should have to yield.
  post_user_blocking();
  EXPECT_TRUE(thread_group_->ShouldYield(
      {TaskPriority::USER_BLOCKING, TimeTicks(), /* worker_count=*/2}));
  post_user_blocking();
  EXPECT_TRUE(thread_group_->ShouldYield(
      {TaskPriority::BEST_EFFORT, TimeTicks(), /* worker_count=*/0}));
  post_user_blocking();
  EXPECT_TRUE(thread_group_->ShouldYield(
      {TaskPriority::USER_VISIBLE, TimeTicks(), /* worker_count=*/0}));
  post_user_blocking();
  EXPECT_FALSE(thread_group_->ShouldYield(
      {TaskPriority::USER_BLOCKING, TimeTicks(), /* worker_count=*/1}));

  threads_continue.Signal();
  task_tracker_.FlushForTesting();
}

INSTANTIATE_TEST_SUITE_P(Parallel,
                         ThreadGroupImplImplTestParam,
                         ::testing::Values(TaskSourceExecutionMode::kParallel));
INSTANTIATE_TEST_SUITE_P(
    Sequenced,
    ThreadGroupImplImplTestParam,
    ::testing::Values(TaskSourceExecutionMode::kSequenced));

INSTANTIATE_TEST_SUITE_P(Job,
                         ThreadGroupImplImplTestParam,
                         ::testing::Values(TaskSourceExecutionMode::kJob));

namespace {

class ThreadGroupImplImplStartInBodyTest : public ThreadGroupImplImplTest {
 public:
  void SetUp() override {
    CreateThreadGroup();
    // Let the test start the thread group.
  }
};

void TaskPostedBeforeStart(PlatformThreadRef* platform_thread_ref,
                           TestWaitableEvent* task_running,
                           TestWaitableEvent* barrier) {
  *platform_thread_ref = PlatformThread::CurrentRef();
  task_running->Signal();
  barrier->Wait();
}

}  // namespace

// Verify that 2 tasks posted before Start() to a ThreadGroupImpl with
// more than 2 workers run on different workers when Start() is called.
TEST_F(ThreadGroupImplImplStartInBodyTest, PostTasksBeforeStart) {
  PlatformThreadRef task_1_thread_ref;
  PlatformThreadRef task_2_thread_ref;
  TestWaitableEvent task_1_running;
  TestWaitableEvent task_2_running;

  // This event is used to prevent a task from completing before the other task
  // starts running. If that happened, both tasks could run on the same worker
  // and this test couldn't verify that the correct number of workers were woken
  // up.
  TestWaitableEvent barrier;

  test::CreatePooledTaskRunner({WithBaseSyncPrimitives()},
                               &mock_pooled_task_runner_delegate_)
      ->PostTask(
          FROM_HERE,
          BindOnce(&TaskPostedBeforeStart, Unretained(&task_1_thread_ref),
                   Unretained(&task_1_running), Unretained(&barrier)));
  test::CreatePooledTaskRunner({WithBaseSyncPrimitives()},
                               &mock_pooled_task_runner_delegate_)
      ->PostTask(
          FROM_HERE,
          BindOnce(&TaskPostedBeforeStart, Unretained(&task_2_thread_ref),
                   Unretained(&task_2_running), Unretained(&barrier)));

  // Workers should not be created and tasks should not run before the thread
  // group is started.
  EXPECT_EQ(0U, thread_group_->NumberOfWorkersForTesting());
  EXPECT_FALSE(task_1_running.IsSignaled());
  EXPECT_FALSE(task_2_running.IsSignaled());

  StartThreadGroup(TimeDelta::Max(), kMaxTasks);

  // Tasks should run shortly after the thread group is started.
  task_1_running.Wait();
  task_2_running.Wait();

  // Tasks should run on different threads.
  EXPECT_NE(task_1_thread_ref, task_2_thread_ref);

  barrier.Signal();
  task_tracker_.FlushForTesting();
}

// Verify that posting many tasks before Start will cause the number of workers
// to grow to |max_tasks_| after Start.
TEST_F(ThreadGroupImplImplStartInBodyTest, PostManyTasks) {
  scoped_refptr<TaskRunner> task_runner = test::CreatePooledTaskRunner(
      {WithBaseSyncPrimitives()}, &mock_pooled_task_runner_delegate_);
  constexpr size_t kNumTasksPosted = 2 * kMaxTasks;

  TestWaitableEvent threads_running;
  TestWaitableEvent threads_continue;

  RepeatingClosure threads_running_barrier = BarrierClosure(
      kMaxTasks,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running)));
  // Posting these tasks should cause new workers to be created.
  for (size_t i = 0; i < kMaxTasks; ++i) {
    task_runner->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                            threads_running_barrier.Run();
                            threads_continue.Wait();
                          }));
  }
  // Post the remaining |kNumTasksPosted - kMaxTasks| tasks, don't wait for them
  // as they'll be blocked behind the above kMaxtasks.
  for (size_t i = kMaxTasks; i < kNumTasksPosted; ++i)
    task_runner->PostTask(FROM_HERE, DoNothing());

  EXPECT_EQ(0U, thread_group_->NumberOfWorkersForTesting());

  StartThreadGroup(TimeDelta::Max(), kMaxTasks);
  EXPECT_GT(thread_group_->NumberOfWorkersForTesting(), 0U);
  EXPECT_EQ(kMaxTasks, thread_group_->GetMaxTasksForTesting());

  threads_running.Wait();
  EXPECT_EQ(thread_group_->NumberOfWorkersForTesting(),
            thread_group_->GetMaxTasksForTesting());
  threads_continue.Signal();
  task_tracker_.FlushForTesting();
}

namespace {

class BackgroundThreadGroupImplTest : public ThreadGroupImplImplTest {
 public:
  void CreateAndStartThreadGroup(
      TimeDelta suggested_reclaim_time = TimeDelta::Max(),
      size_t max_tasks = kMaxTasks,
      std::optional<int> max_best_effort_tasks = std::nullopt,
      WorkerThreadObserver* worker_observer = nullptr,
      std::optional<TimeDelta> may_block_threshold = std::nullopt) {
    if (!CanUseBackgroundThreadTypeForWorkerThread())
      return;
    CreateThreadGroup(ThreadType::kBackground);
    StartThreadGroup(suggested_reclaim_time, max_tasks, max_best_effort_tasks,
                     worker_observer, may_block_threshold);
  }

  void SetUp() override { CreateAndStartThreadGroup(); }
};

}  // namespace

// Verify that ScopedBlockingCall updates thread type when necessary per
// shutdown state.
TEST_F(BackgroundThreadGroupImplTest, UpdatePriorityBlockingStarted) {
  if (!CanUseBackgroundThreadTypeForWorkerThread())
    return;

  const scoped_refptr<TaskRunner> task_runner = test::CreatePooledTaskRunner(
      {MayBlock(), WithBaseSyncPrimitives(), TaskPriority::BEST_EFFORT},
      &mock_pooled_task_runner_delegate_);

  TestWaitableEvent threads_running;
  RepeatingClosure threads_running_barrier = BarrierClosure(
      kMaxTasks,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running)));

  TestWaitableEvent blocking_threads_continue;

  for (size_t i = 0; i < kMaxTasks; ++i) {
    task_runner->PostTask(
        FROM_HERE, BindLambdaForTesting([&] {
          EXPECT_EQ(ThreadType::kBackground,
                    PlatformThread::GetCurrentThreadType());
          {
            // ScopedBlockingCall before shutdown doesn't affect priority.
            ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                    BlockingType::MAY_BLOCK);
            EXPECT_EQ(ThreadType::kBackground,
                      PlatformThread::GetCurrentThreadType());
          }
          threads_running_barrier.Run();
          blocking_threads_continue.Wait();
          // This is reached after StartShutdown(), at which point we expect
          // ScopedBlockingCall to update thread priority.
          ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                  BlockingType::MAY_BLOCK);
          EXPECT_EQ(ThreadType::kDefault,
                    PlatformThread::GetCurrentThreadType());
        }));
  }
  threads_running.Wait();

  task_tracker_.StartShutdown();
  blocking_threads_continue.Signal();
  task_tracker_.FlushForTesting();
}

namespace {

class ThreadGroupImplStandbyPolicyTest : public ThreadGroupImplImplTestBase,
                                         public testing::Test {
 public:
  ThreadGroupImplStandbyPolicyTest() = default;
  ThreadGroupImplStandbyPolicyTest(const ThreadGroupImplStandbyPolicyTest&) =
      delete;
  ThreadGroupImplStandbyPolicyTest& operator=(
      const ThreadGroupImplStandbyPolicyTest&) = delete;

  void SetUp() override {
    CreateAndStartThreadGroup(kReclaimTimeForCleanupTests);
  }

  void TearDown() override { ThreadGroupImplImplTestBase::CommonTearDown(); }
};

}  // namespace

TEST_F(ThreadGroupImplStandbyPolicyTest, InitOne) {
  EXPECT_EQ(1U, thread_group_->NumberOfWorkersForTesting());
}

namespace {

enum class OptionalBlockingType {
  NO_BLOCK,
  MAY_BLOCK,
  WILL_BLOCK,
};

struct NestedBlockingType {
  NestedBlockingType(BlockingType first_in,
                     OptionalBlockingType second_in,
                     BlockingType behaves_as_in)
      : first(first_in), second(second_in), behaves_as(behaves_as_in) {}

  BlockingType first;
  OptionalBlockingType second;
  BlockingType behaves_as;
};

class NestedScopedBlockingCall {
 public:
  explicit NestedScopedBlockingCall(
      const NestedBlockingType& nested_blocking_type)
      : first_scoped_blocking_call_(FROM_HERE, nested_blocking_type.first),
        second_scoped_blocking_call_(
            nested_blocking_type.second == OptionalBlockingType::WILL_BLOCK
                ? std::make_unique<ScopedBlockingCall>(FROM_HERE,
                                                       BlockingType::WILL_BLOCK)
                : (nested_blocking_type.second ==
                           OptionalBlockingType::MAY_BLOCK
                       ? std::make_unique<ScopedBlockingCall>(
                             FROM_HERE,
                             BlockingType::MAY_BLOCK)
                       : nullptr)) {}
  NestedScopedBlockingCall(const NestedScopedBlockingCall&) = delete;
  NestedScopedBlockingCall& operator=(const NestedScopedBlockingCall&) = delete;

 private:
  ScopedBlockingCall first_scoped_blocking_call_;
  std::unique_ptr<ScopedBlockingCall> second_scoped_blocking_call_;
};

}  // namespace

class ThreadGroupImplBlockingTest
    : public ThreadGroupImplImplTestBase,
      public testing::TestWithParam<NestedBlockingType> {
 public:
  ThreadGroupImplBlockingTest() = default;
  ThreadGroupImplBlockingTest(const ThreadGroupImplBlockingTest&) = delete;
  ThreadGroupImplBlockingTest& operator=(const ThreadGroupImplBlockingTest&) =
      delete;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<NestedBlockingType> param_info) {
    std::string str = param_info.param.first == BlockingType::MAY_BLOCK
                          ? "MAY_BLOCK"
                          : "WILL_BLOCK";
    if (param_info.param.second == OptionalBlockingType::MAY_BLOCK)
      str += "_MAY_BLOCK";
    else if (param_info.param.second == OptionalBlockingType::WILL_BLOCK)
      str += "_WILL_BLOCK";
    return str;
  }

  void TearDown() override { ThreadGroupImplImplTestBase::CommonTearDown(); }

 protected:
  // Saturates the thread group with a task that first blocks, waits to be
  // unblocked, then exits.
  void SaturateWithBlockingTasks(
      const NestedBlockingType& nested_blocking_type,
      TaskPriority priority = TaskPriority::USER_BLOCKING) {
    TestWaitableEvent threads_running;

    const scoped_refptr<TaskRunner> task_runner = test::CreatePooledTaskRunner(
        {MayBlock(), WithBaseSyncPrimitives(), priority},
        &mock_pooled_task_runner_delegate_);

    RepeatingClosure threads_running_barrier = BarrierClosure(
        kMaxTasks,
        BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running)));

    for (size_t i = 0; i < kMaxTasks; ++i) {
      task_runner->PostTask(
          FROM_HERE, BindLambdaForTesting([this, &threads_running_barrier,
                                           nested_blocking_type]() {
            NestedScopedBlockingCall nested_scoped_blocking_call(
                nested_blocking_type);
            threads_running_barrier.Run();
            blocking_threads_continue_.Wait();
          }));
    }
    threads_running.Wait();
  }

  // Saturates the thread group with a task that waits for other tasks without
  // entering a ScopedBlockingCall, then exits.
  void SaturateWithBusyTasks(
      TaskPriority priority = TaskPriority::USER_BLOCKING,
      TaskShutdownBehavior shutdown_behavior =
          TaskShutdownBehavior::SKIP_ON_SHUTDOWN) {
    TestWaitableEvent threads_running;

    const scoped_refptr<TaskRunner> task_runner = test::CreatePooledTaskRunner(
        {MayBlock(), WithBaseSyncPrimitives(), priority, shutdown_behavior},
        &mock_pooled_task_runner_delegate_);

    RepeatingClosure threads_running_barrier = BarrierClosure(
        kMaxTasks,
        BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running)));
    // Posting these tasks should cause new workers to be created.
    for (size_t i = 0; i < kMaxTasks; ++i) {
      task_runner->PostTask(
          FROM_HERE, BindLambdaForTesting([this, &threads_running_barrier]() {
            threads_running_barrier.Run();
            busy_threads_continue_.Wait();
          }));
    }
    threads_running.Wait();
  }

  // Returns how long we can expect a change to |max_tasks_| to occur
  // after a task has become blocked.
  TimeDelta GetMaxTasksChangeSleepTime() {
    return std::max(thread_group_->blocked_workers_poll_period_for_testing(),
                    thread_group_->may_block_threshold_for_testing()) +
           TestTimeouts::tiny_timeout();
  }

  // Waits indefinitely, until |thread_group_|'s max tasks increases to
  // |expected_max_tasks|.
  void ExpectMaxTasksIncreasesTo(size_t expected_max_tasks) {
    size_t max_tasks = thread_group_->GetMaxTasksForTesting();
    while (max_tasks != expected_max_tasks) {
      PlatformThread::Sleep(GetMaxTasksChangeSleepTime());
      size_t new_max_tasks = thread_group_->GetMaxTasksForTesting();
      ASSERT_GE(new_max_tasks, max_tasks);
      max_tasks = new_max_tasks;
    }
  }

  // Unblocks tasks posted by SaturateWithBlockingTasks().
  void UnblockBlockingTasks() { blocking_threads_continue_.Signal(); }

  // Unblocks tasks posted by SaturateWithBusyTasks().
  void UnblockBusyTasks() { busy_threads_continue_.Signal(); }

  const scoped_refptr<TaskRunner> task_runner_ =
      test::CreatePooledTaskRunner({MayBlock(), WithBaseSyncPrimitives()},
                                   &mock_pooled_task_runner_delegate_);

 private:
  TestWaitableEvent blocking_threads_continue_;
  TestWaitableEvent busy_threads_continue_;
};

// Verify that SaturateWithBlockingTasks() causes max tasks to increase and
// creates a worker if needed. Also verify that UnblockBlockingTasks() decreases
// max tasks after an increase.
TEST_P(ThreadGroupImplBlockingTest, ThreadBlockedUnblocked) {
  CreateAndStartThreadGroup();

  ASSERT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);

  SaturateWithBlockingTasks(GetParam());

  // Forces |kMaxTasks| extra workers to be instantiated by posting tasks. This
  // should not block forever.
  SaturateWithBusyTasks();

  EXPECT_EQ(thread_group_->NumberOfWorkersForTesting(), 2 * kMaxTasks);

  UnblockBusyTasks();
  UnblockBlockingTasks();
  task_tracker_.FlushForTesting();
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);
}

// Verify that SaturateWithBlockingTasks() of BEST_EFFORT tasks causes max best
// effort tasks to increase and creates a worker if needed. Also verify that
// UnblockBlockingTasks() decreases max best effort tasks after an increase.
TEST_P(ThreadGroupImplBlockingTest, ThreadBlockedUnblockedBestEffort) {
  CreateAndStartThreadGroup();

  ASSERT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);
  ASSERT_EQ(thread_group_->GetMaxBestEffortTasksForTesting(), kMaxTasks);

  SaturateWithBlockingTasks(GetParam(), TaskPriority::BEST_EFFORT);

  // Forces |kMaxTasks| extra workers to be instantiated by posting tasks. This
  // should not block forever.
  SaturateWithBusyTasks(TaskPriority::BEST_EFFORT);

  EXPECT_EQ(thread_group_->NumberOfWorkersForTesting(), 2 * kMaxTasks);

  UnblockBusyTasks();
  UnblockBlockingTasks();
  task_tracker_.FlushForTesting();
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);
  EXPECT_EQ(thread_group_->GetMaxBestEffortTasksForTesting(), kMaxTasks);
}

// Verify that flooding the thread group with more BEST_EFFORT tasks than
// kMaxBestEffortTasks doesn't prevent USER_VISIBLE tasks from running.
TEST_P(ThreadGroupImplBlockingTest, TooManyBestEffortTasks) {
  constexpr size_t kMaxBestEffortTasks = kMaxTasks / 2;

  CreateAndStartThreadGroup(TimeDelta::Max(), kMaxTasks, kMaxBestEffortTasks);

  TestWaitableEvent threads_continue;
  {
    TestWaitableEvent entered_blocking_scope;
    RepeatingClosure entered_blocking_scope_barrier = BarrierClosure(
        kMaxBestEffortTasks + 1, BindOnce(&TestWaitableEvent::Signal,
                                          Unretained(&entered_blocking_scope)));
    TestWaitableEvent exit_blocking_scope;

    TestWaitableEvent threads_running;
    RepeatingClosure threads_running_barrier = BarrierClosure(
        kMaxBestEffortTasks + 1,
        BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running)));

    const auto best_effort_task_runner =
        test::CreatePooledTaskRunner({TaskPriority::BEST_EFFORT, MayBlock()},
                                     &mock_pooled_task_runner_delegate_);
    for (size_t i = 0; i < kMaxBestEffortTasks + 1; ++i) {
      best_effort_task_runner->PostTask(
          FROM_HERE, BindLambdaForTesting([&] {
            {
              NestedScopedBlockingCall scoped_blocking_call(GetParam());
              entered_blocking_scope_barrier.Run();
              exit_blocking_scope.Wait();
            }
            threads_running_barrier.Run();
            threads_continue.Wait();
          }));
    }
    entered_blocking_scope.Wait();
    exit_blocking_scope.Signal();
    threads_running.Wait();
  }

  // At this point, kMaxBestEffortTasks + 1 threads are running (plus
  // potentially the idle thread), but max_task and max_best_effort_task are
  // back to normal.
  EXPECT_GE(thread_group_->NumberOfWorkersForTesting(),
            kMaxBestEffortTasks + 1);
  EXPECT_LE(thread_group_->NumberOfWorkersForTesting(),
            kMaxBestEffortTasks + 2);
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);

  TestWaitableEvent threads_running;
  task_runner_->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                           threads_running.Signal();
                           threads_continue.Wait();
                         }));

  // This should not block forever.
  threads_running.Wait();

  EXPECT_GE(thread_group_->NumberOfWorkersForTesting(),
            kMaxBestEffortTasks + 2);
  EXPECT_LE(thread_group_->NumberOfWorkersForTesting(),
            kMaxBestEffortTasks + 3);
  threads_continue.Signal();

  task_tracker_.FlushForTesting();
}

// Verify that tasks posted in a saturated thread group before a
// ScopedBlockingCall will execute after ScopedBlockingCall is instantiated.
TEST_P(ThreadGroupImplBlockingTest, PostBeforeBlocking) {
  CreateAndStartThreadGroup();

  TestWaitableEvent thread_running(WaitableEvent::ResetPolicy::AUTOMATIC);
  TestWaitableEvent thread_can_block;
  TestWaitableEvent threads_continue;

  for (size_t i = 0; i < kMaxTasks; ++i) {
    task_runner_->PostTask(
        FROM_HERE,
        BindOnce(
            [](const NestedBlockingType& nested_blocking_type,
               TestWaitableEvent* thread_running,
               TestWaitableEvent* thread_can_block,
               TestWaitableEvent* threads_continue) {
              thread_running->Signal();
              thread_can_block->Wait();

              NestedScopedBlockingCall nested_scoped_blocking_call(
                  nested_blocking_type);
              threads_continue->Wait();
            },
            GetParam(), Unretained(&thread_running),
            Unretained(&thread_can_block), Unretained(&threads_continue)));
    thread_running.Wait();
  }

  // All workers should be occupied and the thread group should be saturated.
  // Workers have not entered ScopedBlockingCall yet.
  EXPECT_EQ(thread_group_->NumberOfWorkersForTesting(), kMaxTasks);
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);

  TestWaitableEvent extra_threads_running;
  TestWaitableEvent extra_threads_continue;
  RepeatingClosure extra_threads_running_barrier = BarrierClosure(
      kMaxTasks,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&extra_threads_running)));
  for (size_t i = 0; i < kMaxTasks; ++i) {
    task_runner_->PostTask(
        FROM_HERE, BindOnce(
                       [](RepeatingClosure* extra_threads_running_barrier,
                          TestWaitableEvent* extra_threads_continue) {
                         extra_threads_running_barrier->Run();
                         extra_threads_continue->Wait();
                       },
                       Unretained(&extra_threads_running_barrier),
                       Unretained(&extra_threads_continue)));
  }

  // Allow tasks to enter ScopedBlockingCall. Workers should be created for the
  // tasks we just posted.
  thread_can_block.Signal();

  // Should not block forever.
  extra_threads_running.Wait();
  EXPECT_EQ(thread_group_->NumberOfWorkersForTesting(), 2 * kMaxTasks);
  extra_threads_continue.Signal();

  threads_continue.Signal();
  task_tracker_.FlushForTesting();
}

// Verify that workers become idle when the thread group is over-capacity and
// that those workers do no work.
TEST_P(ThreadGroupImplBlockingTest, WorkersIdleWhenOverCapacity) {
  CreateAndStartThreadGroup();

  ASSERT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);

  SaturateWithBlockingTasks(GetParam());

  // Forces |kMaxTasks| extra workers to be instantiated by posting tasks.
  SaturateWithBusyTasks();

  ASSERT_EQ(thread_group_->NumberOfIdleWorkersForTesting(), 0U);
  EXPECT_EQ(thread_group_->NumberOfWorkersForTesting(), 2 * kMaxTasks);

  AtomicFlag is_exiting;
  // These tasks should not get executed until after other tasks become
  // unblocked.
  for (size_t i = 0; i < kMaxTasks; ++i) {
    task_runner_->PostTask(FROM_HERE, BindOnce(
                                          [](AtomicFlag* is_exiting) {
                                            EXPECT_TRUE(is_exiting->IsSet());
                                          },
                                          Unretained(&is_exiting)));
  }

  // The original |kMaxTasks| will finish their tasks after being unblocked.
  // There will be work in the work queue, but the thread group should now be
  // over-capacity and workers will become idle.
  UnblockBlockingTasks();
  thread_group_->WaitForWorkersIdleForTesting(kMaxTasks);
  EXPECT_EQ(thread_group_->NumberOfIdleWorkersForTesting(), kMaxTasks);

  // Posting more tasks should not cause workers idle from the thread group
  // being over capacity to begin doing work.
  for (size_t i = 0; i < kMaxTasks; ++i) {
    task_runner_->PostTask(FROM_HERE, BindOnce(
                                          [](AtomicFlag* is_exiting) {
                                            EXPECT_TRUE(is_exiting->IsSet());
                                          },
                                          Unretained(&is_exiting)));
  }

  // Give time for those idle workers to possibly do work (which should not
  // happen).
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  is_exiting.Set();
  // Unblocks the new workers.
  UnblockBusyTasks();
  task_tracker_.FlushForTesting();
}

// Verify that an increase of max tasks with SaturateWithBlockingTasks()
// increases the number of tasks that can run before ShouldYield returns true.
TEST_P(ThreadGroupImplBlockingTest, ThreadBlockedUnblockedShouldYield) {
  CreateAndStartThreadGroup();

  ASSERT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);

  EXPECT_FALSE(
      thread_group_->ShouldYield({TaskPriority::BEST_EFFORT, TimeTicks()}));
  SaturateWithBlockingTasks(GetParam());
  EXPECT_FALSE(
      thread_group_->ShouldYield({TaskPriority::BEST_EFFORT, TimeTicks()}));

  // Forces |kMaxTasks| extra workers to be instantiated by posting tasks. This
  // should not block forever.
  SaturateWithBusyTasks();

  // All tasks can run, hence ShouldYield returns false.
  EXPECT_FALSE(
      thread_group_->ShouldYield({TaskPriority::BEST_EFFORT, TimeTicks()}));

  // Post a USER_VISIBLE task that can't run since workers are saturated. This
  // should cause BEST_EFFORT tasks to yield.
  test::CreatePooledTaskRunner({TaskPriority::USER_VISIBLE},
                               &mock_pooled_task_runner_delegate_)
      ->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                   EXPECT_FALSE(thread_group_->ShouldYield(
                       {TaskPriority::BEST_EFFORT, TimeTicks()}));
                 }));
  EXPECT_TRUE(
      thread_group_->ShouldYield({TaskPriority::BEST_EFFORT, TimeTicks()}));

  // Post a USER_BLOCKING task that can't run since workers are saturated. This
  // should cause USER_VISIBLE tasks to yield.
  test::CreatePooledTaskRunner({TaskPriority::USER_BLOCKING},
                               &mock_pooled_task_runner_delegate_)
      ->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                   EXPECT_FALSE(thread_group_->ShouldYield(
                       {TaskPriority::USER_VISIBLE, TimeTicks()}));
                 }));
  EXPECT_TRUE(
      thread_group_->ShouldYield({TaskPriority::USER_VISIBLE, TimeTicks()}));

  UnblockBusyTasks();
  UnblockBlockingTasks();
  task_tracker_.FlushForTesting();
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ThreadGroupImplBlockingTest,
    ::testing::Values(NestedBlockingType(BlockingType::MAY_BLOCK,
                                         OptionalBlockingType::NO_BLOCK,
                                         BlockingType::MAY_BLOCK),
                      NestedBlockingType(BlockingType::WILL_BLOCK,
                                         OptionalBlockingType::NO_BLOCK,
                                         BlockingType::WILL_BLOCK),
                      NestedBlockingType(BlockingType::MAY_BLOCK,
                                         OptionalBlockingType::WILL_BLOCK,
                                         BlockingType::WILL_BLOCK),
                      NestedBlockingType(BlockingType::WILL_BLOCK,
                                         OptionalBlockingType::MAY_BLOCK,
                                         BlockingType::WILL_BLOCK)),
    ThreadGroupImplBlockingTest::ParamInfoToString);

// Verify that if a thread enters the scope of a MAY_BLOCK ScopedBlockingCall,
// but exits the scope before the MayBlock threshold is reached, that the max
// tasks does not increase.
TEST_F(ThreadGroupImplBlockingTest, ThreadBlockUnblockPremature) {
  // Create a thread group with an infinite MayBlock threshold so that a
  // MAY_BLOCK ScopedBlockingCall never increases the max tasks.
  CreateAndStartThreadGroup(TimeDelta::Max(),   // |suggested_reclaim_time|
                            kMaxTasks,          // |max_tasks|
                            std::nullopt,       // |max_best_effort_tasks|
                            nullptr,            // |worker_observer|
                            TimeDelta::Max());  // |may_block_threshold|

  ASSERT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);

  SaturateWithBlockingTasks(NestedBlockingType(BlockingType::MAY_BLOCK,
                                               OptionalBlockingType::NO_BLOCK,
                                               BlockingType::MAY_BLOCK));
  PlatformThread::Sleep(
      2 * thread_group_->blocked_workers_poll_period_for_testing());
  EXPECT_EQ(thread_group_->NumberOfWorkersForTesting(), kMaxTasks);
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);

  UnblockBlockingTasks();
  task_tracker_.FlushForTesting();
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);
}

// Verify that if a BEST_EFFORT task enters the scope of a WILL_BLOCK
// ScopedBlockingCall, but exits the scope before the MayBlock threshold is
// reached, that the max best effort tasks does not increase.
TEST_F(ThreadGroupImplBlockingTest, ThreadBlockUnblockPrematureBestEffort) {
  // Create a thread group with an infinite MayBlock threshold so that a
  // MAY_BLOCK ScopedBlockingCall never increases the max tasks.
  CreateAndStartThreadGroup(TimeDelta::Max(),   // |suggested_reclaim_time|
                            kMaxTasks,          // |max_tasks|
                            kMaxTasks,          // |max_best_effort_tasks|
                            nullptr,            // |worker_observer|
                            TimeDelta::Max());  // |may_block_threshold|

  ASSERT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);
  ASSERT_EQ(thread_group_->GetMaxBestEffortTasksForTesting(), kMaxTasks);

  SaturateWithBlockingTasks(NestedBlockingType(BlockingType::WILL_BLOCK,
                                               OptionalBlockingType::NO_BLOCK,
                                               BlockingType::WILL_BLOCK),
                            TaskPriority::BEST_EFFORT);
  PlatformThread::Sleep(
      2 * thread_group_->blocked_workers_poll_period_for_testing());
  EXPECT_GE(thread_group_->NumberOfWorkersForTesting(), kMaxTasks);
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), 2 * kMaxTasks);
  EXPECT_EQ(thread_group_->GetMaxBestEffortTasksForTesting(), kMaxTasks);

  UnblockBlockingTasks();
  task_tracker_.FlushForTesting();
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);
  EXPECT_EQ(thread_group_->GetMaxBestEffortTasksForTesting(), kMaxTasks);
}

// Verify that if max tasks is incremented because of a MAY_BLOCK
// ScopedBlockingCall, it isn't incremented again when there is a nested
// WILL_BLOCK ScopedBlockingCall.
TEST_F(ThreadGroupImplBlockingTest, MayBlockIncreaseCapacityNestedWillBlock) {
  CreateAndStartThreadGroup();

  ASSERT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);
  auto task_runner =
      test::CreatePooledTaskRunner({MayBlock(), WithBaseSyncPrimitives()},
                                   &mock_pooled_task_runner_delegate_);
  TestWaitableEvent can_return;

  // Saturate the thread group so that a MAY_BLOCK ScopedBlockingCall would
  // increment the max tasks.
  for (size_t i = 0; i < kMaxTasks - 1; ++i) {
    task_runner->PostTask(
        FROM_HERE, BindOnce(&TestWaitableEvent::Wait, Unretained(&can_return)));
  }

  TestWaitableEvent can_instantiate_will_block;
  TestWaitableEvent did_instantiate_will_block;

  // Post a task that instantiates a MAY_BLOCK ScopedBlockingCall.
  task_runner->PostTask(
      FROM_HERE,
      BindOnce(
          [](TestWaitableEvent* can_instantiate_will_block,
             TestWaitableEvent* did_instantiate_will_block,
             TestWaitableEvent* can_return) {
            ScopedBlockingCall may_block(FROM_HERE, BlockingType::MAY_BLOCK);
            can_instantiate_will_block->Wait();
            ScopedBlockingCall will_block(FROM_HERE, BlockingType::WILL_BLOCK);
            did_instantiate_will_block->Signal();
            can_return->Wait();
          },
          Unretained(&can_instantiate_will_block),
          Unretained(&did_instantiate_will_block), Unretained(&can_return)));

  // After a short delay, max tasks should be incremented.
  ExpectMaxTasksIncreasesTo(kMaxTasks + 1);

  // Wait until the task instantiates a WILL_BLOCK ScopedBlockingCall.
  can_instantiate_will_block.Signal();
  did_instantiate_will_block.Wait();

  // Max tasks shouldn't be incremented again.
  EXPECT_EQ(kMaxTasks + 1, thread_group_->GetMaxTasksForTesting());

  // Tear down.
  can_return.Signal();
  task_tracker_.FlushForTesting();
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);
}

// Verify that OnShutdownStarted() causes max tasks to increase and creates a
// worker if needed. Also verify that UnblockBusyTasks() decreases max tasks
// after an increase.
TEST_F(ThreadGroupImplBlockingTest, ThreadBusyShutdown) {
  CreateAndStartThreadGroup();
  ASSERT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);

  SaturateWithBusyTasks(TaskPriority::BEST_EFFORT,
                        TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN);
  thread_group_->OnShutdownStarted();

  // Forces |kMaxTasks| extra workers to be instantiated by posting tasks. This
  // should not block forever.
  SaturateWithBusyTasks(TaskPriority::BEST_EFFORT,
                        TaskShutdownBehavior::BLOCK_SHUTDOWN);

  EXPECT_EQ(thread_group_->NumberOfWorkersForTesting(), 2 * kMaxTasks);

  UnblockBusyTasks();
  task_tracker_.FlushForTesting();
  thread_group_->JoinForTesting();
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks);
  mock_pooled_task_runner_delegate_.SetThreadGroup(nullptr);
  thread_group_.reset();
}

enum class ReclaimType { DELAYED_RECLAIM, NO_RECLAIM };

class ThreadGroupImplOverCapacityTest
    : public ThreadGroupImplImplTestBase,
      public testing::TestWithParam<ReclaimType> {
 public:
  ThreadGroupImplOverCapacityTest() = default;
  ThreadGroupImplOverCapacityTest(const ThreadGroupImplOverCapacityTest&) =
      delete;
  ThreadGroupImplOverCapacityTest& operator=(
      const ThreadGroupImplOverCapacityTest&) = delete;

  void SetUp() override {
    if (GetParam() == ReclaimType::NO_RECLAIM) {
      feature_list.InitAndEnableFeature(kNoWorkerThreadReclaim);
    }
    CreateThreadGroup();
    task_runner_ =
        test::CreatePooledTaskRunner({MayBlock(), WithBaseSyncPrimitives()},
                                     &mock_pooled_task_runner_delegate_);
  }

  void TearDown() override { ThreadGroupImplImplTestBase::CommonTearDown(); }

 protected:
  base::test::ScopedFeatureList feature_list;
  scoped_refptr<TaskRunner> task_runner_;
  static constexpr size_t kLocalMaxTasks = 3;

  void CreateThreadGroup() {
    ASSERT_FALSE(thread_group_);
    service_thread_.Start();
    delayed_task_manager_.Start(service_thread_.task_runner());
    thread_group_ = std::make_unique<ThreadGroupImpl>(
        "OverCapacityTestThreadGroup", "A", ThreadType::kDefault,
        task_tracker_.GetTrackedRef(), tracked_ref_factory_.GetTrackedRef());
    ASSERT_TRUE(thread_group_);

    mock_pooled_task_runner_delegate_.SetThreadGroup(thread_group_.get());
  }
};

// Verify that workers that become idle due to the thread group being over
// capacity will eventually cleanup.
TEST_P(ThreadGroupImplOverCapacityTest, VerifyCleanup) {
  StartThreadGroup(kReclaimTimeForCleanupTests, kLocalMaxTasks);
  TestWaitableEvent threads_running;
  TestWaitableEvent threads_continue;
  RepeatingClosure threads_running_barrier = BarrierClosure(
      kLocalMaxTasks,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running)));

  TestWaitableEvent blocked_call_continue;
  RepeatingClosure closure = BindRepeating(
      [](RepeatingClosure* threads_running_barrier,
         TestWaitableEvent* threads_continue,
         TestWaitableEvent* blocked_call_continue) {
        threads_running_barrier->Run();
        {
          ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                  BlockingType::WILL_BLOCK);
          blocked_call_continue->Wait();
        }
        threads_continue->Wait();
      },
      Unretained(&threads_running_barrier), Unretained(&threads_continue),
      Unretained(&blocked_call_continue));

  for (size_t i = 0; i < kLocalMaxTasks; ++i)
    task_runner_->PostTask(FROM_HERE, closure);

  threads_running.Wait();

  TestWaitableEvent extra_threads_running;
  TestWaitableEvent extra_threads_continue;

  RepeatingClosure extra_threads_running_barrier = BarrierClosure(
      kLocalMaxTasks,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&extra_threads_running)));
  // These tasks should run on the new threads from increasing max tasks.
  for (size_t i = 0; i < kLocalMaxTasks; ++i) {
    task_runner_->PostTask(
        FROM_HERE, BindOnce(
                       [](RepeatingClosure* extra_threads_running_barrier,
                          TestWaitableEvent* extra_threads_continue) {
                         extra_threads_running_barrier->Run();
                         extra_threads_continue->Wait();
                       },
                       Unretained(&extra_threads_running_barrier),
                       Unretained(&extra_threads_continue)));
  }
  extra_threads_running.Wait();

  ASSERT_EQ(kLocalMaxTasks * 2, thread_group_->NumberOfWorkersForTesting());
  EXPECT_EQ(kLocalMaxTasks * 2, thread_group_->GetMaxTasksForTesting());
  blocked_call_continue.Signal();
  extra_threads_continue.Signal();

  // Periodically post tasks to ensure that posting tasks does not prevent
  // workers that are idle due to the thread group being over capacity from
  // cleaning up.
  for (int i = 0; i < 16; ++i) {
    task_runner_->PostDelayedTask(FROM_HERE, DoNothing(),
                                  kReclaimTimeForCleanupTests * i * 0.5);
  }

  if (GetParam() == ReclaimType::DELAYED_RECLAIM) {
    // Note: one worker above capacity will not get cleaned up since it's on the
    // front of the idle set.
    thread_group_->WaitForWorkersCleanedUpForTesting(kLocalMaxTasks - 1);
    EXPECT_EQ(kLocalMaxTasks + 1, thread_group_->NumberOfWorkersForTesting());
    threads_continue.Signal();
  } else {
    // When workers are't automatically reclaimed after a delay, blocking tasks
    // need to return for extra workers to be cleaned up.
    threads_continue.Signal();
    thread_group_->WaitForWorkersCleanedUpForTesting(kLocalMaxTasks);
    EXPECT_EQ(kLocalMaxTasks, thread_group_->NumberOfWorkersForTesting());
  }

  threads_continue.Signal();
  task_tracker_.FlushForTesting();
}

INSTANTIATE_TEST_SUITE_P(ReclaimType,
                         ThreadGroupImplOverCapacityTest,
                         ::testing::Values(ReclaimType::DELAYED_RECLAIM,
                                           ReclaimType::NO_RECLAIM));

// Verify that the maximum number of workers is 256 and that hitting the max
// leaves the thread group in a valid state with regards to max tasks.
TEST_F(ThreadGroupImplBlockingTest, MaximumWorkersTest) {
  CreateAndStartThreadGroup();

  constexpr size_t kMaxNumberOfWorkers = 256;
  constexpr size_t kNumExtraTasks = 10;

  TestWaitableEvent early_blocking_threads_running;
  RepeatingClosure early_threads_barrier_closure =
      BarrierClosure(kMaxNumberOfWorkers,
                     BindOnce(&TestWaitableEvent::Signal,
                              Unretained(&early_blocking_threads_running)));

  TestWaitableEvent early_threads_finished;
  RepeatingClosure early_threads_finished_barrier = BarrierClosure(
      kMaxNumberOfWorkers, BindOnce(&TestWaitableEvent::Signal,
                                    Unretained(&early_threads_finished)));

  TestWaitableEvent early_release_threads_continue;

  // Post ScopedBlockingCall tasks to hit the worker cap.
  for (size_t i = 0; i < kMaxNumberOfWorkers; ++i) {
    task_runner_->PostTask(
        FROM_HERE, BindOnce(
                       [](RepeatingClosure* early_threads_barrier_closure,
                          TestWaitableEvent* early_release_threads_continue,
                          RepeatingClosure* early_threads_finished) {
                         {
                           ScopedBlockingCall scoped_blocking_call(
                               FROM_HERE, BlockingType::WILL_BLOCK);
                           early_threads_barrier_closure->Run();
                           early_release_threads_continue->Wait();
                         }
                         early_threads_finished->Run();
                       },
                       Unretained(&early_threads_barrier_closure),
                       Unretained(&early_release_threads_continue),
                       Unretained(&early_threads_finished_barrier)));
  }

  early_blocking_threads_running.Wait();
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(),
            kMaxTasks + kMaxNumberOfWorkers);

  TestWaitableEvent late_release_thread_contine;
  TestWaitableEvent late_blocking_threads_running;

  RepeatingClosure late_threads_barrier_closure = BarrierClosure(
      kNumExtraTasks, BindOnce(&TestWaitableEvent::Signal,
                               Unretained(&late_blocking_threads_running)));

  // Posts additional tasks. Note: we should already have |kMaxNumberOfWorkers|
  // tasks running. These tasks should not be able to get executed yet as the
  // thread group is already at its max worker cap.
  for (size_t i = 0; i < kNumExtraTasks; ++i) {
    task_runner_->PostTask(
        FROM_HERE, BindOnce(
                       [](RepeatingClosure* late_threads_barrier_closure,
                          TestWaitableEvent* late_release_thread_contine) {
                         ScopedBlockingCall scoped_blocking_call(
                             FROM_HERE, BlockingType::WILL_BLOCK);
                         late_threads_barrier_closure->Run();
                         late_release_thread_contine->Wait();
                       },
                       Unretained(&late_threads_barrier_closure),
                       Unretained(&late_release_thread_contine)));
  }

  // Give time to see if we exceed the max number of workers.
  PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  EXPECT_LE(thread_group_->NumberOfWorkersForTesting(), kMaxNumberOfWorkers);

  early_release_threads_continue.Signal();
  early_threads_finished.Wait();
  late_blocking_threads_running.Wait();

  TestWaitableEvent final_tasks_running;
  TestWaitableEvent final_tasks_continue;
  RepeatingClosure final_tasks_running_barrier = BarrierClosure(
      kMaxTasks,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&final_tasks_running)));

  // Verify that we are still able to saturate the thread group.
  for (size_t i = 0; i < kMaxTasks; ++i) {
    task_runner_->PostTask(FROM_HERE,
                           BindOnce(
                               [](RepeatingClosure* closure,
                                  TestWaitableEvent* final_tasks_continue) {
                                 closure->Run();
                                 final_tasks_continue->Wait();
                               },
                               Unretained(&final_tasks_running_barrier),
                               Unretained(&final_tasks_continue)));
  }
  final_tasks_running.Wait();
  EXPECT_EQ(thread_group_->GetMaxTasksForTesting(), kMaxTasks + kNumExtraTasks);
  late_release_thread_contine.Signal();
  final_tasks_continue.Signal();
  task_tracker_.FlushForTesting();
}

// Verify that the maximum number of best-effort tasks that can run concurrently
// is honored.
TEST_F(ThreadGroupImplImplStartInBodyTest, MaxBestEffortTasks) {
  constexpr int kMaxBestEffortTasks = kMaxTasks / 2;
  StartThreadGroup(TimeDelta::Max(),      // |suggested_reclaim_time|
                   kMaxTasks,             // |max_tasks|
                   kMaxBestEffortTasks);  // |max_best_effort_tasks|
  const scoped_refptr<TaskRunner> foreground_runner =
      test::CreatePooledTaskRunner({MayBlock()},
                                   &mock_pooled_task_runner_delegate_);
  const scoped_refptr<TaskRunner> background_runner =
      test::CreatePooledTaskRunner({TaskPriority::BEST_EFFORT, MayBlock()},
                                   &mock_pooled_task_runner_delegate_);

  // It should be possible to have |kMaxBestEffortTasks|
  // TaskPriority::BEST_EFFORT tasks running concurrently.
  TestWaitableEvent best_effort_tasks_running;
  TestWaitableEvent unblock_best_effort_tasks;
  RepeatingClosure best_effort_tasks_running_barrier = BarrierClosure(
      kMaxBestEffortTasks, BindOnce(&TestWaitableEvent::Signal,
                                    Unretained(&best_effort_tasks_running)));

  for (int i = 0; i < kMaxBestEffortTasks; ++i) {
    background_runner->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                                  best_effort_tasks_running_barrier.Run();
                                  unblock_best_effort_tasks.Wait();
                                }));
  }
  best_effort_tasks_running.Wait();

  // No more TaskPriority::BEST_EFFORT task should run.
  AtomicFlag extra_best_effort_task_can_run;
  TestWaitableEvent extra_best_effort_task_running;
  background_runner->PostTask(
      FROM_HERE, base::BindLambdaForTesting([&] {
        EXPECT_TRUE(extra_best_effort_task_can_run.IsSet());
        extra_best_effort_task_running.Signal();
      }));

  // An extra foreground task should be able to run.
  TestWaitableEvent foreground_task_running;
  foreground_runner->PostTask(
      FROM_HERE, base::BindOnce(&TestWaitableEvent::Signal,
                                Unretained(&foreground_task_running)));
  foreground_task_running.Wait();

  // Completion of the TaskPriority::BEST_EFFORT tasks should allow the extra
  // TaskPriority::BEST_EFFORT task to run.
  extra_best_effort_task_can_run.Set();
  unblock_best_effort_tasks.Signal();
  extra_best_effort_task_running.Wait();

  // Wait for all tasks to complete before exiting to avoid invalid accesses.
  task_tracker_.FlushForTesting();
}

// Verify that flooding the thread group with BEST_EFFORT tasks doesn't cause
// the creation of more than |max_best_effort_tasks| + 1 workers.
TEST_F(ThreadGroupImplImplStartInBodyTest,
       FloodBestEffortTasksDoesNotCreateTooManyWorkers) {
  constexpr size_t kMaxBestEffortTasks = kMaxTasks / 2;
  StartThreadGroup(TimeDelta::Max(),      // |suggested_reclaim_time|
                   kMaxTasks,             // |max_tasks|
                   kMaxBestEffortTasks);  // |max_best_effort_tasks|

  const scoped_refptr<TaskRunner> runner =
      test::CreatePooledTaskRunner({TaskPriority::BEST_EFFORT, MayBlock()},
                                   &mock_pooled_task_runner_delegate_);

  for (size_t i = 0; i < kLargeNumber; ++i) {
    runner->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                       EXPECT_LE(thread_group_->NumberOfWorkersForTesting(),
                                 kMaxBestEffortTasks + 1);
                     }));
  }

  // Wait for all tasks to complete before exiting to avoid invalid accesses.
  task_tracker_.FlushForTesting();
}

// Previously, a WILL_BLOCK ScopedBlockingCall unconditionally woke up a worker
// if the priority queue was non-empty. Sometimes, that caused multiple workers
// to be woken up for the same sequence. This test verifies that it is no longer
// the case:
// 1. Post and run task A.
// 2. Post task B from task A.
// 3. Task A enters a WILL_BLOCK ScopedBlockingCall. Once the idle thread is
//    created, this should no-op because there are already enough workers
//    (previously, a worker would be woken up because the priority queue isn't
//    empty).
// 5. Wait for all tasks to complete.
TEST_F(ThreadGroupImplImplStartInBodyTest,
       RepeatedWillBlockDoesNotCreateTooManyWorkers) {
  constexpr size_t kNumWorkers = 2U;
  StartThreadGroup(TimeDelta::Max(),  // |suggested_reclaim_time|
                   kNumWorkers,       // |max_tasks|
                   std::nullopt);     // |max_best_effort_tasks|
  const scoped_refptr<TaskRunner> runner = test::CreatePooledTaskRunner(
      {MayBlock()}, &mock_pooled_task_runner_delegate_);

  for (size_t i = 0; i < kLargeNumber; ++i) {
    runner->PostTask(FROM_HERE, BindLambdaForTesting([&] {
                       runner->PostTask(
                           FROM_HERE, BindLambdaForTesting([&] {
                             EXPECT_LE(
                                 thread_group_->NumberOfWorkersForTesting(),
                                 kNumWorkers + 1);
                           }));
                       // Number of workers should not increase when there is
                       // enough capacity to accommodate queued and running
                       // sequences.
                       ScopedBlockingCall scoped_blocking_call(
                           FROM_HERE, BlockingType::WILL_BLOCK);
                       EXPECT_EQ(kNumWorkers + 1,
                                 thread_group_->NumberOfWorkersForTesting());
                     }));
    // Wait for all tasks to complete.
    task_tracker_.FlushForTesting();
  }
}

namespace {

class ThreadGroupImplBlockingCallAndMaxBestEffortTasksTest
    : public ThreadGroupImplImplTestBase,
      public testing::TestWithParam<BlockingType> {
 public:
  static constexpr int kMaxBestEffortTasks = kMaxTasks / 2;

  ThreadGroupImplBlockingCallAndMaxBestEffortTasksTest() = default;
  ThreadGroupImplBlockingCallAndMaxBestEffortTasksTest(
      const ThreadGroupImplBlockingCallAndMaxBestEffortTasksTest&) = delete;
  ThreadGroupImplBlockingCallAndMaxBestEffortTasksTest& operator=(
      const ThreadGroupImplBlockingCallAndMaxBestEffortTasksTest&) = delete;

  void SetUp() override {
    CreateThreadGroup();
    thread_group_->Start(kMaxTasks, kMaxBestEffortTasks, base::TimeDelta::Max(),
                         service_thread_.task_runner(), nullptr,
                         ThreadGroup::WorkerEnvironment::NONE,
                         /*synchronous_thread_start_for_testing=*/false,
                         /*may_block_threshold=*/{});
  }

  void TearDown() override { ThreadGroupImplImplTestBase::CommonTearDown(); }

 private:
};

}  // namespace

TEST_P(ThreadGroupImplBlockingCallAndMaxBestEffortTasksTest,
       BlockingCallAndMaxBestEffortTasksTest) {
  const scoped_refptr<TaskRunner> background_runner =
      test::CreatePooledTaskRunner({TaskPriority::BEST_EFFORT, MayBlock()},
                                   &mock_pooled_task_runner_delegate_);

  // Post |kMaxBestEffortTasks| TaskPriority::BEST_EFFORT tasks that block in a
  // ScopedBlockingCall.
  TestWaitableEvent blocking_best_effort_tasks_running;
  TestWaitableEvent unblock_blocking_best_effort_tasks;
  RepeatingClosure blocking_best_effort_tasks_running_barrier =
      BarrierClosure(kMaxBestEffortTasks,
                     BindOnce(&TestWaitableEvent::Signal,
                              Unretained(&blocking_best_effort_tasks_running)));
  for (int i = 0; i < kMaxBestEffortTasks; ++i) {
    background_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          blocking_best_effort_tasks_running_barrier.Run();
          ScopedBlockingCall scoped_blocking_call(FROM_HERE, GetParam());
          unblock_blocking_best_effort_tasks.Wait();
        }));
  }
  blocking_best_effort_tasks_running.Wait();

  // Post an extra |kMaxBestEffortTasks| TaskPriority::BEST_EFFORT tasks. They
  // should be able to run, because the existing TaskPriority::BEST_EFFORT tasks
  // are blocked within a ScopedBlockingCall.
  //
  // Note: We block the tasks until they have all started running to make sure
  // that it is possible to run an extra |kMaxBestEffortTasks| concurrently.
  TestWaitableEvent best_effort_tasks_running;
  TestWaitableEvent unblock_best_effort_tasks;
  RepeatingClosure best_effort_tasks_running_barrier = BarrierClosure(
      kMaxBestEffortTasks, BindOnce(&TestWaitableEvent::Signal,
                                    Unretained(&best_effort_tasks_running)));
  for (int i = 0; i < kMaxBestEffortTasks; ++i) {
    background_runner->PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
                                  best_effort_tasks_running_barrier.Run();
                                  unblock_best_effort_tasks.Wait();
                                }));
  }
  best_effort_tasks_running.Wait();

  // Unblock all tasks and tear down.
  unblock_blocking_best_effort_tasks.Signal();
  unblock_best_effort_tasks.Signal();
  task_tracker_.FlushForTesting();
}

INSTANTIATE_TEST_SUITE_P(MayBlock,
                         ThreadGroupImplBlockingCallAndMaxBestEffortTasksTest,
                         ::testing::Values(BlockingType::MAY_BLOCK));
INSTANTIATE_TEST_SUITE_P(WillBlock,
                         ThreadGroupImplBlockingCallAndMaxBestEffortTasksTest,
                         ::testing::Values(BlockingType::WILL_BLOCK));

// Verify that worker detachment doesn't race with worker cleanup, regression
// test for https://crbug.com/810464.
TEST_F(ThreadGroupImplImplStartInBodyTest, RacyCleanup) {
  constexpr size_t kLocalMaxTasks = 256;
  constexpr TimeDelta kReclaimTimeForRacyCleanupTest = Milliseconds(10);

  thread_group_->Start(kLocalMaxTasks, kLocalMaxTasks,
                       kReclaimTimeForRacyCleanupTest,
                       service_thread_.task_runner(), nullptr,
                       ThreadGroup::WorkerEnvironment::NONE,
                       /*synchronous_thread_start_for_testing=*/false,
                       /*may_block_threshold=*/{});

  scoped_refptr<TaskRunner> task_runner = test::CreatePooledTaskRunner(
      {WithBaseSyncPrimitives()}, &mock_pooled_task_runner_delegate_);

  TestWaitableEvent threads_running;
  TestWaitableEvent unblock_threads;
  RepeatingClosure threads_running_barrier = BarrierClosure(
      kLocalMaxTasks,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&threads_running)));

  for (size_t i = 0; i < kLocalMaxTasks; ++i) {
    task_runner->PostTask(
        FROM_HERE,
        BindOnce(
            [](OnceClosure on_running, TestWaitableEvent* unblock_threads) {
              std::move(on_running).Run();
              unblock_threads->Wait();
            },
            threads_running_barrier, Unretained(&unblock_threads)));
  }

  // Wait for all workers to be ready and release them all at once.
  threads_running.Wait();
  unblock_threads.Signal();

  // Sleep to wakeup precisely when all workers are going to try to cleanup per
  // being idle.
  PlatformThread::Sleep(kReclaimTimeForRacyCleanupTest);

  thread_group_->JoinForTesting();

  // Unwinding this test will be racy if worker cleanup can race with
  // ThreadGroupImpl destruction : https://crbug.com/810464.
  mock_pooled_task_runner_delegate_.SetThreadGroup(nullptr);
  thread_group_.reset();
}

}  // namespace internal
}  // namespace base
