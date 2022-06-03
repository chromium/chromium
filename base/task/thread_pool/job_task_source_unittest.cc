// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/job_task_source.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/task/thread_pool/pooled_task_runner_delegate.h"
#include "base/task/thread_pool/test_utils.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace base {
namespace internal {

class MockPooledTaskRunnerDelegate : public PooledTaskRunnerDelegate {
 public:
  MOCK_METHOD2(PostTaskWithSequence,
               bool(Task task, scoped_refptr<Sequence> sequence));
  MOCK_METHOD1(ShouldYield, bool(const TaskSource* task_source));
  MOCK_METHOD1(EnqueueJobTaskSource,
               bool(scoped_refptr<JobTaskSource> task_source));
  MOCK_METHOD1(RemoveJobTaskSource,
               void(scoped_refptr<JobTaskSource> task_source));
  MOCK_CONST_METHOD1(IsRunningPoolWithTraits, bool(const TaskTraits& traits));
  MOCK_METHOD2(UpdatePriority,
               void(scoped_refptr<TaskSource> task_source,
                    TaskPriority priority));
  MOCK_METHOD2(UpdateJobPriority,
               void(scoped_refptr<TaskSource> task_source,
                    TaskPriority priority));
};

class ThreadPoolJobTaskSourceTest : public testing::Test {
 protected:
  testing::StrictMock<MockPooledTaskRunnerDelegate>
      pooled_task_runner_delegate_;
};

// Verifies the normal flow of running 2 tasks one after the other.
TEST_F(ThreadPoolJobTaskSourceTest, RunTasks) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 2);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);
  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(task_source);

  EXPECT_EQ(2U, task_source->GetRemainingConcurrency());
  {
    EXPECT_EQ(registered_task_source.WillRunTask(),
              TaskSource::RunStatus::kAllowedNotSaturated);
    EXPECT_EQ(1U, task_source->GetWorkerCount());

    auto task = registered_task_source.TakeTask();
    std::move(task.task).Run();
    EXPECT_TRUE(registered_task_source.DidProcessTask());
    EXPECT_EQ(0U, task_source->GetWorkerCount());
  }
  {
    EXPECT_EQ(registered_task_source.WillRunTask(),
              TaskSource::RunStatus::kAllowedSaturated);
    EXPECT_EQ(1U, task_source->GetWorkerCount());

    // An attempt to run an additional task is not allowed.
    EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
              TaskSource::RunStatus::kDisallowed);
    EXPECT_EQ(0U, task_source->GetRemainingConcurrency());
    auto task = registered_task_source.TakeTask();
    EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
              TaskSource::RunStatus::kDisallowed);

    std::move(task.task).Run();
    EXPECT_EQ(0U, task_source->GetRemainingConcurrency());
    EXPECT_TRUE(task_source->IsActive());
    // Returns false because the task source is out of tasks.
    EXPECT_FALSE(registered_task_source.DidProcessTask());
    EXPECT_EQ(0U, task_source->GetWorkerCount());
    EXPECT_FALSE(task_source->IsActive());
  }
}

// Verifies that a job task source doesn't allow any new RunStatus after Clear()
// is called.
TEST_F(ThreadPoolJobTaskSourceTest, Clear) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 5);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  EXPECT_EQ(5U, task_source->GetRemainingConcurrency());
  auto registered_task_source_a =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_a.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);
  auto task_a = registered_task_source_a.TakeTask();

  auto registered_task_source_b =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_b.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);

  auto registered_task_source_c =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_c.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);

  auto registered_task_source_d =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_d.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);

  EXPECT_FALSE(task_source->ShouldYield());

  {
    EXPECT_EQ(1U, task_source->GetRemainingConcurrency());
    auto task = registered_task_source_c.Clear();
    std::move(task.task).Run();
    registered_task_source_c.DidProcessTask();
    EXPECT_EQ(0U, task_source->GetRemainingConcurrency());
  }
  // The task source shouldn't allow any further tasks after Clear.
  EXPECT_TRUE(task_source->ShouldYield());
  EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
            TaskSource::RunStatus::kDisallowed);

  // Another outstanding RunStatus can still call Clear.
  {
    auto task = registered_task_source_d.Clear();
    std::move(task.task).Run();
    registered_task_source_d.DidProcessTask();
    EXPECT_EQ(0U, task_source->GetRemainingConcurrency());
  }

  // A task that was already acquired can still run.
  std::move(task_a.task).Run();
  registered_task_source_a.DidProcessTask();

  // A valid outstanding RunStatus can also take and run a task.
  {
    auto task = registered_task_source_b.TakeTask();
    std::move(task.task).Run();
    registered_task_source_b.DidProcessTask();
  }
}

// Verifies that a job task source doesn't return an "allowed" RunStatus after
// Cancel() is called.
TEST_F(ThreadPoolJobTaskSourceTest, Cancel) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 3);
  scoped_refptr<JobTaskSource> task_source = job_task->GetJobTaskSource(
      FROM_HERE, {TaskPriority::BEST_EFFORT}, &pooled_task_runner_delegate_);

  auto registered_task_source_a =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_a.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);
  auto task_a = registered_task_source_a.TakeTask();

  auto registered_task_source_b =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_b.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);

  EXPECT_FALSE(task_source->ShouldYield());

  task_source->Cancel();
  EXPECT_TRUE(task_source->ShouldYield());

  // The task source shouldn't allow any further tasks after Cancel.
  EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
            TaskSource::RunStatus::kDisallowed);

  // A task that was already acquired can still run.
  std::move(task_a.task).Run();
  registered_task_source_a.DidProcessTask();

  // A RegisteredTaskSource that's ready can also take and run a task.
  {
    auto task = registered_task_source_b.TakeTask();
    std::move(task.task).Run();
    registered_task_source_b.DidProcessTask();
  }
}

// Verifies that multiple tasks can run in parallel up to |max_concurrency|.
TEST_F(ThreadPoolJobTaskSourceTest, RunTasksInParallel) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 2);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  auto registered_task_source_a =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_a.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);
  EXPECT_EQ(1U, task_source->GetWorkerCount());
  EXPECT_EQ(1U, task_source->GetSortKey().worker_count());
  auto task_a = registered_task_source_a.TakeTask();

  auto registered_task_source_b =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_b.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  EXPECT_EQ(2U, task_source->GetWorkerCount());
  EXPECT_EQ(2U, task_source->GetSortKey().worker_count());
  auto task_b = registered_task_source_b.TakeTask();

  // WillRunTask() should return a null RunStatus once the max concurrency is
  // reached.
  EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
            TaskSource::RunStatus::kDisallowed);

  std::move(task_a.task).Run();
  // Adding a task before closing the first run operation should cause the task
  // source to re-enqueue.
  job_task->SetNumTasksToRun(2);
  EXPECT_TRUE(registered_task_source_a.DidProcessTask());
  EXPECT_EQ(1U, task_source->GetSortKey().worker_count());

  std::move(task_b.task).Run();
  EXPECT_TRUE(registered_task_source_b.DidProcessTask());
  EXPECT_EQ(0U, task_source->GetSortKey().worker_count());

  EXPECT_EQ(0U, task_source->GetWorkerCount());

  auto registered_task_source_c =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_c.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  auto task_c = registered_task_source_c.TakeTask();

  std::move(task_c.task).Run();
  EXPECT_FALSE(registered_task_source_c.DidProcessTask());
}

// Verifies the normal flow of running the join task until completion.
TEST_F(ThreadPoolJobTaskSourceTest, RunJoinTask) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 2);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  EXPECT_TRUE(task_source->WillJoin());
  // Intentionally run |worker_task| twice to make sure RunJoinTask() calls
  // it again. This can happen in production if the joining thread spuriously
  // return and needs to run again.
  EXPECT_TRUE(task_source->RunJoinTask());
  EXPECT_FALSE(task_source->RunJoinTask());
}

// Verify that |worker_count| excludes the (inactive) returning thread calling
// max_concurrency_callback.
TEST_F(ThreadPoolJobTaskSourceTest, RunTaskWorkerCount) {
  size_t max_concurrency = 1;
  scoped_refptr<JobTaskSource> task_source =
      base::MakeRefCounted<JobTaskSource>(
          FROM_HERE, TaskTraits(),
          BindLambdaForTesting(
              [&](JobDelegate* delegate) { --max_concurrency; }),
          BindLambdaForTesting([&](size_t worker_count) -> size_t {
            return max_concurrency + worker_count;
          }),
          &pooled_task_runner_delegate_);

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(task_source);

  EXPECT_EQ(registered_task_source.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  auto task = registered_task_source.TakeTask();
  std::move(task.task).Run();
  // Once the worker_task runs, |worker_count| should drop to 0 and the job
  // should finish.
  EXPECT_FALSE(registered_task_source.DidProcessTask());
  EXPECT_EQ(0U, max_concurrency);
}

// Verify that |worker_count| excludes the (inactive) joining thread calling
// max_concurrency_callback.
TEST_F(ThreadPoolJobTaskSourceTest, RunJoinTaskWorkerCount) {
  size_t max_concurrency = 1;
  scoped_refptr<JobTaskSource> task_source =
      base::MakeRefCounted<JobTaskSource>(
          FROM_HERE, TaskTraits(),
          BindLambdaForTesting(
              [&](JobDelegate* delegate) { --max_concurrency; }),
          BindLambdaForTesting([&](size_t worker_count) -> size_t {
            return max_concurrency + worker_count;
          }),
          &pooled_task_runner_delegate_);

  EXPECT_TRUE(task_source->WillJoin());
  // Once the worker_task runs, |worker_count| should drop to 0 and the job
  // should finish.
  EXPECT_FALSE(task_source->RunJoinTask());
  EXPECT_EQ(0U, max_concurrency);
}

// Verifies that WillJoin() doesn't allow a joining thread to contribute
// after Cancel() is called.
TEST_F(ThreadPoolJobTaskSourceTest, CancelJoinTask) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 2);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  task_source->Cancel();
  EXPECT_FALSE(task_source->WillJoin());
}

// Verifies that RunJoinTask() doesn't allow a joining thread to contribute
// after Cancel() is called.
TEST_F(ThreadPoolJobTaskSourceTest, JoinCancelTask) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 2);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  EXPECT_TRUE(task_source->WillJoin());
  task_source->Cancel();
  EXPECT_FALSE(task_source->RunJoinTask());
}

// Verifies that the join task can run in parallel with worker tasks up to
// |max_concurrency|.
TEST_F(ThreadPoolJobTaskSourceTest, RunJoinTaskInParallel) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 2);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source.WillRunTask(),
            TaskSource::RunStatus::kAllowedNotSaturated);
  auto worker_task = registered_task_source.TakeTask();

  EXPECT_TRUE(task_source->WillJoin());
  EXPECT_TRUE(task_source->IsActive());

  std::move(worker_task.task).Run();
  EXPECT_FALSE(registered_task_source.DidProcessTask());

  EXPECT_FALSE(task_source->RunJoinTask());
  EXPECT_FALSE(task_source->IsActive());
}

// Verifies that a call to NotifyConcurrencyIncrease() calls the delegate
// and allows to run additional tasks.
TEST_F(ThreadPoolJobTaskSourceTest, NotifyConcurrencyIncrease) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      DoNothing(), /* num_tasks_to_run */ 1);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  auto registered_task_source_a =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_a.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  auto task_a = registered_task_source_a.TakeTask();
  EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
            TaskSource::RunStatus::kDisallowed);

  job_task->SetNumTasksToRun(2);
  EXPECT_CALL(pooled_task_runner_delegate_, EnqueueJobTaskSource(_)).Times(1);
  task_source->NotifyConcurrencyIncrease();

  auto registered_task_source_b =
      RegisteredTaskSource::CreateForTesting(task_source);
  // WillRunTask() should return a valid RunStatus because max concurrency was
  // increased to 2.
  EXPECT_EQ(registered_task_source_b.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  auto task_b = registered_task_source_b.TakeTask();
  EXPECT_EQ(RegisteredTaskSource::CreateForTesting(task_source).WillRunTask(),
            TaskSource::RunStatus::kDisallowed);

  std::move(task_a.task).Run();
  EXPECT_FALSE(registered_task_source_a.DidProcessTask());

  std::move(task_b.task).Run();
  EXPECT_FALSE(registered_task_source_b.DidProcessTask());
}

// Verifies that ShouldYield() calls the delegate.
TEST_F(ThreadPoolJobTaskSourceTest, ShouldYield) {
  auto job_task = base::MakeRefCounted<test::MockJobTask>(
      BindLambdaForTesting([](JobDelegate* delegate) {
        // As set up below, the mock will return false once and true the second
        // time.
        EXPECT_FALSE(delegate->ShouldYield());
        EXPECT_TRUE(delegate->ShouldYield());
      }),
      /* num_tasks_to_run */ 1);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(task_source);
  ASSERT_EQ(registered_task_source.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);

  auto task = registered_task_source.TakeTask();

  EXPECT_CALL(pooled_task_runner_delegate_, ShouldYield(_))
      .Times(2)
      .WillOnce(Return(false))
      .WillOnce(Return(true));

  std::move(task.task).Run();
  EXPECT_FALSE(registered_task_source.DidProcessTask());
}

// Verifies that max concurrency is allowed to stagnate when ShouldYield returns
// true.
TEST_F(ThreadPoolJobTaskSourceTest, MaxConcurrencyStagnateIfShouldYield) {
  scoped_refptr<JobTaskSource> task_source =
      base::MakeRefCounted<JobTaskSource>(
          FROM_HERE, TaskTraits(), BindRepeating([](JobDelegate* delegate) {
            // As set up below, the mock will return true once.
            ASSERT_TRUE(delegate->ShouldYield());
          }),
          BindRepeating([](size_t /*worker_count*/) -> size_t {
            return 1;  // max concurrency is always 1.
          }),
          &pooled_task_runner_delegate_);

  EXPECT_CALL(pooled_task_runner_delegate_, ShouldYield(_))
      .WillOnce(Return(true));

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(task_source);
  ASSERT_EQ(registered_task_source.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  auto task = registered_task_source.TakeTask();

  // Running the task should not fail even though max concurrency remained at 1,
  // since ShouldYield() returned true.
  std::move(task.task).Run();
  registered_task_source.DidProcessTask();
}

TEST_F(ThreadPoolJobTaskSourceTest, InvalidTakeTask) {
  auto job_task =
      base::MakeRefCounted<test::MockJobTask>(DoNothing(),
                                              /* num_tasks_to_run */ 1);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  auto registered_task_source_a =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_a.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);

  auto registered_task_source_b =
      RegisteredTaskSource::CreateForTesting(task_source);
  EXPECT_EQ(registered_task_source_b.WillRunTask(),
            TaskSource::RunStatus::kDisallowed);

  // Can not be called with an invalid RunStatus.
  EXPECT_DCHECK_DEATH({ auto task = registered_task_source_b.TakeTask(); });

  auto task = registered_task_source_a.TakeTask();
  registered_task_source_a.DidProcessTask();
}

TEST_F(ThreadPoolJobTaskSourceTest, InvalidDidProcessTask) {
  auto job_task =
      base::MakeRefCounted<test::MockJobTask>(DoNothing(),
                                              /* num_tasks_to_run */ 1);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(task_source);

  // Can not be called before WillRunTask().
  EXPECT_DCHECK_DEATH(registered_task_source.DidProcessTask());
}

TEST_F(ThreadPoolJobTaskSourceTest, AcquireTaskId) {
  auto job_task =
      base::MakeRefCounted<test::MockJobTask>(DoNothing(),
                                              /* num_tasks_to_run */ 4);
  scoped_refptr<JobTaskSource> task_source =
      job_task->GetJobTaskSource(FROM_HERE, {}, &pooled_task_runner_delegate_);

  EXPECT_EQ(0U, task_source->AcquireTaskId());
  EXPECT_EQ(1U, task_source->AcquireTaskId());
  EXPECT_EQ(2U, task_source->AcquireTaskId());
  EXPECT_EQ(3U, task_source->AcquireTaskId());
  EXPECT_EQ(4U, task_source->AcquireTaskId());
  task_source->ReleaseTaskId(1);
  task_source->ReleaseTaskId(3);
  EXPECT_EQ(1U, task_source->AcquireTaskId());
  EXPECT_EQ(3U, task_source->AcquireTaskId());
  EXPECT_EQ(5U, task_source->AcquireTaskId());
}

// Verifies that task id is released after worker_task returns.
TEST_F(ThreadPoolJobTaskSourceTest, GetTaskId) {
  auto task_source = MakeRefCounted<JobTaskSource>(
      FROM_HERE, TaskTraits{}, BindRepeating([](JobDelegate* delegate) {
        // Confirm that task id 0 is reused on the second run.
        EXPECT_EQ(0U, delegate->GetTaskId());

        // Allow running the task again.
        delegate->NotifyConcurrencyIncrease();
      }),
      BindRepeating([](size_t /*worker_count*/) -> size_t { return 1; }),
      &pooled_task_runner_delegate_);

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(task_source);

  // Run the worker_task twice.
  ASSERT_EQ(registered_task_source.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  auto task1 = registered_task_source.TakeTask();
  std::move(task1.task).Run();
  registered_task_source.DidProcessTask();

  ASSERT_EQ(registered_task_source.WillRunTask(),
            TaskSource::RunStatus::kAllowedSaturated);
  auto task2 = registered_task_source.TakeTask();
  std::move(task2.task).Run();
  registered_task_source.DidProcessTask();
}

}  // namespace internal
}  // namespace base
