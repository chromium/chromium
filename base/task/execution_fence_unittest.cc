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
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/tracing/protos/chrome_track_event.pbzero.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using ::testing::_;
using TaskQueue = sequence_manager::TaskQueue;

// Types of task to post while a fence is up.
enum class TaskType {
  // ThreadPool task with default priority.
  kThreadPoolDefault,
  // ThreadPool task with best-effort priority.
  kThreadPoolBestEffort,
  // Task posted to a TaskQueue with default priority.
  kTaskQueueDefault,
};
using TaskTypes = EnumSet<TaskType,
                          TaskType::kThreadPoolDefault,
                          TaskType::kTaskQueueDefault>;

std::ostream& operator<<(std::ostream& os, TaskType task_type) {
  switch (task_type) {
    case TaskType::kThreadPoolDefault:
      return os << "kThreadPoolDefault";
    case TaskType::kThreadPoolBestEffort:
      return os << "kThreadPoolBestEffort";
    case TaskType::kTaskQueueDefault:
      return os << "kTaskQueueDefault";
  }
}

std::ostream& operator<<(std::ostream& os, TaskTypes task_types) {
  std::string sep = "";
  os << "[";
  for (TaskType task_type : task_types) {
    os << sep << task_type;
    sep = ",";
  }
  return os << "]";
}

// A TaskEnvironment that creates an extra TaskQueue, to give a destination for
// PostTask while the test runs on the main thread.
class TaskEnvironmentWithExtraTaskQueue final : public test::TaskEnvironment {
 public:
  // Don't use MOCK_TIME since it doesn't run ThreadPool tasks without using
  // RunUntilIdle, which spins forever if there are any tasks blocked by fences.
  TaskEnvironmentWithExtraTaskQueue()
      : test::TaskEnvironment(SubclassCreatesDefaultTaskRunner{}) {
    DeferredInitFromSubclass(default_task_queue_->task_runner());
  }

  scoped_refptr<SequencedTaskRunner> task_queue_task_runner() {
    return extra_task_queue_->task_runner();
  }

 private:
  TaskQueue::Handle default_task_queue_ =
      sequence_manager()->CreateTaskQueue(TaskQueue::Spec(
          sequence_manager::QueueName::TASK_ENVIRONMENT_DEFAULT_TQ));
  TaskQueue::Handle extra_task_queue_ = sequence_manager()->CreateTaskQueue(
      TaskQueue::Spec(sequence_manager::QueueName::TEST_TQ));
};

}  // namespace

class ExecutionFenceTest : public ::testing::Test {
 public:
  ~ExecutionFenceTest() override {
    // Flush every task runner being tested.
    RepeatingClosure barrier_closure =
        BarrierClosure(TaskTypes::All().size(), task_env_.QuitClosure());
    for (TaskType task_type : TaskTypes::All()) {
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
  void RunPostedTasksAndExpect(TaskTypes expected_tasks,
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
    if (expected_tasks != TaskTypes::All()) {
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
    for (TaskType task_type : TaskTypes::All()) {
      task_runners_.at(task_type)->PostTask(
          FROM_HERE, BindLambdaForTesting([this, task_type] {
            AutoLock lock(tasks_that_ran_lock_);
            tasks_that_ran_.Put(task_type);
          }));
    }
  }

 protected:
  TaskEnvironmentWithExtraTaskQueue task_env_;

  // A TaskRunner for each TaskType.
  std::map<TaskType, scoped_refptr<SequencedTaskRunner>> task_runners_{
      {TaskType::kThreadPoolDefault, ThreadPool::CreateSequencedTaskRunner({})},
      {TaskType::kThreadPoolBestEffort,
       ThreadPool::CreateSequencedTaskRunner({TaskPriority::BEST_EFFORT})},
      {TaskType::kTaskQueueDefault, task_env_.task_queue_task_runner()},
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
  TaskTypes tasks_that_ran_ GUARDED_BY(tasks_that_ran_lock_);
};

TEST_F(ExecutionFenceTest, SingleFence) {
  {
    ScopedBestEffortExecutionFence best_effort_fence;

    // While this fence is up, only default-priority tasks should run.
    PostTestTasks();
    RunPostedTasksAndExpect(
        {TaskType::kThreadPoolDefault, TaskType::kTaskQueueDefault});
  }

  // After bringing the fence down, unblocked best-effort tasks should run.
  RunPostedTasksAndExpect({TaskType::kThreadPoolBestEffort});

  // Now that the fence is down all tasks should run.
  PostTestTasks();
  RunPostedTasksAndExpect(TaskTypes::All());

  {
    ScopedThreadPoolExecutionFence thread_pool_fence;

    // While this fence is up, only TaskQueue tasks should run.
    PostTestTasks();
    RunPostedTasksAndExpect({TaskType::kTaskQueueDefault});
  }

  // After bringing the fence down, unblocked ThreadPool tasks should run.
  RunPostedTasksAndExpect(
      {TaskType::kThreadPoolDefault, TaskType::kThreadPoolBestEffort});

  // No more fences. All posted tasks run.
  PostTestTasks();
  RunPostedTasksAndExpect(TaskTypes::All());
}

TEST_F(ExecutionFenceTest, NestedFences) {
  auto best_effort_fence1 =
      std::make_optional<ScopedBestEffortExecutionFence>();
  auto best_effort_fence2 =
      std::make_optional<ScopedBestEffortExecutionFence>();

  // While these fences are up, only default-priority tasks should run.
  PostTestTasks();
  RunPostedTasksAndExpect(
      {TaskType::kThreadPoolDefault, TaskType::kTaskQueueDefault});

  auto thread_pool_fence1 =
      std::make_optional<ScopedThreadPoolExecutionFence>();
  auto thread_pool_fence2 =
      std::make_optional<ScopedThreadPoolExecutionFence>();

  // Now both types of fence are up, so only TaskPool tasks should run.
  PostTestTasks();
  RunPostedTasksAndExpect({TaskType::kTaskQueueDefault});

  thread_pool_fence2.reset();

  // Still a fence up, so nothing should be unblocked.
  RunPostedTasksAndExpect({});

  // New ThreadPool tasks still shouldn't run.
  PostTestTasks();
  RunPostedTasksAndExpect({TaskType::kTaskQueueDefault});

  thread_pool_fence1.reset();

  // After bringing the last ThreadPool fence down, unblocked ThreadPool
  // tasks should run.
  RunPostedTasksAndExpect({TaskType::kThreadPoolDefault});

  // But new best-effort tasks shouldn't.
  PostTestTasks();
  RunPostedTasksAndExpect(
      {TaskType::kThreadPoolDefault, TaskType::kTaskQueueDefault});

  best_effort_fence2.reset();

  // Still a best-effort fence up, so nothing should be unblocked.
  RunPostedTasksAndExpect({});

  // New best-effort tasks still shouldn't run.
  PostTestTasks();
  RunPostedTasksAndExpect(
      {TaskType::kThreadPoolDefault, TaskType::kTaskQueueDefault});

  best_effort_fence1.reset();

  // After bringing the last fence down, unblocked best-effort tasks should
  // run.
  RunPostedTasksAndExpect({TaskType::kThreadPoolBestEffort});

  // No more fences. All posted tasks run.
  PostTestTasks();
  RunPostedTasksAndExpect(TaskTypes::All());
}

TEST_F(ExecutionFenceTest, StaggeredFences) {
  auto best_effort_fence1 =
      std::make_optional<ScopedBestEffortExecutionFence>();

  // Best-effort tasks don't run.
  PostTestTasks();
  RunPostedTasksAndExpect(
      {TaskType::kThreadPoolDefault, TaskType::kTaskQueueDefault});

  auto thread_pool_fence1 =
      std::make_optional<ScopedThreadPoolExecutionFence>();

  // Best-effort and ThreadPool tasks don't run.
  PostTestTasks();
  RunPostedTasksAndExpect({TaskType::kTaskQueueDefault});

  auto best_effort_fence2 =
      std::make_optional<ScopedBestEffortExecutionFence>();
  auto thread_pool_fence2 =
      std::make_optional<ScopedThreadPoolExecutionFence>();

  // Best-effort and ThreadPool tasks still don't run.
  PostTestTasks();
  RunPostedTasksAndExpect({TaskType::kTaskQueueDefault});

  // Bring down the first best-effort fence. Another one's still up, so
  // nothing's unblocked.
  best_effort_fence1.reset();
  RunPostedTasksAndExpect({});

  // Bring down the first ThreadPool fence. Another one's still up, so nothing's
  // unblocked.
  thread_pool_fence1.reset();
  RunPostedTasksAndExpect({});

  // Bring down the second best-effort fence. All best-effort tasks are on the
  // ThreadPool, so still nothing's unblocked.
  best_effort_fence2.reset();
  RunPostedTasksAndExpect({});

  // Bring down the second ThreadPool fence. All tasks are now unblocked.
  thread_pool_fence2.reset();
  RunPostedTasksAndExpect(
      {TaskType::kThreadPoolDefault, TaskType::kThreadPoolBestEffort});

  // No more fences. All posted tasks run.
  PostTestTasks();
  RunPostedTasksAndExpect(TaskTypes::All());
}

}  // namespace base
