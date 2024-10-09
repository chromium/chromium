// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue.h"

#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_features.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace sequence_manager {
namespace internal {
// To avoid symbol collisions in jumbo builds.
namespace task_queue_unittest {
namespace {

TEST(TaskQueueTest, TaskQueueVoters) {
  auto sequence_manager = CreateSequenceManagerOnCurrentThreadWithPump(
      MessagePump::Create(MessagePumpType::DEFAULT));

  auto queue =
      sequence_manager->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));

  // The task queue should be initially enabled.
  EXPECT_TRUE(queue->IsQueueEnabled());

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter1 =
      queue->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter2 =
      queue->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter3 =
      queue->CreateQueueEnabledVoter();
  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter4 =
      queue->CreateQueueEnabledVoter();

  // Voters should initially vote for the queue to be enabled.
  EXPECT_TRUE(queue->IsQueueEnabled());

  // If any voter wants to disable, the queue is disabled.
  voter1->SetVoteToEnable(false);
  EXPECT_FALSE(queue->IsQueueEnabled());

  // If the voter is deleted then the queue should be re-enabled.
  voter1.reset();
  EXPECT_TRUE(queue->IsQueueEnabled());

  // If any of the remaining voters wants to disable, the queue should be
  // disabled.
  voter2->SetVoteToEnable(false);
  EXPECT_FALSE(queue->IsQueueEnabled());

  // If another queue votes to disable, nothing happens because it's already
  // disabled.
  voter3->SetVoteToEnable(false);
  EXPECT_FALSE(queue->IsQueueEnabled());

  // There are two votes to disable, so one of them voting to enable does
  // nothing.
  voter2->SetVoteToEnable(true);
  EXPECT_FALSE(queue->IsQueueEnabled());

  // IF all queues vote to enable then the queue is enabled.
  voter3->SetVoteToEnable(true);
  EXPECT_TRUE(queue->IsQueueEnabled());
}

TEST(TaskQueueTest, ShutdownQueueBeforeEnabledVoterDeleted) {
  auto sequence_manager = CreateSequenceManagerOnCurrentThreadWithPump(
      MessagePump::Create(MessagePumpType::DEFAULT));
  auto queue =
      sequence_manager->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();

  voter->SetVoteToEnable(true);  // NOP
  queue.reset();

  // This should complete without DCHECKing.
  voter.reset();
}

TEST(TaskQueueTest, ShutdownQueueBeforeDisabledVoterDeleted) {
  auto sequence_manager = CreateSequenceManagerOnCurrentThreadWithPump(
      MessagePump::Create(MessagePumpType::DEFAULT));
  auto queue =
      sequence_manager->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();

  voter->SetVoteToEnable(false);
  queue.reset();

  // This should complete without DCHECKing.
  voter.reset();
}

TEST(TaskQueueTest, CanceledTaskRemoved) {
  auto sequence_manager = CreateSequenceManagerOnCurrentThreadWithPump(
      MessagePump::Create(MessagePumpType::DEFAULT));
  auto queue =
      sequence_manager->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));

  // Get the default task runner.
  auto task_runner = queue->task_runner();
  EXPECT_EQ(queue->GetNumberOfPendingTasks(), 0u);

  bool task_ran = false;
  DelayedTaskHandle delayed_task_handle =
      task_runner->PostCancelableDelayedTask(
          subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
          BindLambdaForTesting([&task_ran] { task_ran = true; }), Seconds(20));
  EXPECT_EQ(queue->GetNumberOfPendingTasks(), 1u);

  // The task is only removed from the queue if the feature is enabled.
  delayed_task_handle.CancelTask();
  EXPECT_EQ(queue->GetNumberOfPendingTasks(), 0u);

  // In any case, the task never actually ran.
  EXPECT_FALSE(task_ran);
}

// Tests that a task posted through `PostCancelableDelayedTask()` is not
// considered canceled once it has reached the |delayed_work_queue| and is
// therefore not removed.
//
// This is a regression test for a bug in `Task::IsCanceled()` (see
// https://crbug.com/1288882). Note that this function is only called on tasks
// inside the |delayed_work_queue|, and not for tasks in the
// |delayed_incoming_queue|. This is because a task posted through
// `PostCancelableDelayedTask()` is always valid while it is in the
// |delayed_incoming_queue|, since canceling it would remove it from the queue.
TEST(TaskQueueTest, ValidCancelableTaskIsNotCanceled) {
  auto sequence_manager = CreateSequenceManagerOnCurrentThreadWithPump(
      MessagePump::Create(MessagePumpType::DEFAULT));
  auto queue =
      sequence_manager->CreateTaskQueue(TaskQueue::Spec(QueueName::TEST_TQ));

  // Get the default task runner.
  auto task_runner = queue->task_runner();
  EXPECT_EQ(queue->GetNumberOfPendingTasks(), 0u);

  // RunLoop requires the SingleThreadTaskRunner::CurrentDefaultHandle to be
  // set.
  SingleThreadTaskRunner::CurrentDefaultHandle
      single_thread_task_runner_current_default_handle(task_runner);
  RunLoop run_loop;

  // To reach the |delayed_work_queue|, the task must be posted with a non-
  // zero delay, which is then moved to the |delayed_work_queue| when it is
  // ripe. To achieve this, run the RunLoop for exactly the same delay of the
  // cancelable task. Since real time waiting happens, chose a very small delay.
  constexpr TimeDelta kTestDelay = Microseconds(1);
  task_runner->PostDelayedTask(FROM_HERE, run_loop.QuitClosure(), kTestDelay);

  DelayedTaskHandle delayed_task_handle =
      task_runner->PostCancelableDelayedTask(
          subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE, DoNothing(),
          kTestDelay);
  run_loop.Run();

  // Now only the cancelable delayed task remains and it is ripe.
  EXPECT_EQ(queue->GetNumberOfPendingTasks(), 1u);

  // ReclaimMemory doesn't remove the task because it is valid (not canceled).
  sequence_manager->ReclaimMemory();
  EXPECT_EQ(queue->GetNumberOfPendingTasks(), 1u);

  // Clean-up.
  delayed_task_handle.CancelTask();
}

}  // namespace
}  // namespace task_queue_unittest
}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
