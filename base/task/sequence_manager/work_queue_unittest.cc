// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_queue.h"

#include <stddef.h>
#include <memory>

#include "base/bind.h"
#include "base/task/sequence_manager/real_time_domain.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/work_queue_sets.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
namespace sequence_manager {
namespace internal {

namespace {

void NopTask() {}

struct Cancelable {
  Cancelable() : weak_ptr_factory(this) {}

  void NopTask() {}

  WeakPtrFactory<Cancelable> weak_ptr_factory;
};

}  // namespace

class WorkQueueTest : public testing::Test {
 public:
  void SetUp() override {
    dummy_sequence_manager_ = SequenceManagerImpl::CreateUnbound(nullptr);
    time_domain_.reset(new RealTimeDomain());
    task_queue_ = std::make_unique<TaskQueueImpl>(dummy_sequence_manager_.get(),
                                                  time_domain_.get(),
                                                  TaskQueue::Spec("test"));

    work_queue_.reset(new WorkQueue(task_queue_.get(), "test",
                                    WorkQueue::QueueType::kImmediate));
    work_queue_sets_.reset(new WorkQueueSets(1, "test"));
    work_queue_sets_->AddQueue(work_queue_.get(), 0);
  }

  void TearDown() override {
    work_queue_sets_->RemoveQueue(work_queue_.get());

    task_queue_->ClearSequenceManagerForTesting();
  }

 protected:
  Task FakeCancelableTaskWithEnqueueOrder(int enqueue_order,
                                          WeakPtr<Cancelable> weak_ptr) {
    Task fake_task(
        PostedTask(BindOnce(&Cancelable::NopTask, weak_ptr), FROM_HERE),
        TimeTicks(), EnqueueOrder(),
        EnqueueOrder::FromIntForTesting(enqueue_order));
    return fake_task;
  }

  Task FakeTaskWithEnqueueOrder(int enqueue_order) {
    Task fake_task(PostedTask(BindOnce(&NopTask), FROM_HERE), TimeTicks(),
                   EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    return fake_task;
  }

  Task FakeNonNestableTaskWithEnqueueOrder(int enqueue_order) {
    Task fake_task(PostedTask(BindOnce(&NopTask), FROM_HERE), TimeTicks(),
                   EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    fake_task.nestable = Nestable::kNonNestable;
    return fake_task;
  }

  std::unique_ptr<SequenceManagerImpl> dummy_sequence_manager_;
  std::unique_ptr<RealTimeDomain> time_domain_;
  std::unique_ptr<TaskQueueImpl> task_queue_;
  std::unique_ptr<WorkQueue> work_queue_;
  std::unique_ptr<WorkQueueSets> work_queue_sets_;
  std::unique_ptr<TaskQueueImpl::TaskDeque> incoming_queue_;
};

TEST_F(WorkQueueTest, Empty) {
  EXPECT_TRUE(work_queue_->Empty());
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->Empty());
}

TEST_F(WorkQueueTest, Empty_IgnoresFences) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  work_queue_->InsertFence(EnqueueOrder::blocking_fence());
  EXPECT_FALSE(work_queue_->Empty());
}

TEST_F(WorkQueueTest, GetFrontTaskEnqueueOrderQueueEmpty) {
  EnqueueOrder enqueue_order;
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
}

TEST_F(WorkQueueTest, GetFrontTaskEnqueueOrder) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  EnqueueOrder enqueue_order;
  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_EQ(2ull, enqueue_order);
}

TEST_F(WorkQueueTest, GetFrontTaskQueueEmpty) {
  EXPECT_EQ(nullptr, work_queue_->GetFrontTask());
}

TEST_F(WorkQueueTest, GetFrontTask) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  ASSERT_NE(nullptr, work_queue_->GetFrontTask());
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());
}

TEST_F(WorkQueueTest, GetBackTask_Empty) {
  EXPECT_EQ(nullptr, work_queue_->GetBackTask());
}

TEST_F(WorkQueueTest, GetBackTask) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  ASSERT_NE(nullptr, work_queue_->GetBackTask());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, Push) {
  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_EQ(work_queue_.get(), work_queue);
}

TEST_F(WorkQueueTest, PushAfterFenceHit) {
  work_queue_->InsertFence(EnqueueOrder::blocking_fence());
  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
}

TEST_F(WorkQueueTest, PushNonNestableTaskToFront) {
  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(3));
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_EQ(work_queue_.get(), work_queue);

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(2));

  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());
  EXPECT_EQ(3ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, PushNonNestableTaskToFrontAfterFenceHit) {
  work_queue_->InsertFence(EnqueueOrder::blocking_fence());
  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(2));
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
}

TEST_F(WorkQueueTest, PushNonNestableTaskToFrontBeforeFenceHit) {
  work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(3));
  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(2));
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
}

TEST_F(WorkQueueTest, ReloadEmptyImmediateQueue) {
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(2));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(3));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(4));

  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_TRUE(work_queue_->Empty());
  work_queue_->ReloadEmptyImmediateQueue();

  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());

  ASSERT_NE(nullptr, work_queue_->GetFrontTask());
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());

  ASSERT_NE(nullptr, work_queue_->GetBackTask());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, ReloadEmptyImmediateQueueAfterFenceHit) {
  work_queue_->InsertFence(EnqueueOrder::blocking_fence());
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(2));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(3));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(4));

  WorkQueue* work_queue;
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_TRUE(work_queue_->Empty());
  work_queue_->ReloadEmptyImmediateQueue();

  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());

  ASSERT_NE(nullptr, work_queue_->GetFrontTask());
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());

  ASSERT_NE(nullptr, work_queue_->GetBackTask());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, TakeTaskFromWorkQueue) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  WorkQueue* work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(3ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());

  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_TRUE(work_queue_->Empty());
}

TEST_F(WorkQueueTest, TakeTaskFromWorkQueue_HitFence) {
  work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  WorkQueue* work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, InsertFenceBeforeEnqueueing) {
  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::blocking_fence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  EnqueueOrder enqueue_order;
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
}

TEST_F(WorkQueueTest, InsertFenceAfterEnqueueingNonBlocking) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(5)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EnqueueOrder enqueue_order;
  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
}

TEST_F(WorkQueueTest, InsertFenceAfterEnqueueing) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  // NB in reality a fence will always be greater than any currently enqueued
  // tasks.
  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::blocking_fence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EnqueueOrder enqueue_order;
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
}

TEST_F(WorkQueueTest, InsertNewFence) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  work_queue_->Push(FakeTaskWithEnqueueOrder(5));

  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(3)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  // Note until TakeTaskFromWorkQueue() is called we don't hit the fence.
  EnqueueOrder enqueue_order;
  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_EQ(2ull, enqueue_order);

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  // Inserting the new fence should temporarily unblock the queue until the new
  // one is hit.
  EXPECT_TRUE(work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(6)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_EQ(4ull, enqueue_order);
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, PushWithNonEmptyQueueDoesNotHitFence) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(2)));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, RemoveFence) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  work_queue_->Push(FakeTaskWithEnqueueOrder(5));
  work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(3));

  WorkQueue* work_queue;
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->RemoveFence());
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_sets_->GetOldestQueueInSet(0, &work_queue));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, RemoveFenceButNoFence) {
  EXPECT_FALSE(work_queue_->RemoveFence());
}

TEST_F(WorkQueueTest, RemoveFenceNothingUnblocked) {
  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::blocking_fence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_FALSE(work_queue_->RemoveFence());
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFence) {
  EXPECT_FALSE(work_queue_->BlockedByFence());
  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::blocking_fence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFencePopBecomesEmpty) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(2)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(1ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFencePop) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(2)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(1ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, InitiallyEmptyBlockedByFenceNewFenceUnblocks) {
  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::blocking_fence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_TRUE(work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(3)));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFenceNewFenceUnblocks) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(2)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(1ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(4)));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, InsertFenceAfterEnqueuing) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::blocking_fence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EnqueueOrder enqueue_order;
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
}

TEST_F(WorkQueueTest, RemoveAllCanceledTasksFromFront) {
  {
    Cancelable cancelable;
    work_queue_->Push(FakeCancelableTaskWithEnqueueOrder(
        2, cancelable.weak_ptr_factory.GetWeakPtr()));
    work_queue_->Push(FakeCancelableTaskWithEnqueueOrder(
        3, cancelable.weak_ptr_factory.GetWeakPtr()));
    work_queue_->Push(FakeCancelableTaskWithEnqueueOrder(
        4, cancelable.weak_ptr_factory.GetWeakPtr()));
    work_queue_->Push(FakeTaskWithEnqueueOrder(5));
  }
  EXPECT_TRUE(work_queue_->RemoveAllCanceledTasksFromFront());

  EnqueueOrder enqueue_order;
  EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
  EXPECT_EQ(5ull, enqueue_order);
}

TEST_F(WorkQueueTest, RemoveAllCanceledTasksFromFrontTasksNotCanceled) {
  {
    Cancelable cancelable;
    work_queue_->Push(FakeCancelableTaskWithEnqueueOrder(
        2, cancelable.weak_ptr_factory.GetWeakPtr()));
    work_queue_->Push(FakeCancelableTaskWithEnqueueOrder(
        3, cancelable.weak_ptr_factory.GetWeakPtr()));
    work_queue_->Push(FakeCancelableTaskWithEnqueueOrder(
        4, cancelable.weak_ptr_factory.GetWeakPtr()));
    work_queue_->Push(FakeTaskWithEnqueueOrder(5));
    EXPECT_FALSE(work_queue_->RemoveAllCanceledTasksFromFront());

    EnqueueOrder enqueue_order;
    EXPECT_TRUE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
    EXPECT_EQ(2ull, enqueue_order);
  }
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
