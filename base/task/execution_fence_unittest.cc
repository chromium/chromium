// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/execution_fence.h"

#include <map>
#include <optional>
#include <ostream>
#include <string>

#include "base/barrier_closure.h"
#include "base/containers/enum_set.h"
#include "base/features.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using ::testing::_;

// Types of task to post while a fence is up.
enum class TaskType {
  // ThreadPool task with default priority.
  kThreadPoolDefault,
  // ThreadPool task with best-effort priority.
  kThreadPoolBestEffort,
  // Task posted to a TaskQueue with default priority.
  kTaskQueueDefault,
  // Task posted to a TaskQueue with best-effort priority.
  kTaskQueueBestEffort,
};
using TaskTypeSet = EnumSet<TaskType,
                            TaskType::kThreadPoolDefault,
                            TaskType::kTaskQueueBestEffort>;

std::ostream& operator<<(std::ostream& os, TaskType task_type) {
  switch (task_type) {
    case TaskType::kThreadPoolDefault:
      return os << "kThreadPoolDefault";
    case TaskType::kThreadPoolBestEffort:
      return os << "kThreadPoolBestEffort";
    case TaskType::kTaskQueueDefault:
      return os << "kTaskQueueDefault";
    case TaskType::kTaskQueueBestEffort:
      return os << "kTaskQueueBestEffort";
  }
}

std::ostream& operator<<(std::ostream& os, TaskTypeSet task_types) {
  std::string sep = "";
  os << "[";
  for (TaskType task_type : task_types) {
    os << sep << task_type;
    sep = ",";
  }
  return os << "]";
}

}  // namespace

struct TestParams {
  // Whether or not ScopedBestEffortExecutionFence should block TaskQueue tasks.
  bool block_best_effort_task_queue = false;

  // All TaskQueue task types that should run while a
  // ScopedBestEffortExecutionFence is up.
  TaskTypeSet task_queue_types_during_best_effort_fence;

  // All TaskQueue task types that should run as soon as the last
  // ScopedBestEffortExecutionFence comes down.
  TaskTypeSet task_queue_types_after_best_effort_fence;
};

class ExecutionFenceTest : public ::testing::TestWithParam<TestParams> {
 public:
  ExecutionFenceTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kScopedBestEffortExecutionFenceForTaskQueue,
        GetParam().block_best_effort_task_queue);
  }

  ~ExecutionFenceTest() override {
    // Flush every task runner being tested.
    RepeatingClosure barrier_closure =
        BarrierClosure(TaskTypeSet::All().size(), task_env_.QuitClosure());
    for (TaskType task_type : TaskTypeSet::All()) {
      task_runners_.at(task_type)->PostTask(FROM_HERE, barrier_closure);
    }
    task_env_.RunUntilQuit();
  }

  // Wait for all tasks to get a chance to run.
  void TinyWait() {
    task_env_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, task_env_.QuitClosure(), TestTimeouts::tiny_timeout());
    task_env_.RunUntilQuit();
  }

  // Wait for all posted tasks to get a chance to run, and then expect that one
  // each of `expected_tasks` ran since the last call.
  void RunPostedTasksAndExpect(TaskTypeSet expected_tasks,
                               const Location& location = Location::Current()) {
    SCOPED_TRACE(location.ToString());

    // Wait for all expected tasks to run. If a task is blocked incorrectly, the
    // test will time out.
    {
      AutoLock lock(tasks_that_ran_lock_);
      while (!tasks_that_ran_.HasAll(expected_tasks)) {
        AutoUnlock unlock(tasks_that_ran_lock_);
        TinyWait();
      }
    }

    // If we expect any blocked tasks, wait a bit to make sure they don't run.
    // There's a chance that a task won't be scheduled until after TinyWait(),
    // but it's small. Since this is testing for tasks that run when they're not
    // supposed to, missing the timeout would be a false negative. So a flaky
    // test should be considered a failure - it usually fails (correctly
    // detecting an error) but occasionally succeeds (incorrectly).
    if (expected_tasks != TaskTypeSet::All()) {
      TinyWait();
    }

    // Whew. Now make sure the exact expected task types ran.
    AutoLock lock(tasks_that_ran_lock_);
    EXPECT_EQ(tasks_that_ran_, expected_tasks);
    tasks_that_ran_.Clear();
  }

  // Post a task of each type.
  void PostTestTasks() {
    {
      AutoLock lock(tasks_that_ran_lock_);
      ASSERT_TRUE(tasks_that_ran_.empty());
    }
    for (TaskType task_type : TaskTypeSet::All()) {
      task_runners_.at(task_type)->PostTask(
          FROM_HERE, BindLambdaForTesting([this, task_type] {
            AutoLock lock(tasks_that_ran_lock_);
            tasks_that_ran_.Put(task_type);
          }));
    }
  }

 protected:
  test::ScopedFeatureList scoped_feature_list_;
  test::TaskEnvironmentWithMainThreadPriorities task_env_{
      test::TaskEnvironment::ScopedExecutionFenceBehaviour::
          MAIN_THREAD_AND_THREAD_POOL};

  // A TaskRunner for each TaskType.
  std::map<TaskType, scoped_refptr<SequencedTaskRunner>> task_runners_{
      {TaskType::kThreadPoolDefault, ThreadPool::CreateSequencedTaskRunner({})},
      {TaskType::kThreadPoolBestEffort,
       ThreadPool::CreateSequencedTaskRunner({TaskPriority::BEST_EFFORT})},
      {TaskType::kTaskQueueDefault, task_env_.GetMainThreadTaskRunner()},
      {TaskType::kTaskQueueBestEffort,
       task_env_.GetMainThreadTaskRunnerWithPriority(
           TaskPriority::BEST_EFFORT)},
  };

  // Lock protecting `tasks_that_ran_`. This doesn't need to be held while
  // bringing down a fence since the test waits for all tasks to finish running
  // before modifying the fence on the main thread, so there's no chance for
  // tasks on other threads to run before taking the lock. (Unless there's a
  // flake as described in RunPostedTasksAndExpect(), but then taking the lock
  // would hide the flake - a task might incorrectly start running with the
  // fence still up, but context switch before taking the lock, and then block
  // while the main thread takes the lock and checks `tasks_that_ran_`, making
  // it appear to run after the fence goes down.)
  Lock tasks_that_ran_lock_;

  // Each type of tasks that executes between calls to
  // RunPostedTasksAndExpect(). This is updated from multiple sequences and read
  // from the main thread.
  TaskTypeSet tasks_that_ran_ GUARDED_BY(tasks_that_ran_lock_);
};

INSTANTIATE_TEST_SUITE_P(All,
                         ExecutionFenceTest,
                         ::testing::Values(
                             TestParams{
                                 .block_best_effort_task_queue = false,
                                 .task_queue_types_during_best_effort_fence =
                                     {TaskType::kTaskQueueDefault,
                                      TaskType::kTaskQueueBestEffort},
                                 .task_queue_types_after_best_effort_fence = {},
                             },
                             TestParams{
                                 .block_best_effort_task_queue = true,
                                 .task_queue_types_during_best_effort_fence =
                                     {TaskType::kTaskQueueDefault},
                                 .task_queue_types_after_best_effort_fence =
                                     {TaskType::kTaskQueueBestEffort},
                             }));

TEST_P(ExecutionFenceTest, BestEffortFence) {
  {
    ScopedBestEffortExecutionFence best_effort_fence;

    // While this fence is up, only default-priority tasks should run.
    PostTestTasks();
    RunPostedTasksAndExpect(
        Union({TaskType::kThreadPoolDefault},
              GetParam().task_queue_types_during_best_effort_fence));
  }

  // After bringing the fence down, unblocked best-effort tasks should run.
  RunPostedTasksAndExpect(
      Union({TaskType::kThreadPoolBestEffort},
            GetParam().task_queue_types_after_best_effort_fence));

  // Now that the fence is down all tasks should run.
  PostTestTasks();
  RunPostedTasksAndExpect(TaskTypeSet::All());
}

TEST_P(ExecutionFenceTest, ThreadPoolFence) {
  {
    ScopedThreadPoolExecutionFence thread_pool_fence;

    // While this fence is up, only TaskQueue tasks should run.
    PostTestTasks();
    RunPostedTasksAndExpect(
        {TaskType::kTaskQueueDefault, TaskType::kTaskQueueBestEffort});
  }

  // After bringing the fence down, unblocked ThreadPool tasks should run.
  RunPostedTasksAndExpect(
      {TaskType::kThreadPoolDefault, TaskType::kThreadPoolBestEffort});

  // No more fences. All posted tasks run.
  PostTestTasks();
  RunPostedTasksAndExpect(TaskTypeSet::All());
}

TEST_P(ExecutionFenceTest, NestedFences) {
  auto best_effort_fence1 =
      std::make_optional<ScopedBestEffortExecutionFence>();
  auto best_effort_fence2 =
      std::make_optional<ScopedBestEffortExecutionFence>();

  // While these fences are up, only default-priority tasks should run.
  PostTestTasks();
  RunPostedTasksAndExpect(
      Union({TaskType::kThreadPoolDefault},
            GetParam().task_queue_types_during_best_effort_fence));

  auto thread_pool_fence1 =
      std::make_optional<ScopedThreadPoolExecutionFence>();
  auto thread_pool_fence2 =
      std::make_optional<ScopedThreadPoolExecutionFence>();

  // Now both types of fence are up, so only TaskQueue tasks should run.
  PostTestTasks();
  RunPostedTasksAndExpect(GetParam().task_queue_types_during_best_effort_fence);

  thread_pool_fence2.reset();

  // Still a fence up, so nothing should be unblocked.
  RunPostedTasksAndExpect({});

  // New ThreadPool tasks still shouldn't run.
  PostTestTasks();
  RunPostedTasksAndExpect(GetParam().task_queue_types_during_best_effort_fence);

  thread_pool_fence1.reset();

  // After bringing the last ThreadPool fence down, unblocked ThreadPool
  // tasks should run.
  RunPostedTasksAndExpect({TaskType::kThreadPoolDefault});

  // But new best-effort tasks shouldn't.
  PostTestTasks();
  RunPostedTasksAndExpect(
      Union({TaskType::kThreadPoolDefault},
            GetParam().task_queue_types_during_best_effort_fence));

  best_effort_fence2.reset();

  // Still a best-effort fence up, so nothing should be unblocked.
  RunPostedTasksAndExpect({});

  // New best-effort tasks still shouldn't run.
  PostTestTasks();
  RunPostedTasksAndExpect(
      Union({TaskType::kThreadPoolDefault},
            GetParam().task_queue_types_during_best_effort_fence));

  best_effort_fence1.reset();

  // After bringing the last fence down, unblocked best-effort tasks should
  // run.
  RunPostedTasksAndExpect(
      Union({TaskType::kThreadPoolBestEffort},
            GetParam().task_queue_types_after_best_effort_fence));

  // No more fences. All posted tasks run.
  PostTestTasks();
  RunPostedTasksAndExpect(TaskTypeSet::All());
}

TEST_P(ExecutionFenceTest, StaggeredFences) {
  auto best_effort_fence1 =
      std::make_optional<ScopedBestEffortExecutionFence>();

  // Best-effort tasks don't run.
  PostTestTasks();
  RunPostedTasksAndExpect(
      Union({TaskType::kThreadPoolDefault},
            GetParam().task_queue_types_during_best_effort_fence));

  auto thread_pool_fence1 =
      std::make_optional<ScopedThreadPoolExecutionFence>();

  // Best-effort and ThreadPool tasks don't run.
  PostTestTasks();
  RunPostedTasksAndExpect(GetParam().task_queue_types_during_best_effort_fence);

  auto best_effort_fence2 =
      std::make_optional<ScopedBestEffortExecutionFence>();
  auto thread_pool_fence2 =
      std::make_optional<ScopedThreadPoolExecutionFence>();

  // Best-effort and ThreadPool tasks still don't run.
  PostTestTasks();
  RunPostedTasksAndExpect(GetParam().task_queue_types_during_best_effort_fence);

  // Bring down the first best-effort fence. Another one's still up, so
  // nothing's unblocked.
  best_effort_fence1.reset();
  RunPostedTasksAndExpect({});

  // Bring down the first ThreadPool fence. Another one's still up, so nothing's
  // unblocked.
  thread_pool_fence1.reset();
  RunPostedTasksAndExpect({});

  // Bring down the second best-effort fence. Only best-effort TaskQueue tasks
  // are unblocked.
  best_effort_fence2.reset();
  RunPostedTasksAndExpect(GetParam().task_queue_types_after_best_effort_fence);

  // Bring down the second ThreadPool fence. All tasks are now unblocked.
  thread_pool_fence2.reset();
  RunPostedTasksAndExpect(
      {TaskType::kThreadPoolDefault, TaskType::kThreadPoolBestEffort});

  // No more fences. All posted tasks run.
  PostTestTasks();
  RunPostedTasksAndExpect(TaskTypeSet::All());
}

}  // namespace base
