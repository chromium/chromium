// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue.h"

#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/test/sequence_manager_for_test.h"
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

  auto queue = sequence_manager->CreateTaskQueue(TaskQueue::Spec("test"));

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
  auto queue = sequence_manager->CreateTaskQueue(TaskQueue::Spec("test"));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();

  voter->SetVoteToEnable(true);  // NOP
  queue->ShutdownTaskQueue();

  // This should complete without DCHECKing.
  voter.reset();
}

TEST(TaskQueueTest, ShutdownQueueBeforeDisabledVoterDeleted) {
  auto sequence_manager = CreateSequenceManagerOnCurrentThreadWithPump(
      MessagePump::Create(MessagePumpType::DEFAULT));
  auto queue = sequence_manager->CreateTaskQueue(TaskQueue::Spec("test"));

  std::unique_ptr<TaskQueue::QueueEnabledVoter> voter =
      queue->CreateQueueEnabledVoter();

  voter->SetVoteToEnable(false);
  queue->ShutdownTaskQueue();

  // This should complete without DCHECKing.
  voter.reset();
}

}  // namespace
}  // namespace task_queue_unittest
}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
