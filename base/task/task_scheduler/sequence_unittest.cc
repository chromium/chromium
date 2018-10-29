// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_scheduler/sequence.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

class MockTask {
 public:
  MOCK_METHOD0(Run, void());
};

Task CreateTask(MockTask* mock_task) {
  return Task(FROM_HERE, BindOnce(&MockTask::Run, Unretained(mock_task)),
              TimeDelta());
}

void ExpectMockTask(MockTask* mock_task, Task* task) {
  EXPECT_CALL(*mock_task, Run());
  std::move(task->task).Run();
  testing::Mock::VerifyAndClear(mock_task);
}

}  // namespace

TEST(TaskSchedulerSequenceTest, PushTakeRemove) {
  testing::StrictMock<MockTask> mock_task_a;
  testing::StrictMock<MockTask> mock_task_b;
  testing::StrictMock<MockTask> mock_task_c;
  testing::StrictMock<MockTask> mock_task_d;
  testing::StrictMock<MockTask> mock_task_e;

  scoped_refptr<Sequence> sequence =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::BEST_EFFORT));

  // Push task A in the sequence. PushTask() should return true since it's the
  // first task->
  EXPECT_TRUE(sequence->PushTask(CreateTask(&mock_task_a)));

  // Push task B, C and D in the sequence. PushTask() should return false since
  // there is already a task in a sequence.
  EXPECT_FALSE(sequence->PushTask(CreateTask(&mock_task_b)));
  EXPECT_FALSE(sequence->PushTask(CreateTask(&mock_task_c)));
  EXPECT_FALSE(sequence->PushTask(CreateTask(&mock_task_d)));

  // Take the task in front of the sequence. It should be task A.
  Optional<Task> task = sequence->TakeTask();
  ExpectMockTask(&mock_task_a, &task.value());
  EXPECT_FALSE(task->sequenced_time.is_null());

  // Remove the empty slot. Task B should now be in front.
  EXPECT_FALSE(sequence->Pop());
  task = sequence->TakeTask();
  ExpectMockTask(&mock_task_b, &task.value());
  EXPECT_FALSE(task->sequenced_time.is_null());

  // Remove the empty slot. Task C should now be in front.
  EXPECT_FALSE(sequence->Pop());
  task = sequence->TakeTask();
  ExpectMockTask(&mock_task_c, &task.value());
  EXPECT_FALSE(task->sequenced_time.is_null());

  // Remove the empty slot.
  EXPECT_FALSE(sequence->Pop());

  // Push task E in the sequence.
  EXPECT_FALSE(sequence->PushTask(CreateTask(&mock_task_e)));

  // Task D should be in front.
  task = sequence->TakeTask();
  ExpectMockTask(&mock_task_d, &task.value());
  EXPECT_FALSE(task->sequenced_time.is_null());

  // Remove the empty slot. Task E should now be in front.
  EXPECT_FALSE(sequence->Pop());
  task = sequence->TakeTask();
  ExpectMockTask(&mock_task_e, &task.value());
  EXPECT_FALSE(task->sequenced_time.is_null());

  // Remove the empty slot. The sequence should now be empty.
  EXPECT_TRUE(sequence->Pop());
}

// Verifies the sort key of a BEST_EFFORT sequence that contains one task.
TEST(TaskSchedulerSequenceTest, GetSortKeyBestEffort) {
  // Create a BEST_EFFORT sequence with a task.
  Task best_effort_task(FROM_HERE, DoNothing(), TimeDelta());
  scoped_refptr<Sequence> best_effort_sequence =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::BEST_EFFORT));
  best_effort_sequence->PushTask(std::move(best_effort_task));

  // Get the sort key.
  const SequenceSortKey best_effort_sort_key =
      best_effort_sequence->GetSortKey();

  // Take the task from the sequence, so that its sequenced time is available
  // for the check below.
  auto take_best_effort_task = best_effort_sequence->TakeTask();

  // Verify the sort key.
  EXPECT_EQ(TaskPriority::BEST_EFFORT, best_effort_sort_key.priority());
  EXPECT_EQ(take_best_effort_task->sequenced_time,
            best_effort_sort_key.next_task_sequenced_time());

  // Pop for correctness.
  best_effort_sequence->Pop();
}

// Same as TaskSchedulerSequenceTest.GetSortKeyBestEffort, but with a
// USER_VISIBLE sequence.
TEST(TaskSchedulerSequenceTest, GetSortKeyForeground) {
  // Create a USER_VISIBLE sequence with a task.
  Task foreground_task(FROM_HERE, DoNothing(), TimeDelta());
  scoped_refptr<Sequence> foreground_sequence =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::USER_VISIBLE));
  foreground_sequence->PushTask(std::move(foreground_task));

  // Get the sort key.
  const SequenceSortKey foreground_sort_key = foreground_sequence->GetSortKey();

  // Take the task from the sequence, so that its sequenced time is available
  // for the check below.
  auto take_foreground_task = foreground_sequence->TakeTask();

  // Verify the sort key.
  EXPECT_EQ(TaskPriority::USER_VISIBLE, foreground_sort_key.priority());
  EXPECT_EQ(take_foreground_task->sequenced_time,
            foreground_sort_key.next_task_sequenced_time());

  // Pop for correctness.
  foreground_sequence->Pop();
}

// Verify that a DCHECK fires if Pop() is called on a sequence whose front slot
// isn't empty.
TEST(TaskSchedulerSequenceTest, PopNonEmptyFrontSlot) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(TaskTraits());
  sequence->PushTask(Task(FROM_HERE, DoNothing(), TimeDelta()));

  EXPECT_DCHECK_DEATH({ sequence->Pop(); });
}

// Verify that a DCHECK fires if TakeTask() is called on a sequence whose front
// slot is empty.
TEST(TaskSchedulerSequenceTest, TakeEmptyFrontSlot) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(TaskTraits());
  sequence->PushTask(Task(FROM_HERE, DoNothing(), TimeDelta()));

  EXPECT_TRUE(sequence->TakeTask());
  EXPECT_DCHECK_DEATH({ sequence->TakeTask(); });
}

// Verify that a DCHECK fires if TakeTask() is called on an empty sequence.
TEST(TaskSchedulerSequenceTest, TakeEmptySequence) {
  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(TaskTraits());
  EXPECT_DCHECK_DEATH({ sequence->TakeTask(); });
}

}  // namespace internal
}  // namespace base
