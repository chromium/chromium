// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/task/sequence_manager/work_queue_sets.h"

#include <stddef.h>

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequence_manager/enqueue_order.h"
#include "base/task/sequence_manager/fence.h"
#include "base/task/sequence_manager/task_order.h"
#include "base/task/sequence_manager/task_queue.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
namespace sequence_manager {

class TimeDomain;

namespace internal {

namespace {

class MockObserver : public WorkQueueSets::Observer {
  MOCK_METHOD1(WorkQueueSetBecameEmpty, void(size_t set_index));
  MOCK_METHOD1(WorkQueueSetBecameNonEmpty, void(size_t set_index));
};

const TaskQueue::QueuePriority kHighestPriority = 0;
const TaskQueue::QueuePriority kDefaultPriority = 5;
const TaskQueue::QueuePriority kPriorityCount = 10;

}  // namespace

class WorkQueueSetsTest : public testing::Test {
 public:
  void SetUp() override {
    work_queue_sets_ = std::make_unique<WorkQueueSets>(
        "test", &mock_observer_,
        SequenceManager::Settings::Builder()
            .SetPrioritySettings(SequenceManager::PrioritySettings(
                kPriorityCount, kDefaultPriority))
            .Build());
  }

  void TearDown() override {
    for (std::unique_ptr<WorkQueue>& work_queue : work_queues_) {
      if (work_queue->work_queue_sets())
        work_queue_sets_->RemoveQueue(work_queue.get());
    }
  }

 protected:
  WorkQueue* NewTaskQueue(
      const char* queue_name,
      WorkQueue::QueueType queue_type = WorkQueue::QueueType::kImmediate) {
    WorkQueue* queue = new WorkQueue(nullptr, "test", queue_type);
    work_queues_.push_back(WrapUnique(queue));
    work_queue_sets_->AddQueue(queue, kHighestPriority);
    return queue;
  }

  Task FakeTaskWithEnqueueOrder(int enqueue_order) {
    Task fake_task(PostedTask(nullptr, BindOnce([] {}), FROM_HERE),
                   EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    return fake_task;
  }

  Task FakeNonNestableTaskWithEnqueueOrder(int enqueue_order) {
    Task fake_task(PostedTask(nullptr, BindOnce([] {}), FROM_HERE),
                   EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    fake_task.nestable = Nestable::kNonNestable;
    return fake_task;
  }

  Task FakeTaskWithTaskOrder(TaskOrder task_order) {
    Task fake_task(PostedTask(nullptr, BindOnce([] {}), FROM_HERE,
                              task_order.delayed_run_time(),
                              subtle::DelayPolicy::kFlexibleNoSooner),
                   EnqueueOrder::FromIntForTesting(task_order.sequence_num()),
                   task_order.enqueue_order(), TimeTicks() + Milliseconds(1));
    return fake_task;
  }

  WorkQueue* GetOldestQueueInSet(int set) const {
    if (auto queue_and_task_order =
            work_queue_sets_->GetOldestQueueAndTaskOrderInSet(set)) {
      return queue_and_task_order->queue;
    }
    return nullptr;
  }

  MockObserver mock_observer_;
  std::vector<std::unique_ptr<WorkQueue>> work_queues_;
  std::unique_ptr<WorkQueueSets> work_queue_sets_;
};

TEST_F(WorkQueueSetsTest, ChangeSetIndex) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  size_t set = kDefaultPriority;
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_EQ(set, work_queue->work_queue_set_index());
}

TEST_F(WorkQueueSetsTest, GetOldestQueueAndTaskOrderInSet_QueueEmpty) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  size_t set = kDefaultPriority;
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueAndTaskOrderInSet(set));
}

TEST_F(WorkQueueSetsTest, OnTaskPushedToEmptyQueue) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  size_t set = kDefaultPriority;
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_FALSE(work_queue_sets_->GetOldestQueueAndTaskOrderInSet(set));

  // Calls OnTaskPushedToEmptyQueue.
  work_queue->Push(FakeTaskWithEnqueueOrder(10));
  EXPECT_EQ(work_queue, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, GetOldestQueueAndTaskOrderInSet_SingleTaskInSet) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  work_queue->Push(FakeTaskWithEnqueueOrder(10));
  size_t set = 1;
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_EQ(work_queue, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, GetOldestQueueAndTaskOrderInSet_TaskOrder) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  work_queue->Push(FakeTaskWithEnqueueOrder(10));
  size_t set = 1;
  work_queue_sets_->ChangeSetIndex(work_queue, set);

  std::optional<WorkQueueAndTaskOrder> work_queue_and_task_order =
      work_queue_sets_->GetOldestQueueAndTaskOrderInSet(set);
  ASSERT_TRUE(work_queue_and_task_order);
  EXPECT_EQ(work_queue, work_queue_and_task_order->queue);
  EXPECT_EQ(10u, work_queue_and_task_order->order.enqueue_order());
}

TEST_F(WorkQueueSetsTest, GetOldestQueueAndTaskOrderInSet_MultipleAgesInSet) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue2");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 2;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);
  std::optional<WorkQueueAndTaskOrder> queue_and_order =
      work_queue_sets_->GetOldestQueueAndTaskOrderInSet(set);
  ASSERT_TRUE(queue_and_order);
  EXPECT_EQ(queue3, queue_and_order->queue);
  EXPECT_EQ(EnqueueOrder::FromIntForTesting(4),
            queue_and_order->order.enqueue_order());
}

TEST_F(WorkQueueSetsTest, OnQueuesFrontTaskChanged) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 4;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);
  EXPECT_EQ(queue3, GetOldestQueueInSet(set));

  // Make |queue1| now have a task with the lowest enqueue order.
  *const_cast<Task*>(queue1->GetFrontTask()) = FakeTaskWithEnqueueOrder(1);
  work_queue_sets_->OnQueuesFrontTaskChanged(queue1);
  EXPECT_EQ(queue1, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, OnQueuesFrontTaskChanged_OldestQueueBecomesEmpty) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 4;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);
  EXPECT_EQ(queue3, GetOldestQueueInSet(set));

  queue3->PopTaskForTesting();
  work_queue_sets_->OnQueuesFrontTaskChanged(queue3);
  EXPECT_EQ(queue2, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, OnQueuesFrontTaskChanged_YoungestQueueBecomesEmpty) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 4;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);
  EXPECT_EQ(queue3, GetOldestQueueInSet(set));

  queue1->PopTaskForTesting();
  work_queue_sets_->OnQueuesFrontTaskChanged(queue1);
  EXPECT_EQ(queue3, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, OnPopMinQueueInSet) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(1));
  queue2->Push(FakeTaskWithEnqueueOrder(3));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 3;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);
  EXPECT_EQ(queue2, GetOldestQueueInSet(set));

  queue2->PopTaskForTesting();
  work_queue_sets_->OnPopMinQueueInSet(queue2);
  EXPECT_EQ(queue2, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, OnPopMinQueueInSet_QueueBecomesEmpty) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 4;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);
  EXPECT_EQ(queue3, GetOldestQueueInSet(set));

  queue3->PopTaskForTesting();
  work_queue_sets_->OnPopMinQueueInSet(queue3);
  EXPECT_EQ(queue2, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest,
       GetOldestQueueAndTaskOrderInSet_MultipleAgesInSetIntegerRollover) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(0x7ffffff1));
  queue2->Push(FakeTaskWithEnqueueOrder(0x7ffffff0));
  queue3->Push(FakeTaskWithEnqueueOrder(-0x7ffffff1));
  size_t set = 1;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);
  EXPECT_EQ(queue2, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest,
       GetOldestQueueAndTaskOrderInSet_MultipleAgesInSet_RemoveQueue) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 1;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);
  work_queue_sets_->RemoveQueue(queue3);
  EXPECT_EQ(queue2, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, ChangeSetIndex_Complex) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  WorkQueue* queue4 = NewTaskQueue("queue4");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  queue4->Push(FakeTaskWithEnqueueOrder(3));
  size_t set1 = 1;
  size_t set2 = 2;
  work_queue_sets_->ChangeSetIndex(queue1, set1);
  work_queue_sets_->ChangeSetIndex(queue2, set1);
  work_queue_sets_->ChangeSetIndex(queue3, set2);
  work_queue_sets_->ChangeSetIndex(queue4, set2);
  EXPECT_EQ(queue2, GetOldestQueueInSet(set1));
  EXPECT_EQ(queue4, GetOldestQueueInSet(set2));

  work_queue_sets_->ChangeSetIndex(queue4, set1);
  EXPECT_EQ(queue4, GetOldestQueueInSet(set1));
  EXPECT_EQ(queue3, GetOldestQueueInSet(set2));
}

TEST_F(WorkQueueSetsTest, IsSetEmpty_NoWork) {
  size_t set = 2;
  EXPECT_TRUE(work_queue_sets_->IsSetEmpty(set));

  WorkQueue* work_queue = NewTaskQueue("queue");
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_TRUE(work_queue_sets_->IsSetEmpty(set));
}

TEST_F(WorkQueueSetsTest, IsSetEmpty_Work) {
  size_t set = 2;
  EXPECT_TRUE(work_queue_sets_->IsSetEmpty(set));

  WorkQueue* work_queue = NewTaskQueue("queue");
  work_queue->Push(FakeTaskWithEnqueueOrder(1));
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_FALSE(work_queue_sets_->IsSetEmpty(set));

  work_queue->PopTaskForTesting();
  work_queue_sets_->OnPopMinQueueInSet(work_queue);
  EXPECT_TRUE(work_queue_sets_->IsSetEmpty(set));
}

TEST_F(WorkQueueSetsTest, BlockQueuesByFence) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");

  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(7));
  queue1->Push(FakeTaskWithEnqueueOrder(8));
  queue2->Push(FakeTaskWithEnqueueOrder(9));

  size_t set = kHighestPriority;

  EXPECT_EQ(queue1, GetOldestQueueInSet(set));

  queue1->InsertFence(Fence::BlockingFence());
  EXPECT_EQ(queue2, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, PushNonNestableTaskToFront) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");
  queue1->Push(FakeTaskWithEnqueueOrder(6));
  queue2->Push(FakeTaskWithEnqueueOrder(5));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  size_t set = 4;
  work_queue_sets_->ChangeSetIndex(queue1, set);
  work_queue_sets_->ChangeSetIndex(queue2, set);
  work_queue_sets_->ChangeSetIndex(queue3, set);
  EXPECT_EQ(queue3, GetOldestQueueInSet(set));

  queue1->PushNonNestableTaskToFront(FakeNonNestableTaskWithEnqueueOrder(2));
  EXPECT_EQ(queue1, GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, CollectSkippedOverLowerPriorityTasks) {
  WorkQueue* queue1 = NewTaskQueue("queue1");
  WorkQueue* queue2 = NewTaskQueue("queue2");
  WorkQueue* queue3 = NewTaskQueue("queue3");

  work_queue_sets_->ChangeSetIndex(queue1, 3);
  work_queue_sets_->ChangeSetIndex(queue2, 2);
  work_queue_sets_->ChangeSetIndex(queue3, 1);

  queue1->Push(FakeTaskWithEnqueueOrder(1));
  queue1->Push(FakeTaskWithEnqueueOrder(2));
  queue2->Push(FakeTaskWithEnqueueOrder(3));
  queue3->Push(FakeTaskWithEnqueueOrder(4));
  queue3->Push(FakeTaskWithEnqueueOrder(5));
  queue2->Push(FakeTaskWithEnqueueOrder(6));
  queue1->Push(FakeTaskWithEnqueueOrder(7));

  std::vector<const Task*> result;
  work_queue_sets_->CollectSkippedOverLowerPriorityTasks(queue3, &result);

  ASSERT_EQ(3u, result.size());
  EXPECT_EQ(3u, result[0]->enqueue_order());  // The order here isn't important.
  EXPECT_EQ(1u, result[1]->enqueue_order());
  EXPECT_EQ(2u, result[2]->enqueue_order());
}

TEST_F(WorkQueueSetsTest, CompareDelayedTasksWithSameEnqueueOrder) {
  constexpr int kNumQueues = 3;

  WorkQueue* queues[kNumQueues] = {
      NewTaskQueue("queue0", WorkQueue::QueueType::kDelayed),
      NewTaskQueue("queue1", WorkQueue::QueueType::kDelayed),
      NewTaskQueue("queue2", WorkQueue::QueueType::kDelayed),
  };

  const EnqueueOrder kEnqueueOrder = EnqueueOrder::FromIntForTesting(5);
  TaskOrder task_orders[kNumQueues] = {
      TaskOrder::CreateForTesting(kEnqueueOrder, TimeTicks() + Seconds(1),
                                  /*sequence_num=*/4),
      TaskOrder::CreateForTesting(kEnqueueOrder, TimeTicks() + Seconds(2),
                                  /*sequence_num=*/3),
      TaskOrder::CreateForTesting(kEnqueueOrder, TimeTicks() + Seconds(3),
                                  /*sequence_num=*/2),
  };

  constexpr size_t kSet = kDefaultPriority;

  for (int i = 0; i < kNumQueues; i++) {
    queues[i]->Push(FakeTaskWithTaskOrder(task_orders[i]));
    work_queue_sets_->ChangeSetIndex(queues[i], kSet);
  }

  for (auto* queue : queues) {
    EXPECT_EQ(queue, GetOldestQueueInSet(kSet));
    queue->PopTaskForTesting();
    work_queue_sets_->OnQueuesFrontTaskChanged(queue);
  }
}

TEST_F(WorkQueueSetsTest, CompareDelayedTasksWithSameEnqueueOrderAndRunTime) {
  constexpr int kNumQueues = 3;

  WorkQueue* queues[kNumQueues] = {
      NewTaskQueue("queue0", WorkQueue::QueueType::kDelayed),
      NewTaskQueue("queue1", WorkQueue::QueueType::kDelayed),
      NewTaskQueue("queue2", WorkQueue::QueueType::kDelayed),
  };

  const EnqueueOrder kEnqueueOrder = EnqueueOrder::FromIntForTesting(5);
  constexpr TimeTicks delayed_run_time = TimeTicks() + Seconds(1);
  TaskOrder task_orders[kNumQueues] = {
      TaskOrder::CreateForTesting(kEnqueueOrder, delayed_run_time,
                                  /*sequence_num=*/2),
      TaskOrder::CreateForTesting(kEnqueueOrder, delayed_run_time,
                                  /*sequence_num=*/3),
      TaskOrder::CreateForTesting(kEnqueueOrder, delayed_run_time,
                                  /*sequence_num=*/4),
  };

  constexpr size_t kSet = kDefaultPriority;

  for (int i = 0; i < kNumQueues; i++) {
    queues[i]->Push(FakeTaskWithTaskOrder(task_orders[i]));
    work_queue_sets_->ChangeSetIndex(queues[i], kSet);
  }

  for (auto* queue : queues) {
    EXPECT_EQ(queue, GetOldestQueueInSet(kSet));
    queue->PopTaskForTesting();
    work_queue_sets_->OnQueuesFrontTaskChanged(queue);
  }
}

TEST_F(WorkQueueSetsTest, CompareDelayedAndImmediateTasks) {
  constexpr int kNumQueues = 5;
  WorkQueue* queues[kNumQueues] = {
      NewTaskQueue("queue0", WorkQueue::QueueType::kImmediate),
      NewTaskQueue("queue1", WorkQueue::QueueType::kDelayed),
      NewTaskQueue("queue2", WorkQueue::QueueType::kDelayed),
      NewTaskQueue("queue3", WorkQueue::QueueType::kDelayed),
      NewTaskQueue("queue4", WorkQueue::QueueType::kImmediate),
  };

  // TaskOrders in increasing order.
  TaskOrder task_orders[kNumQueues] = {
      // Immediate.
      TaskOrder::CreateForTesting(EnqueueOrder::FromIntForTesting(10),
                                  TimeTicks(),
                                  /*sequence_num=*/6),
      // Delayed.
      TaskOrder::CreateForTesting(EnqueueOrder::FromIntForTesting(11),
                                  TimeTicks() + Seconds(1),
                                  /*sequence_num=*/5),
      // Delayed, with the same enqueue order as the previous task.
      TaskOrder::CreateForTesting(EnqueueOrder::FromIntForTesting(11),
                                  TimeTicks() + Seconds(2),
                                  /*sequence_num=*/4),
      // Delayed, with the same delayed run time as the previous task but queued
      // in a subsequent wake-up.
      TaskOrder::CreateForTesting(EnqueueOrder::FromIntForTesting(12),
                                  TimeTicks() + Seconds(2),
                                  /*sequence_num=*/3),
      // Immediate.
      TaskOrder::CreateForTesting(EnqueueOrder::FromIntForTesting(13),
                                  TimeTicks(),
                                  /*sequence_num=*/2),
  };

  constexpr size_t kSet = kDefaultPriority;

  for (int i = kNumQueues - 1; i >= 0; i--) {
    queues[i]->Push(FakeTaskWithTaskOrder(task_orders[i]));
    work_queue_sets_->ChangeSetIndex(queues[i], kSet);
  }

  for (auto* queue : queues) {
    EXPECT_EQ(queue, GetOldestQueueInSet(kSet));
    queue->PopTaskForTesting();
    work_queue_sets_->OnQueuesFrontTaskChanged(queue);
  }
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
