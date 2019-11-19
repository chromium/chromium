// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/work_queue_sets.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequence_manager/work_queue.h"
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

}  // namespace

class WorkQueueSetsTest : public testing::Test {
 public:
  void SetUp() override {
    work_queue_sets_.reset(new WorkQueueSets("test", &mock_observer_,
                                             SequenceManager::Settings()));
  }

  void TearDown() override {
    for (std::unique_ptr<WorkQueue>& work_queue : work_queues_) {
      if (work_queue->work_queue_sets())
        work_queue_sets_->RemoveQueue(work_queue.get());
    }
  }

 protected:
  WorkQueue* NewTaskQueue(const char* queue_name) {
    WorkQueue* queue =
        new WorkQueue(nullptr, "test", WorkQueue::QueueType::kImmediate);
    work_queues_.push_back(WrapUnique(queue));
    work_queue_sets_->AddQueue(queue, TaskQueue::kControlPriority);
    return queue;
  }

  Task FakeTaskWithEnqueueOrder(int enqueue_order) {
    Task fake_task(PostedTask(nullptr, BindOnce([] {}), FROM_HERE), TimeTicks(),
                   EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    return fake_task;
  }

  Task FakeNonNestableTaskWithEnqueueOrder(int enqueue_order) {
    Task fake_task(PostedTask(nullptr, BindOnce([] {}), FROM_HERE), TimeTicks(),
                   EnqueueOrder(),
                   EnqueueOrder::FromIntForTesting(enqueue_order));
    fake_task.nestable = Nestable::kNonNestable;
    return fake_task;
  }

  MockObserver mock_observer_;
  std::vector<std::unique_ptr<WorkQueue>> work_queues_;
  std::unique_ptr<WorkQueueSets> work_queue_sets_;
};

TEST_F(WorkQueueSetsTest, ChangeSetIndex) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  size_t set = TaskQueue::kNormalPriority;
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_EQ(set, work_queue->work_queue_set_index());
}

TEST_F(WorkQueueSetsTest, GetOldestQueueInSet_QueueEmpty) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  size_t set = TaskQueue::kNormalPriority;
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, OnTaskPushedToEmptyQueue) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  size_t set = TaskQueue::kNormalPriority;
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_EQ(nullptr, work_queue_sets_->GetOldestQueueInSet(set));

  // Calls OnTaskPushedToEmptyQueue.
  work_queue->Push(FakeTaskWithEnqueueOrder(10));
  EXPECT_EQ(work_queue, work_queue_sets_->GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, GetOldestQueueInSet_SingleTaskInSet) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  work_queue->Push(FakeTaskWithEnqueueOrder(10));
  size_t set = 1;
  work_queue_sets_->ChangeSetIndex(work_queue, set);
  EXPECT_EQ(work_queue, work_queue_sets_->GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, GetOldestQueueAndEnqueueOrderInSet) {
  WorkQueue* work_queue = NewTaskQueue("queue");
  work_queue->Push(FakeTaskWithEnqueueOrder(10));
  size_t set = 1;
  work_queue_sets_->ChangeSetIndex(work_queue, set);

  EnqueueOrder enqueue_order;
  EXPECT_EQ(work_queue, work_queue_sets_->GetOldestQueueAndEnqueueOrderInSet(
                            set, &enqueue_order));
  EXPECT_EQ(10u, enqueue_order);
}

TEST_F(WorkQueueSetsTest, GetOldestQueueInSet_MultipleAgesInSet) {
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
  EXPECT_EQ(queue3, work_queue_sets_->GetOldestQueueInSet(set));
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
  EXPECT_EQ(queue3, work_queue_sets_->GetOldestQueueInSet(set));

  // Make |queue1| now have a task with the lowest enqueue order.
  *const_cast<Task*>(queue1->GetFrontTask()) = FakeTaskWithEnqueueOrder(1);
  work_queue_sets_->OnQueuesFrontTaskChanged(queue1);
  EXPECT_EQ(queue1, work_queue_sets_->GetOldestQueueInSet(set));
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
  EXPECT_EQ(queue3, work_queue_sets_->GetOldestQueueInSet(set));

  queue3->PopTaskForTesting();
  work_queue_sets_->OnQueuesFrontTaskChanged(queue3);
  EXPECT_EQ(queue2, work_queue_sets_->GetOldestQueueInSet(set));
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
  EXPECT_EQ(queue3, work_queue_sets_->GetOldestQueueInSet(set));

  queue1->PopTaskForTesting();
  work_queue_sets_->OnQueuesFrontTaskChanged(queue1);
  EXPECT_EQ(queue3, work_queue_sets_->GetOldestQueueInSet(set));
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
  EXPECT_EQ(queue2, work_queue_sets_->GetOldestQueueInSet(set));

  queue2->PopTaskForTesting();
  work_queue_sets_->OnPopMinQueueInSet(queue2);
  EXPECT_EQ(queue2, work_queue_sets_->GetOldestQueueInSet(set));
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
  EXPECT_EQ(queue3, work_queue_sets_->GetOldestQueueInSet(set));

  queue3->PopTaskForTesting();
  work_queue_sets_->OnPopMinQueueInSet(queue3);
  EXPECT_EQ(queue2, work_queue_sets_->GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest,
       GetOldestQueueInSet_MultipleAgesInSetIntegerRollover) {
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
  EXPECT_EQ(queue2, work_queue_sets_->GetOldestQueueInSet(set));
}

TEST_F(WorkQueueSetsTest, GetOldestQueueInSet_MultipleAgesInSet_RemoveQueue) {
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
  EXPECT_EQ(queue2, work_queue_sets_->GetOldestQueueInSet(set));
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
  EXPECT_EQ(queue2, work_queue_sets_->GetOldestQueueInSet(set1));
  EXPECT_EQ(queue4, work_queue_sets_->GetOldestQueueInSet(set2));

  work_queue_sets_->ChangeSetIndex(queue4, set1);
  EXPECT_EQ(queue4, work_queue_sets_->GetOldestQueueInSet(set1));
  EXPECT_EQ(queue3, work_queue_sets_->GetOldestQueueInSet(set2));
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

  size_t set = TaskQueue::kControlPriority;

  EXPECT_EQ(queue1, work_queue_sets_->GetOldestQueueInSet(set));

  queue1->InsertFence(EnqueueOrder::blocking_fence());
  EXPECT_EQ(queue2, work_queue_sets_->GetOldestQueueInSet(set));
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
  EXPECT_EQ(queue3, work_queue_sets_->GetOldestQueueInSet(set));

  queue1->PushNonNestableTaskToFront(FakeNonNestableTaskWithEnqueueOrder(2));
  EXPECT_EQ(queue1, work_queue_sets_->GetOldestQueueInSet(set));
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

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
