// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/sequence.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace internal {

namespace {

class MockTask {
 public:
  MOCK_METHOD0(Run, void());
};

Task CreateTask(MockTask* mock_task) {
  return Task(FROM_HERE, BindOnce(&MockTask::Run, Unretained(mock_task)),
              TimeTicks::Now(), TimeDelta());
}

void ExpectMockTask(MockTask* mock_task, Task* task) {
  EXPECT_CALL(*mock_task, Run());
  std::move(task->task).Run();
  testing::Mock::VerifyAndClear(mock_task);
}

}  // namespace

TEST(ThreadPoolSequenceTest, PushTakeRemove) {
  testing::StrictMock<MockTask> mock_task_a;
  testing::StrictMock<MockTask> mock_task_b;
  testing::StrictMock<MockTask> mock_task_c;
  testing::StrictMock<MockTask> mock_task_d;
  testing::StrictMock<MockTask> mock_task_e;

  scoped_refptr<Sequence> sequence =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::BEST_EFFORT), nullptr,
                               TaskSourceExecutionMode::kParallel);
  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());

  // Push task A in the sequence. PushTask() should return true since it's the
  // first task->
  EXPECT_TRUE(sequence_transaction.WillPushTask());
  sequence_transaction.PushTask(CreateTask(&mock_task_a));

  // Push task B, C and D in the sequence. PushTask() should return false
  // since there is already a task in a sequence.
  EXPECT_FALSE(sequence_transaction.WillPushTask());
  sequence_transaction.PushTask(CreateTask(&mock_task_b));
  EXPECT_FALSE(sequence_transaction.WillPushTask());
  sequence_transaction.PushTask(CreateTask(&mock_task_c));
  EXPECT_FALSE(sequence_transaction.WillPushTask());
  sequence_transaction.PushTask(CreateTask(&mock_task_d));

  // Take the task in front of the sequence. It should be task A.
  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(sequence);
  registered_task_source.WillRunTask();
  absl::optional<Task> task =
      registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_a, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task B should now be in front.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));

  EXPECT_FALSE(sequence_transaction.WillPushTask());
  registered_task_source.WillRunTask();
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_b, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task C should now be in front.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));

  EXPECT_FALSE(sequence_transaction.WillPushTask());
  registered_task_source.WillRunTask();
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_c, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));

  // Push task E in the sequence.
  EXPECT_FALSE(sequence_transaction.WillPushTask());
  sequence_transaction.PushTask(CreateTask(&mock_task_e));

  // Task D should be in front.
  registered_task_source.WillRunTask();
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_d, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task E should now be in front.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));
  EXPECT_FALSE(sequence_transaction.WillPushTask());
  registered_task_source.WillRunTask();
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_e, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. The sequence should now be empty.
  EXPECT_FALSE(registered_task_source.DidProcessTask(&sequence_transaction));
  EXPECT_TRUE(sequence_transaction.WillPushTask());
}

// Verifies the sort key of a BEST_EFFORT sequence that contains one task.
TEST(ThreadPoolSequenceTest, GetSortKeyBestEffort) {
  // Create a BEST_EFFORT sequence with a task.
  Task best_effort_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  scoped_refptr<Sequence> best_effort_sequence =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::BEST_EFFORT), nullptr,
                               TaskSourceExecutionMode::kParallel);
  Sequence::Transaction best_effort_sequence_transaction(
      best_effort_sequence->BeginTransaction());
  best_effort_sequence_transaction.PushTask(std::move(best_effort_task));

  // Get the sort key.
  const TaskSourceSortKey best_effort_sort_key =
      best_effort_sequence->GetSortKey();

  // Take the task from the sequence, so that its sequenced time is available
  // for the check below.
  auto best_effort_registered_task_source =
      RegisteredTaskSource::CreateForTesting(best_effort_sequence);
  best_effort_registered_task_source.WillRunTask();
  auto take_best_effort_task = best_effort_registered_task_source.TakeTask(
      &best_effort_sequence_transaction);

  // Verify the sort key.
  EXPECT_EQ(TaskPriority::BEST_EFFORT, best_effort_sort_key.priority());
  EXPECT_EQ(take_best_effort_task.queue_time,
            best_effort_sort_key.ready_time());

  // DidProcessTask for correctness.
  best_effort_registered_task_source.DidProcessTask(
      &best_effort_sequence_transaction);
}

// Same as ThreadPoolSequenceTest.GetSortKeyBestEffort, but with a
// USER_VISIBLE sequence.
TEST(ThreadPoolSequenceTest, GetSortKeyForeground) {
  // Create a USER_VISIBLE sequence with a task.
  Task foreground_task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta());
  scoped_refptr<Sequence> foreground_sequence =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::USER_VISIBLE), nullptr,
                               TaskSourceExecutionMode::kParallel);
  Sequence::Transaction foreground_sequence_transaction(
      foreground_sequence->BeginTransaction());
  foreground_sequence_transaction.PushTask(std::move(foreground_task));

  // Get the sort key.
  const TaskSourceSortKey foreground_sort_key =
      foreground_sequence->GetSortKey();

  // Take the task from the sequence, so that its sequenced time is available
  // for the check below.
  auto foreground_registered_task_source =
      RegisteredTaskSource::CreateForTesting(foreground_sequence);
  foreground_registered_task_source.WillRunTask();
  auto take_foreground_task = foreground_registered_task_source.TakeTask(
      &foreground_sequence_transaction);

  // Verify the sort key.
  EXPECT_EQ(TaskPriority::USER_VISIBLE, foreground_sort_key.priority());
  EXPECT_EQ(take_foreground_task.queue_time, foreground_sort_key.ready_time());

  // DidProcessTask for correctness.
  foreground_registered_task_source.DidProcessTask(
      &foreground_sequence_transaction);
}

// Verify that a DCHECK fires if DidProcessTask() is called on a sequence which
// didn't return a Task.
TEST(ThreadPoolSequenceTest, DidProcessTaskWithoutWillRunTask) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(), nullptr, TaskSourceExecutionMode::kParallel);
  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());
  sequence_transaction.PushTask(
      Task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta()));

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(sequence);
  EXPECT_DCHECK_DEATH({
    registered_task_source.DidProcessTask(&sequence_transaction);
  });
}

// Verify that a DCHECK fires if TakeTask() is called on a sequence whose front
// slot is empty.
TEST(ThreadPoolSequenceTest, TakeEmptyFrontSlot) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(), nullptr, TaskSourceExecutionMode::kParallel);
  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());
  sequence_transaction.PushTask(
      Task(FROM_HERE, DoNothing(), TimeTicks::Now(), TimeDelta()));

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(sequence);
  {
    registered_task_source.WillRunTask();
    IgnoreResult(registered_task_source.TakeTask(&sequence_transaction));
    registered_task_source.DidProcessTask(&sequence_transaction);
  }
  EXPECT_DCHECK_DEATH({
    registered_task_source.WillRunTask();
    auto task = registered_task_source.TakeTask(&sequence_transaction);
  });
}

// Verify that a DCHECK fires if TakeTask() is called on an empty sequence.
TEST(ThreadPoolSequenceTest, TakeEmptySequence) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(), nullptr, TaskSourceExecutionMode::kParallel);
  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(sequence);
  EXPECT_DCHECK_DEATH({
    registered_task_source.WillRunTask();
    auto task = registered_task_source.TakeTask();
  });
}

}  // namespace internal
}  // namespace base
