// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_queue.h"

#include <stddef.h>

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/task/common/lazy_now.h"
#include "base/task/sequence_manager/enqueue_order.h"
#include "base/task/sequence_manager/fence.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_order.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/work_queue_sets.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
namespace sequence_manager {
namespace internal {

namespace {

class MockObserver : public WorkQueueSets::Observer {
  MOCK_METHOD1(WorkQueueSetBecameEmpty, void(size_t set_index));
  MOCK_METHOD1(WorkQueueSetBecameNonEmpty, void(size_t set_index));
};

void NopTask() {}

struct Cancelable {
  Cancelable() = default;

  void NopTask() {}

  WeakPtrFactory<Cancelable> weak_ptr_factory{this};
};

}  // namespace

class WorkQueueTest : public testing::Test {
 public:
  WorkQueueTest() : WorkQueueTest(WorkQueue::QueueType::kImmediate) {}

  explicit WorkQueueTest(WorkQueue::QueueType queue_type)
      : queue_type_(queue_type) {}

  void SetUp() override {
    task_queue_ = std::make_unique<TaskQueueImpl>(
        /*sequence_manager=*/nullptr, /*wake_up_queue=*/nullptr,
        TaskQueue::Spec(QueueName::TEST_TQ));

    work_queue_ =
        std::make_unique<WorkQueue>(task_queue_.get(), "test", queue_type_);
    mock_observer_ = std::make_unique<MockObserver>();
    work_queue_sets_ = std::make_unique<WorkQueueSets>(
        "test", mock_observer_.get(), SequenceManager::Settings());
    work_queue_sets_->AddQueue(work_queue_.get(), 0);
  }

  void TearDown() override {
    work_queue_sets_->RemoveQueue(work_queue_.get());
    task_queue_->UnregisterTaskQueue();
  }

 protected:
  Task FakeCancelableTaskWithEnqueueOrder(int enqueue_order,
                                          WeakPtr<Cancelable> weak_ptr) {
    Task fake_task(PostedTask(nullptr, BindOnce(&Cancelable::NopTask, weak_ptr),
                              FROM_HERE),
                   EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    return fake_task;
  }

  Task FakeTaskWithEnqueueOrder(int enqueue_order) {
    Task fake_task(PostedTask(nullptr, BindOnce(&NopTask), FROM_HERE),
                   EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    return fake_task;
  }

  Task FakeNonNestableTaskWithEnqueueOrder(int enqueue_order) {
    Task fake_task(PostedTask(nullptr, BindOnce(&NopTask), FROM_HERE),
                   EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    fake_task.nestable = Nestable::kNonNestable;
    return fake_task;
  }

  Task FakeTaskWithTaskOrder(TaskOrder task_order) {
    Task fake_task(PostedTask(nullptr, BindOnce(&NopTask), FROM_HERE,
                              task_order.delayed_run_time(),
                              subtle::DelayPolicy::kFlexibleNoSooner),
                   EnqueueOrder::FromIntForTesting(task_order.sequence_num()),
                   task_order.enqueue_order(), TimeTicks() + Milliseconds(1));
    return fake_task;
  }

  Fence CreateFenceWithEnqueueOrder(int enqueue_order) {
    return Fence(TaskOrder::CreateForTesting(
        EnqueueOrder::FromIntForTesting(enqueue_order)));
  }

  WorkQueue* GetOldestQueueInSet(int set) {
    if (auto queue_and_task_order =
            work_queue_sets_->GetOldestQueueAndTaskOrderInSet(set)) {
      return queue_and_task_order->queue;
    }
    return nullptr;
  }

  std::unique_ptr<MockObserver> mock_observer_;
  std::unique_ptr<TaskQueueImpl> task_queue_;
  std::unique_ptr<WorkQueue> work_queue_;
  std::unique_ptr<WorkQueueSets> work_queue_sets_;
  std::unique_ptr<TaskQueueImpl::TaskDeque> incoming_queue_;

 private:
  const WorkQueue::QueueType queue_type_;
};

class DelayedWorkQueueTest : public WorkQueueTest {
 public:
  DelayedWorkQueueTest() : WorkQueueTest(WorkQueue::QueueType::kDelayed) {}
};

TEST_F(WorkQueueTest, Empty) {
  EXPECT_TRUE(work_queue_->Empty());
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->Empty());
}

TEST_F(WorkQueueTest, Empty_IgnoresFences) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  work_queue_->InsertFence(Fence::BlockingFence());
  EXPECT_FALSE(work_queue_->Empty());
}

TEST_F(WorkQueueTest, GetFrontTaskOrderQueueEmpty) {
  EXPECT_FALSE(work_queue_->GetFrontTaskOrder());
}

TEST_F(WorkQueueTest, GetFrontTaskOrder) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  std::optional<TaskOrder> task_order = work_queue_->GetFrontTaskOrder();
  EXPECT_TRUE(task_order);
  EXPECT_EQ(2ull, task_order->enqueue_order());
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
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, PushMultiple) {
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, PushAfterFenceHit) {
  work_queue_->InsertFence(Fence::BlockingFence());
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, CreateTaskPusherNothingPushed) {
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  { WorkQueue::TaskPusher task_pusher(work_queue_->CreateTaskPusher()); }
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, CreateTaskPusherOneTask) {
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  {
    WorkQueue::TaskPusher task_pusher(work_queue_->CreateTaskPusher());
    Task task = FakeTaskWithEnqueueOrder(2);
    task_pusher.Push(std::move(task));
  }
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, CreateTaskPusherThreeTasks) {
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  {
    WorkQueue::TaskPusher task_pusher(work_queue_->CreateTaskPusher());
    task_pusher.Push(FakeTaskWithEnqueueOrder(2));
    task_pusher.Push(FakeTaskWithEnqueueOrder(3));
    task_pusher.Push(FakeTaskWithEnqueueOrder(4));
  }
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, CreateTaskPusherAfterFenceHit) {
  work_queue_->InsertFence(Fence::BlockingFence());
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  {
    WorkQueue::TaskPusher task_pusher(work_queue_->CreateTaskPusher());
    task_pusher.Push(FakeTaskWithEnqueueOrder(2));
    task_pusher.Push(FakeTaskWithEnqueueOrder(3));
    task_pusher.Push(FakeTaskWithEnqueueOrder(4));
  }
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, PushNonNestableTaskToFront) {
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(3));
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(2));
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());
  EXPECT_EQ(3ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, PushNonNestableTaskToFrontAfterFenceHit) {
  work_queue_->InsertFence(Fence::BlockingFence());
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(2));
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, PushNonNestableTaskToFrontBeforeFenceHit) {
  work_queue_->InsertFence(CreateFenceWithEnqueueOrder(3));
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(2));
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, TakeImmediateIncomingQueueTasks) {
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(2));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(3));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(4));
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  EXPECT_TRUE(work_queue_->Empty());

  work_queue_->TakeImmediateIncomingQueueTasks();
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());

  ASSERT_NE(nullptr, work_queue_->GetFrontTask());
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());

  ASSERT_NE(nullptr, work_queue_->GetBackTask());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, TakeImmediateIncomingQueueTasksAfterFenceHit) {
  work_queue_->InsertFence(Fence::BlockingFence());
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(2));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(3));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(4));
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  EXPECT_TRUE(work_queue_->Empty());

  work_queue_->TakeImmediateIncomingQueueTasks();
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
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
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(3ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());

  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  EXPECT_TRUE(work_queue_->Empty());
}

TEST_F(WorkQueueTest, TakeTaskFromWorkQueue_HitFence) {
  work_queue_->InsertFence(CreateFenceWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, InsertFenceBeforeEnqueueing) {
  EXPECT_FALSE(work_queue_->InsertFence(Fence::BlockingFence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  EXPECT_FALSE(work_queue_->GetFrontTaskOrder());
}

TEST_F(WorkQueueTest, InsertFenceAfterEnqueueingNonBlocking) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  EXPECT_FALSE(work_queue_->InsertFence(CreateFenceWithEnqueueOrder(5)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->GetFrontTaskOrder());
  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
}

TEST_F(WorkQueueTest, InsertFenceAfterEnqueueing) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  // NB in reality a fence will always be greater than any currently enqueued
  // tasks.
  EXPECT_FALSE(work_queue_->InsertFence(Fence::BlockingFence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_FALSE(work_queue_->GetFrontTaskOrder());
}

TEST_F(WorkQueueTest, InsertNewFence) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  work_queue_->Push(FakeTaskWithEnqueueOrder(5));

  EXPECT_FALSE(work_queue_->InsertFence(CreateFenceWithEnqueueOrder(3)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  // Note until TakeTaskFromWorkQueue() is called we don't hit the fence.
  std::optional<TaskOrder> task_order = work_queue_->GetFrontTaskOrder();
  EXPECT_TRUE(task_order);
  EXPECT_EQ(2ull, task_order->enqueue_order());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_FALSE(work_queue_->GetFrontTaskOrder());
  EXPECT_TRUE(work_queue_->BlockedByFence());

  // Inserting the new fence should temporarily unblock the queue until the new
  // one is hit.
  EXPECT_TRUE(work_queue_->InsertFence(CreateFenceWithEnqueueOrder(6)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  task_order = work_queue_->GetFrontTaskOrder();
  EXPECT_TRUE(task_order);
  EXPECT_EQ(4ull, task_order->enqueue_order());
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->GetFrontTaskOrder());
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, PushWithNonEmptyQueueDoesNotHitFence) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(CreateFenceWithEnqueueOrder(2)));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, RemoveFence) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  work_queue_->Push(FakeTaskWithEnqueueOrder(5));
  work_queue_->InsertFence(CreateFenceWithEnqueueOrder(3));
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->RemoveFence());
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, RemoveFenceButNoFence) {
  EXPECT_FALSE(work_queue_->RemoveFence());
}

TEST_F(WorkQueueTest, RemoveFenceNothingUnblocked) {
  EXPECT_FALSE(work_queue_->InsertFence(Fence::BlockingFence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_FALSE(work_queue_->RemoveFence());
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFence) {
  EXPECT_FALSE(work_queue_->BlockedByFence());
  EXPECT_FALSE(work_queue_->InsertFence(Fence::BlockingFence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFencePopBecomesEmpty) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(CreateFenceWithEnqueueOrder(2)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(1ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFencePop) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(CreateFenceWithEnqueueOrder(2)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(1ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, InitiallyEmptyBlockedByFenceNewFenceUnblocks) {
  EXPECT_FALSE(work_queue_->InsertFence(Fence::BlockingFence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_TRUE(work_queue_->InsertFence(CreateFenceWithEnqueueOrder(3)));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, BlockedByFenceNewFenceUnblocks) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(1));
  EXPECT_FALSE(work_queue_->InsertFence(CreateFenceWithEnqueueOrder(2)));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(1ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->InsertFence(CreateFenceWithEnqueueOrder(4)));
  EXPECT_FALSE(work_queue_->BlockedByFence());
}

TEST_F(WorkQueueTest, InsertFenceAfterEnqueuing) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_FALSE(work_queue_->InsertFence(Fence::BlockingFence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_FALSE(work_queue_->GetFrontTaskOrder());
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

  std::optional<TaskOrder> task_order = work_queue_->GetFrontTaskOrder();
  EXPECT_TRUE(task_order);
  EXPECT_EQ(5ull, task_order->enqueue_order());
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

    std::optional<TaskOrder> task_order = work_queue_->GetFrontTaskOrder();
    EXPECT_TRUE(task_order);
    EXPECT_EQ(2ull, task_order->enqueue_order());
  }
}

TEST_F(WorkQueueTest, RemoveAllCanceledTasksFromFrontQueueBlockedByFence) {
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

  EXPECT_FALSE(work_queue_->InsertFence(Fence::BlockingFence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->RemoveAllCanceledTasksFromFront());

  EXPECT_FALSE(work_queue_->GetFrontTaskOrder());
}

TEST_F(WorkQueueTest, CollectTasksOlderThan) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  std::vector<const Task*> result;
  work_queue_->CollectTasksOlderThan(
      TaskOrder::CreateForTesting(EnqueueOrder::FromIntForTesting(4),
                                  TimeTicks(), 0),
      &result);

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(2u, result[0]->enqueue_order());
  EXPECT_EQ(3u, result[1]->enqueue_order());
}

TEST_F(DelayedWorkQueueTest, PushMultipleWithSameEnqueueOrder) {
  const EnqueueOrder kEnqueueOrder = EnqueueOrder::FromIntForTesting(5);
  TaskOrder task_orders[3] = {
      TaskOrder::CreateForTesting(kEnqueueOrder, TimeTicks() + Seconds(1),
                                  /*sequence_num=*/4),
      TaskOrder::CreateForTesting(kEnqueueOrder, TimeTicks() + Seconds(2),
                                  /*sequence_num=*/3),
      TaskOrder::CreateForTesting(kEnqueueOrder, TimeTicks() + Seconds(3),
                                  /*sequence_num=*/2),
  };

  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  for (auto& task_order : task_orders) {
    work_queue_->Push(FakeTaskWithTaskOrder(task_order));
  }

  EXPECT_TRUE(task_orders[0] == work_queue_->GetFrontTaskOrder());
  EXPECT_TRUE(task_orders[0] == work_queue_->GetFrontTask()->task_order());

  EXPECT_TRUE(task_orders[2] == work_queue_->GetBackTask()->task_order());
}

TEST_F(DelayedWorkQueueTest, DelayedFenceInDelayedTaskGroup) {
  const EnqueueOrder kEnqueueOrder = EnqueueOrder::FromIntForTesting(5);

  TaskOrder task_orders[3] = {
      TaskOrder::CreateForTesting(kEnqueueOrder, TimeTicks() + Seconds(1),
                                  /*sequence_num=*/4),
      TaskOrder::CreateForTesting(kEnqueueOrder, TimeTicks() + Seconds(2),
                                  /*sequence_num=*/3),
      TaskOrder::CreateForTesting(kEnqueueOrder, TimeTicks() + Seconds(3),
                                  /*sequence_num=*/2),
  };

  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  for (auto& task_order : task_orders) {
    work_queue_->Push(FakeTaskWithTaskOrder(task_order));
  }

  work_queue_->InsertFence(Fence(task_orders[2]));

  EXPECT_FALSE(work_queue_->BlockedByFence());
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(task_orders[0] ==
              work_queue_->TakeTaskFromWorkQueue().task_order());

  EXPECT_FALSE(work_queue_->BlockedByFence());
  EXPECT_EQ(work_queue_.get(), GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(task_orders[1] ==
              work_queue_->TakeTaskFromWorkQueue().task_order());

  EXPECT_TRUE(work_queue_->BlockedByFence());
  EXPECT_EQ(nullptr, GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
