// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_queue.h"

#include <stddef.h>
#include <memory>

#include "base/bind.h"
#include "base/task/sequence_manager/lazy_now.h"
#include "base/task/sequence_manager/real_time_domain.h"
#include "base/task/sequence_manager/sequence_manager.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/work_queue_sets.h"
#include "base/time/default_tick_clock.h"
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
  Cancelable() {}

  void NopTask() {}

  WeakPtrFactory<Cancelable> weak_ptr_factory{this};
};

class RealTimeDomainFake : public RealTimeDomain {
 public:
  LazyNow CreateLazyNow() const override {
    return LazyNow(DefaultTickClock::GetInstance());
  }

  TimeTicks Now() const override { return TimeTicks::Now(); }
};

}  // namespace

class WorkQueueTest : public testing::Test {
 public:
  void SetUp() override {
    time_domain_ = std::make_unique<RealTimeDomainFake>();
    task_queue_ = std::make_unique<TaskQueueImpl>(/*sequence_manager=*/nullptr,
                                                  time_domain_.get(),
                                                  TaskQueue::Spec("test"));

    work_queue_ = std::make_unique<WorkQueue>(task_queue_.get(), "test",
                                              WorkQueue::QueueType::kImmediate);
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
                   TimeTicks(), EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    return fake_task;
  }

  Task FakeTaskWithEnqueueOrder(int enqueue_order) {
    Task fake_task(PostedTask(nullptr, BindOnce(&NopTask), FROM_HERE),
                   TimeTicks(), EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    return fake_task;
  }

  Task FakeNonNestableTaskWithEnqueueOrder(int enqueue_order) {
    Task fake_task(PostedTask(nullptr, BindOnce(&NopTask), FROM_HERE),
                   TimeTicks(), EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    fake_task.nestable = Nestable::kNonNestable;
    return fake_task;
  }

  std::unique_ptr<MockObserver> mock_observer_;
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
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, PushMultiple) {
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, PushAfterFenceHit) {
  work_queue_->InsertFence(EnqueueOrder::blocking_fence());
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));

  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, CreateTaskPusherNothingPushed) {
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
  { WorkQueue::TaskPusher task_pusher(work_queue_->CreateTaskPusher()); }
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, CreateTaskPusherOneTask) {
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
  {
    WorkQueue::TaskPusher task_pusher(work_queue_->CreateTaskPusher());
    Task task = FakeTaskWithEnqueueOrder(2);
    task_pusher.Push(&task);
  }
  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, CreateTaskPusherThreeTasks) {
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
  {
    WorkQueue::TaskPusher task_pusher(work_queue_->CreateTaskPusher());
    Task task1 = FakeTaskWithEnqueueOrder(2);
    Task task2 = FakeTaskWithEnqueueOrder(3);
    Task task3 = FakeTaskWithEnqueueOrder(4);
    task_pusher.Push(&task1);
    task_pusher.Push(&task2);
    task_pusher.Push(&task3);
  }
  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, CreateTaskPusherAfterFenceHit) {
  work_queue_->InsertFence(EnqueueOrder::blocking_fence());
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
  {
    WorkQueue::TaskPusher task_pusher(work_queue_->CreateTaskPusher());
    Task task1 = FakeTaskWithEnqueueOrder(2);
    Task task2 = FakeTaskWithEnqueueOrder(3);
    Task task3 = FakeTaskWithEnqueueOrder(4);
    task_pusher.Push(&task1);
    task_pusher.Push(&task2);
    task_pusher.Push(&task3);
  }
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, PushNonNestableTaskToFront) {
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(3));
  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(2));
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());
  EXPECT_EQ(3ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, PushNonNestableTaskToFrontAfterFenceHit) {
  work_queue_->InsertFence(EnqueueOrder::blocking_fence());
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(2));
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, PushNonNestableTaskToFrontBeforeFenceHit) {
  work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(3));
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));

  work_queue_->PushNonNestableTaskToFront(
      FakeNonNestableTaskWithEnqueueOrder(2));
  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));
}

TEST_F(WorkQueueTest, TakeImmediateIncomingQueueTasks) {
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(2));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(3));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(4));
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
  EXPECT_TRUE(work_queue_->Empty());

  work_queue_->TakeImmediateIncomingQueueTasks();
  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());

  ASSERT_NE(nullptr, work_queue_->GetFrontTask());
  EXPECT_EQ(2ull, work_queue_->GetFrontTask()->enqueue_order());

  ASSERT_NE(nullptr, work_queue_->GetBackTask());
  EXPECT_EQ(4ull, work_queue_->GetBackTask()->enqueue_order());
}

TEST_F(WorkQueueTest, TakeImmediateIncomingQueueTasksAfterFenceHit) {
  work_queue_->InsertFence(EnqueueOrder::blocking_fence());
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(2));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(3));
  task_queue_->PushImmediateIncomingTaskForTest(FakeTaskWithEnqueueOrder(4));
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
  EXPECT_TRUE(work_queue_->Empty());

  work_queue_->TakeImmediateIncomingQueueTasks();
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
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
  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(3ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());

  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
  EXPECT_TRUE(work_queue_->Empty());
}

TEST_F(WorkQueueTest, TakeTaskFromWorkQueue_HitFence) {
  work_queue_->InsertFence(EnqueueOrder::FromIntForTesting(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_FALSE(work_queue_->BlockedByFence());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
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
  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());

  EXPECT_EQ(2ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(0));
  EXPECT_FALSE(work_queue_->Empty());
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->RemoveFence());
  EXPECT_EQ(4ull, work_queue_->TakeTaskFromWorkQueue().enqueue_order());
  EXPECT_EQ(work_queue_.get(), work_queue_sets_->GetOldestQueueInSet(0));
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

  EXPECT_FALSE(work_queue_->InsertFence(EnqueueOrder::blocking_fence()));
  EXPECT_TRUE(work_queue_->BlockedByFence());

  EXPECT_TRUE(work_queue_->RemoveAllCanceledTasksFromFront());

  EnqueueOrder enqueue_order;
  EXPECT_FALSE(work_queue_->GetFrontTaskEnqueueOrder(&enqueue_order));
}

TEST_F(WorkQueueTest, CollectTasksOlderThan) {
  work_queue_->Push(FakeTaskWithEnqueueOrder(2));
  work_queue_->Push(FakeTaskWithEnqueueOrder(3));
  work_queue_->Push(FakeTaskWithEnqueueOrder(4));

  std::vector<const Task*> result;
  work_queue_->CollectTasksOlderThan(EnqueueOrder::FromIntForTesting(4),
                                     &result);

  ASSERT_EQ(2u, result.size());
  EXPECT_EQ(2u, result[0]->enqueue_order());
  EXPECT_EQ(3u, result[1]->enqueue_order());
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
