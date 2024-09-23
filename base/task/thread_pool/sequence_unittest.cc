// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/sequence.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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

Task CreateTask(MockTask* mock_task, TimeTicks now = TimeTicks::Now()) {
  return Task(FROM_HERE, BindOnce(&MockTask::Run, Unretained(mock_task)), now,
              TimeDelta());
}

Task CreateDelayedTask(MockTask* mock_task,
                       TimeDelta delay,
                       TimeTicks now = TimeTicks::Now()) {
  return Task(FROM_HERE, BindOnce(&MockTask::Run, Unretained(mock_task)), now,
              delay);
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

  // Push task A in the sequence. PushImmediateTask() should return true since
  // it's the first task.
  EXPECT_TRUE(sequence_transaction.WillPushImmediateTask());
  sequence_transaction.PushImmediateTask(CreateTask(&mock_task_a));

  // Push task B, C and D in the sequence. PushImmediateTask() should return
  // false since there is already a task in a sequence.
  EXPECT_FALSE(sequence_transaction.WillPushImmediateTask());
  sequence_transaction.PushImmediateTask(CreateTask(&mock_task_b));
  EXPECT_FALSE(sequence_transaction.WillPushImmediateTask());
  sequence_transaction.PushImmediateTask(CreateTask(&mock_task_c));
  EXPECT_FALSE(sequence_transaction.WillPushImmediateTask());
  sequence_transaction.PushImmediateTask(CreateTask(&mock_task_d));

  // Take the task in front of the sequence. It should be task A.
  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(sequence);
  registered_task_source.WillRunTask();
  std::optional<Task> task =
      registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_a, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task B should now be in front.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));
  EXPECT_TRUE(registered_task_source.WillReEnqueue(TimeTicks::Now(),
                                                   &sequence_transaction));

  registered_task_source.WillRunTask();
  EXPECT_TRUE(sequence->has_worker_for_testing());
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_b, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task C should now be in front.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));
  EXPECT_TRUE(registered_task_source.WillReEnqueue(TimeTicks::Now(),
                                                   &sequence_transaction));

  registered_task_source.WillRunTask();
  EXPECT_TRUE(sequence->has_worker_for_testing());
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_c, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));
  EXPECT_TRUE(registered_task_source.WillReEnqueue(TimeTicks::Now(),
                                                   &sequence_transaction));
  EXPECT_FALSE(sequence->has_worker_for_testing());

  // Push task E in the sequence.
  EXPECT_FALSE(sequence_transaction.WillPushImmediateTask());
  sequence_transaction.PushImmediateTask(CreateTask(&mock_task_e));

  // Task D should be in front.
  registered_task_source.WillRunTask();
  EXPECT_TRUE(sequence->has_worker_for_testing());
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_d, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task E should now be in front.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));
  EXPECT_TRUE(registered_task_source.WillReEnqueue(TimeTicks::Now(),
                                                   &sequence_transaction));
  registered_task_source.WillRunTask();
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_e, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. The sequence should now be empty.
  EXPECT_FALSE(registered_task_source.DidProcessTask(&sequence_transaction));

  // Sequence is empty and it won't be returned to the priority queue.
  EXPECT_FALSE(sequence->has_worker_for_testing());
  EXPECT_FALSE(sequence->is_immediate_for_testing());
  EXPECT_TRUE(sequence->IsEmptyForTesting());
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
  best_effort_sequence_transaction.WillPushImmediateTask();
  best_effort_sequence_transaction.PushImmediateTask(
      std::move(best_effort_task));

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
  foreground_sequence_transaction.WillPushImmediateTask();
  foreground_sequence_transaction.PushImmediateTask(std::move(foreground_task));

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
  EXPECT_TRUE(sequence_transaction.WillPushImmediateTask());
  sequence_transaction.PushImmediateTask(
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
  sequence_transaction.WillPushImmediateTask();
  sequence_transaction.PushImmediateTask(
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

// Verify that the sequence stays in worker when new tasks are being pushed
// while it's being processed.
TEST(ThreadPoolSequenceTest, SequenceHasWorker) {
  testing::StrictMock<MockTask> mock_task_a;
  testing::StrictMock<MockTask> mock_task_b;

  scoped_refptr<Sequence> sequence =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::BEST_EFFORT), nullptr,
                               TaskSourceExecutionMode::kParallel);

  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());

  // Push task A in the sequence. WillPushImmediateTask() should return
  // true since sequence is empty.
  EXPECT_TRUE(sequence_transaction.WillPushImmediateTask());
  sequence_transaction.PushImmediateTask(CreateTask(&mock_task_a));

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(sequence);

  registered_task_source.WillRunTask();

  // WillRunTask indicates that a worker has called GetWork() and is ready to
  // run a task.
  EXPECT_TRUE(sequence->has_worker_for_testing());

  // The next task we get when we call Sequence::TakeTask should be Task A.
  std::optional<Task> task_a =
      registered_task_source.TakeTask(&sequence_transaction);

  // Push task B into the sequence. WillPushImmediateTask() should return false.
  EXPECT_FALSE(sequence_transaction.WillPushImmediateTask());
  sequence_transaction.PushImmediateTask(CreateTask(&mock_task_b));

  // Sequence is still being processed by a worker so pushing a new task
  // shouldn't change its location. We should expect it to still be in worker.
  EXPECT_TRUE(sequence->has_worker_for_testing());

  // Remove the empty slot. Sequence still has task B. This should return true.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));
  // Sequence can run immediately.
  EXPECT_TRUE(registered_task_source.WillReEnqueue(TimeTicks::Now(),
                                                   &sequence_transaction));

  // Sequence is not empty so it will be returned to the priority queue.
  EXPECT_FALSE(sequence->has_worker_for_testing());

  registered_task_source.WillRunTask();

  // The next task we get when we call Sequence::TakeTask should be Task B.
  std::optional<Task> task_b =
      registered_task_source.TakeTask(&sequence_transaction);

  // Remove the empty slot. Sequence is be empty. This should return false.
  EXPECT_FALSE(registered_task_source.DidProcessTask(&sequence_transaction));

  // Sequence is empty and it won't be returned to the priority queue.
  EXPECT_FALSE(sequence->has_worker_for_testing());
  EXPECT_FALSE(sequence->is_immediate_for_testing());
  EXPECT_TRUE(sequence->IsEmptyForTesting());
}

// Verify that the sequence handle delayed tasks.
TEST(ThreadPoolSequenceTest, PushTakeRemoveDelayedTasks) {
  TimeTicks now = TimeTicks::Now();

  testing::StrictMock<MockTask> mock_task_a;
  testing::StrictMock<MockTask> mock_task_b;
  testing::StrictMock<MockTask> mock_task_c;
  testing::StrictMock<MockTask> mock_task_d;

  scoped_refptr<Sequence> sequence =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::BEST_EFFORT), nullptr,
                               TaskSourceExecutionMode::kParallel);

  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());

  // Push task A in the sequence.
  auto delayed_task_a = CreateDelayedTask(&mock_task_a, Milliseconds(20), now);
  // PushDelayedTask(delayed_task_a, now) should return true since sequence is
  // empty.
  EXPECT_TRUE(sequence_transaction.PushDelayedTask(std::move(delayed_task_a)));

  // Push task B into the sequence.
  auto delayed_task_b = CreateDelayedTask(&mock_task_b, Milliseconds(10), now);
  // PushDelayedTask(...) should return true since task b runtime is earlier
  // than task a's.
  EXPECT_TRUE(sequence_transaction.PushDelayedTask(std::move(delayed_task_b)));

  // Sequence doesn't have immediate tasks.
  EXPECT_FALSE(sequence->is_immediate_for_testing());

  // Time advances by 15s.
  now += Milliseconds(15);

  // Set sequence to ready
  EXPECT_TRUE(sequence->OnBecomeReady());

  // Sequence is about to be run.
  EXPECT_TRUE(sequence->is_immediate_for_testing());

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(sequence);
  registered_task_source.WillRunTask();

  // Take the task in front of the sequence. It should be task B.
  std::optional<Task> task =
      registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_b, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task A should now be in front. Sequence is not empty
  // so this should return true.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));

  // Task A is still not ready so this should return false.
  EXPECT_FALSE(
      registered_task_source.WillReEnqueue(now, &sequence_transaction));
  EXPECT_FALSE(sequence->is_immediate_for_testing());

  // Push task C into the sequence.
  auto delayed_task_c = CreateDelayedTask(&mock_task_c, Milliseconds(1), now);
  // PushDelayedTask(...) should return true since task c runtime is
  // earlier than task a's.
  EXPECT_TRUE(sequence_transaction.PushDelayedTask(std::move(delayed_task_c)));

  // Push task D into the sequence.
  auto delayed_task_d = CreateDelayedTask(&mock_task_d, Milliseconds(1), now);
  // PushDelayedTask(...) should return false since task d queue time
  // is later than task c's.
  EXPECT_FALSE(sequence_transaction.PushDelayedTask(std::move(delayed_task_d)));

  // Time advances by 2ms.
  now += Milliseconds(2);
  // Set sequence to ready
  EXPECT_TRUE(registered_task_source->OnBecomeReady());

  registered_task_source.WillRunTask();

  // This should return task C
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_c, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task D should now be in front.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));

  // Task D is ready so this should return true.
  EXPECT_TRUE(registered_task_source.WillReEnqueue(now, &sequence_transaction));
  EXPECT_TRUE(sequence->is_immediate_for_testing());

  registered_task_source.WillRunTask();

  // This should return task D
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_d, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task A should now be in front.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));

  // Time advances by 10ms.
  now += Milliseconds(10);

  // Task A is ready so this should return true.
  EXPECT_TRUE(registered_task_source.WillReEnqueue(now, &sequence_transaction));
  EXPECT_TRUE(sequence->is_immediate_for_testing());

  registered_task_source.WillRunTask();

  // This should return task A since it's ripe
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_a, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Sequence should be empty now.
  EXPECT_FALSE(registered_task_source.DidProcessTask(&sequence_transaction));

  // Sequence is empty and it won't be returned to the priority queue.
  EXPECT_FALSE(sequence->has_worker_for_testing());
  EXPECT_FALSE(sequence->is_immediate_for_testing());
  EXPECT_TRUE(sequence->IsEmptyForTesting());
}

// Verify that the sequence handle delayed and immediate tasks.
TEST(ThreadPoolSequenceTest, PushTakeRemoveMixedTasks) {
  TimeTicks now = TimeTicks::Now();

  testing::StrictMock<MockTask> mock_task_a;
  testing::StrictMock<MockTask> mock_task_b;
  testing::StrictMock<MockTask> mock_task_c;
  testing::StrictMock<MockTask> mock_task_d;

  scoped_refptr<Sequence> sequence =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::BEST_EFFORT), nullptr,
                               TaskSourceExecutionMode::kParallel);

  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());

  // Starting with a delayed task
  // Push task A in the sequence.
  auto delayed_task_a = CreateDelayedTask(&mock_task_a, Milliseconds(20), now);
  // PushDelayedTask(delayed_task_a) should return
  // true since sequence is empty.
  EXPECT_TRUE(sequence_transaction.PushDelayedTask(std::move(delayed_task_a)));
  // Sequence doesn't have immediate tasks.
  EXPECT_FALSE(sequence->is_immediate_for_testing());

  // Push an immediate task while a delayed task is already sitting in the
  // delayed queue. This should prompt a move to the immediate queue.
  // Push task B in the sequence.
  auto task_b = CreateTask(&mock_task_b, now);
  // WillPushImmediateTask() should return true since sequence is in delayed
  // queue.
  EXPECT_TRUE(sequence_transaction.WillPushImmediateTask());
  sequence_transaction.PushImmediateTask(std::move(task_b));
  // Sequence now has an immediate tasks.
  EXPECT_TRUE(sequence->is_immediate_for_testing());

  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(sequence);

  // Prepare to run a task.
  registered_task_source.WillRunTask();
  EXPECT_TRUE(sequence->has_worker_for_testing());

  // Take the task in front of the sequence. It should be task B.
  std::optional<Task> task =
      registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_b, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task A should now be in front. Sequence is not empty
  // so this should return true.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));

  // Time advances by 21ms.
  now += Milliseconds(21);

  // Task A is ready so this should return true.
  EXPECT_TRUE(registered_task_source.WillReEnqueue(now, &sequence_transaction));
  EXPECT_TRUE(sequence->is_immediate_for_testing());

  registered_task_source.WillRunTask();
  EXPECT_TRUE(sequence->has_worker_for_testing());

  // Push a delayed task while sequence is being run by a worker. Push task C in
  // the sequence.
  auto delayed_task_c = CreateDelayedTask(&mock_task_c, Milliseconds(5), now);
  // PushDelayedTask(delayed_task_c) should return false since sequence is in
  // worker.
  EXPECT_FALSE(sequence_transaction.PushDelayedTask(std::move(delayed_task_c)));
  // Sequence is still in worker.
  EXPECT_TRUE(sequence->has_worker_for_testing());

  // This should return task A
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_a, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task C should now be in front.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));

  // Time advances by 2ms.
  now += Milliseconds(2);

  // Task C is not ready so this should return false.
  EXPECT_FALSE(
      registered_task_source.WillReEnqueue(now, &sequence_transaction));
  EXPECT_FALSE(sequence->is_immediate_for_testing());

  // Time advances by 4ms. Task C becomes ready.
  now += Milliseconds(4);

  // Set sequence to ready
  EXPECT_TRUE(registered_task_source->OnBecomeReady());
  EXPECT_TRUE(sequence->is_immediate_for_testing());

  // Push task D in the sequence while sequence is ready.
  auto task_d = CreateTask(&mock_task_d, now);
  // WillPushImmediateTask() should return false since sequence is already in
  // immediate queue.
  EXPECT_FALSE(sequence_transaction.WillPushImmediateTask());
  sequence_transaction.PushImmediateTask(std::move(task_d));
  EXPECT_TRUE(sequence->is_immediate_for_testing());

  registered_task_source.WillRunTask();
  EXPECT_TRUE(sequence->has_worker_for_testing());

  // This should return task C since was ready before Task D was posted.
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_c, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Task D should now be in front.
  EXPECT_TRUE(registered_task_source.DidProcessTask(&sequence_transaction));

  // Task D should be run so this should return true.
  EXPECT_TRUE(registered_task_source.WillReEnqueue(now, &sequence_transaction));
  EXPECT_TRUE(sequence->is_immediate_for_testing());

  registered_task_source.WillRunTask();

  // This should return task D since it's immediate.
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_d, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Remove the empty slot. Sequence should be empty.
  EXPECT_FALSE(registered_task_source.DidProcessTask(&sequence_transaction));
  EXPECT_FALSE(sequence->has_worker_for_testing());
  EXPECT_FALSE(sequence->is_immediate_for_testing());
}

// Test that PushDelayedTask method is used only for delayed tasks
TEST(ThreadPoolSequenceTest, TestPushDelayedTaskMethodUsage) {
  testing::StrictMock<MockTask> mock_task_a;

  scoped_refptr<Sequence> sequence =
      MakeRefCounted<Sequence>(TaskTraits(TaskPriority::BEST_EFFORT), nullptr,
                               TaskSourceExecutionMode::kParallel);

  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());

  // Push task B in the sequence.
  auto task_a = CreateTask(&mock_task_a);
  EXPECT_DCHECK_DEATH(
      { sequence_transaction.PushDelayedTask(std::move(task_a)); });
}

// Verifies the delayed sort key of a sequence that contains one delayed task.
// We will also test for the case where we push a delayed task with a runtime
// earlier than the queue_time of an already pushed immediate task.
TEST(ThreadPoolSequenceTest, GetDelayedSortKeyMixedtasks) {
  TimeTicks now = TimeTicks::Now();

  testing::StrictMock<MockTask> mock_task_a;
  testing::StrictMock<MockTask> mock_task_b;

  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(), nullptr, TaskSourceExecutionMode::kParallel);
  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());

  // Create a first delayed task.
  EXPECT_TRUE(sequence_transaction.PushDelayedTask(
      CreateDelayedTask(&mock_task_a, Milliseconds(10), now)));

  // Get the delayed sort key (first time).
  const TimeTicks sort_key_1 = sequence->GetDelayedSortKey();

  // Time advances by 11ms.
  now += Milliseconds(11);

  // Push an immediate task that should run after the delayed task.
  auto immediate_task = CreateTask(&mock_task_b, now);
  sequence_transaction.WillPushImmediateTask();
  sequence_transaction.PushImmediateTask(std::move(immediate_task));

  // Get the delayed sort key (second time).
  const TimeTicks sort_key_2 = sequence->GetDelayedSortKey();

  // Take the delayed task from the sequence, so that its next delayed runtime
  // is available for the check below.
  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(sequence);
  registered_task_source.WillRunTask();
  std::optional<Task> take_delayed_task =
      registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_a, &take_delayed_task.value());
  EXPECT_FALSE(take_delayed_task->queue_time.is_null());

  // For correctness.
  registered_task_source.DidProcessTask(&sequence_transaction);
  registered_task_source.WillReEnqueue(now, &sequence_transaction);

  // Verify that sort_key_1 is equal to the delayed task latest run time.
  EXPECT_EQ(take_delayed_task->latest_delayed_run_time(), sort_key_1);

  // Verify that the sort key didn't change after pushing the immediate task.
  EXPECT_EQ(sort_key_1, sort_key_2);

  // Get the delayed sort key (third time).
  const TimeTicks sort_key_3 = sequence->GetDelayedSortKey();

  // Take the immediate task from the sequence, so that its queue time
  // is available for the check below.
  registered_task_source.WillRunTask();
  std::optional<Task> take_immediate_task =
      registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_b, &take_immediate_task.value());
  EXPECT_FALSE(take_immediate_task->queue_time.is_null());

  // Verify that sort_key_1 is equal to the immediate task queue time.
  EXPECT_EQ(take_immediate_task->queue_time, sort_key_3);

  // DidProcessTask for correctness.
  registered_task_source.DidProcessTask(&sequence_transaction);
}

// Test for the case where we push a delayed task to run earlier than the
// already posted delayed task.
TEST(ThreadPoolSequenceTest, GetDelayedSortKeyDelayedtasks) {
  TimeTicks now = TimeTicks::Now();

  testing::StrictMock<MockTask> mock_task_a;
  testing::StrictMock<MockTask> mock_task_b;

  scoped_refptr<Sequence> sequence = MakeRefCounted<Sequence>(
      TaskTraits(), nullptr, TaskSourceExecutionMode::kParallel);
  Sequence::Transaction sequence_transaction(sequence->BeginTransaction());

  // Create a first delayed task.
  sequence_transaction.PushDelayedTask(
      CreateDelayedTask(&mock_task_a, Milliseconds(15), now));

  // Get the delayed sort key (first time).
  const TimeTicks sort_key_1 = sequence->GetDelayedSortKey();

  // Create a first delayed task.
  sequence_transaction.PushDelayedTask(
      CreateDelayedTask(&mock_task_b, Milliseconds(10), now));

  // Get the delayed sort key (second time).
  const TimeTicks sort_key_2 = sequence->GetDelayedSortKey();

  // Time advances by 11ms
  now += Milliseconds(11);

  sequence->OnBecomeReady();
  auto registered_task_source =
      RegisteredTaskSource::CreateForTesting(sequence);
  registered_task_source.WillRunTask();
  std::optional<Task> task =
      registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_b, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Verify that sort_key_2 is equal to the last posted task latest delayed run
  // time.
  EXPECT_EQ(task->latest_delayed_run_time(), sort_key_2);

  // Time advances by 5ms
  now += Milliseconds(5);

  // For correctness.
  registered_task_source.DidProcessTask(&sequence_transaction);
  registered_task_source.WillReEnqueue(now, &sequence_transaction);

  registered_task_source.WillRunTask();
  task = registered_task_source.TakeTask(&sequence_transaction);
  ExpectMockTask(&mock_task_a, &task.value());
  EXPECT_FALSE(task->queue_time.is_null());

  // Verify that sort_key_1 is equal to the first posted task latest delayed run
  // time.
  EXPECT_EQ(task->latest_delayed_run_time(), sort_key_1);

  // DidProcessTask for correctness.
  registered_task_source.DidProcessTask(&sequence_transaction);
}

}  // namespace internal
}  // namespace base
