// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_queue_selector.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/pending_task.h"
#include "base/task/sequence_manager/enqueue_order_generator.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/test/mock_time_domain.h"
#include "base/task/sequence_manager/work_queue.h"
#include "base/task/sequence_manager/work_queue_sets.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::NotNull;

namespace base {
namespace sequence_manager {
namespace internal {
// To avoid symbol collisions in jumbo builds.
namespace task_queue_selector_unittest {

class MockObserver : public TaskQueueSelector::Observer {
 public:
  MockObserver() = default;
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  MOCK_METHOD1(OnTaskQueueEnabled, void(internal::TaskQueueImpl*));
};

class TaskQueueSelectorForTest : public TaskQueueSelector {
 public:
  using TaskQueueSelector::ActivePriorityTracker;
  using TaskQueueSelector::ChooseWithPriority;
  using TaskQueueSelector::delayed_work_queue_sets;
  using TaskQueueSelector::immediate_work_queue_sets;
  using TaskQueueSelector::SetImmediateStarvationCountForTest;
  using TaskQueueSelector::SetOperationOldest;

  explicit TaskQueueSelectorForTest(
      scoped_refptr<AssociatedThreadId> associated_thread)
      : TaskQueueSelector(associated_thread, SequenceManager::Settings()) {}
};

class TaskQueueSelectorTest : public testing::Test {
 public:
  TaskQueueSelectorTest()
      : test_closure_(BindRepeating(&TaskQueueSelectorTest::TestFunction)),
        associated_thread_(AssociatedThreadId::CreateBound()),
        selector_(associated_thread_) {}
  ~TaskQueueSelectorTest() override = default;

  void PushTasks(const size_t queue_indices[], size_t num_tasks) {
    EnqueueOrderGenerator enqueue_order_generator;
    for (size_t i = 0; i < num_tasks; i++) {
      task_queues_[queue_indices[i]]->immediate_work_queue()->Push(
          Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
               EnqueueOrder(), enqueue_order_generator.GenerateNext()));
    }
  }

  void PushTasksWithEnqueueOrder(const size_t queue_indices[],
                                 const size_t enqueue_orders[],
                                 size_t num_tasks) {
    for (size_t i = 0; i < num_tasks; i++) {
      task_queues_[queue_indices[i]]->immediate_work_queue()->Push(Task(
          PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
          EnqueueOrder(), EnqueueOrder::FromIntForTesting(enqueue_orders[i])));
    }
  }

  void PushTask(const size_t queue_index, const size_t enqueue_order) {
    task_queues_[queue_index]->immediate_work_queue()->Push(
        Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
             EnqueueOrder(), EnqueueOrder::FromIntForTesting(enqueue_order)));
  }

  std::vector<size_t> PopTasksAndReturnQueueIndices() {
    std::vector<size_t> order;
    while (WorkQueue* chosen_work_queue =
               selector_.SelectWorkQueueToService()) {
      size_t chosen_queue_index =
          queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
      order.push_back(chosen_queue_index);
      chosen_work_queue->PopTaskForTesting();
      chosen_work_queue->work_queue_sets()->OnPopMinQueueInSet(
          chosen_work_queue);
    }
    return order;
  }

  static void TestFunction() {}

 protected:
  void SetUp() final {
    time_domain_ = std::make_unique<MockTimeDomain>(TimeTicks() +
                                                    TimeDelta::FromSeconds(1));
    for (size_t i = 0; i < kTaskQueueCount; i++) {
      std::unique_ptr<TaskQueueImpl> task_queue =
          std::make_unique<TaskQueueImpl>(nullptr, time_domain_.get(),
                                          TaskQueue::Spec("test"));
      selector_.AddQueue(task_queue.get());
      task_queues_.push_back(std::move(task_queue));
    }
    for (size_t i = 0; i < kTaskQueueCount; i++) {
      EXPECT_EQ(TaskQueue::kNormalPriority, task_queues_[i]->GetQueuePriority())
          << i;
      queue_to_index_map_.insert(std::make_pair(task_queues_[i].get(), i));
    }
  }

  void TearDown() final {
    for (std::unique_ptr<TaskQueueImpl>& task_queue : task_queues_) {
      // Note since this test doesn't have a SequenceManager we need to
      // manually remove |task_queue| from the |selector_|.  Normally
      // UnregisterTaskQueue would do that.
      selector_.RemoveQueue(task_queue.get());
      task_queue->UnregisterTaskQueue();
    }
  }

  std::unique_ptr<TaskQueueImpl> NewTaskQueueWithBlockReporting() {
    return std::make_unique<TaskQueueImpl>(nullptr, time_domain_.get(),
                                           TaskQueue::Spec("test"));
  }

  const size_t kTaskQueueCount =
      static_cast<size_t>(TaskQueue::QueuePriority::kQueuePriorityCount);
  RepeatingClosure test_closure_;
  scoped_refptr<AssociatedThreadId> associated_thread_;
  TaskQueueSelectorForTest selector_;
  std::unique_ptr<TimeDomain> time_domain_;
  std::vector<std::unique_ptr<TaskQueueImpl>> task_queues_;
  std::map<TaskQueueImpl*, size_t> queue_to_index_map_;
};

TEST_F(TaskQueueSelectorTest, TestDefaultPriority) {
  size_t queue_order[] = {4, 3, 2, 1, 0};
  PushTasks(queue_order, 5);
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(4, 3, 2, 1, 0));
}

TEST_F(TaskQueueSelectorTest, TestHighestPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[2].get(),
                             TaskQueue::kHighestPriority);
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(2, 0, 1, 3, 4));
}

TEST_F(TaskQueueSelectorTest, TestHighPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[2].get(),
                             TaskQueue::kHighestPriority);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);
  selector_.SetQueuePriority(task_queues_[0].get(), TaskQueue::kLowPriority);
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(2, 1, 3, 4, 0));
}

TEST_F(TaskQueueSelectorTest, TestLowPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kLowPriority);
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(0, 1, 3, 4, 2));
}

TEST_F(TaskQueueSelectorTest, TestBestEffortPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kBestEffortPriority);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kLowPriority);
  selector_.SetQueuePriority(task_queues_[3].get(),
                             TaskQueue::kHighestPriority);
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(3, 1, 4, 2, 0));
}

TEST_F(TaskQueueSelectorTest, TestControlPriority) {
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  selector_.SetQueuePriority(task_queues_[4].get(),
                             TaskQueue::kControlPriority);
  EXPECT_EQ(TaskQueue::kControlPriority, task_queues_[4]->GetQueuePriority());
  selector_.SetQueuePriority(task_queues_[2].get(),
                             TaskQueue::kHighestPriority);
  EXPECT_EQ(TaskQueue::kHighestPriority, task_queues_[2]->GetQueuePriority());
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(4, 2, 0, 1, 3));
}

TEST_F(TaskQueueSelectorTest, TestObserverWithEnabledQueue) {
  task_queues_[1]->SetQueueEnabled(false);
  selector_.DisableQueue(task_queues_[1].get());
  MockObserver mock_observer;
  selector_.SetTaskQueueSelectorObserver(&mock_observer);
  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(1);
  task_queues_[1]->SetQueueEnabled(true);
  selector_.EnableQueue(task_queues_[1].get());
}

TEST_F(TaskQueueSelectorTest,
       TestObserverWithSetQueuePriorityAndQueueAlreadyEnabled) {
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::kHighestPriority);
  MockObserver mock_observer;
  selector_.SetTaskQueueSelectorObserver(&mock_observer);
  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(0);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kNormalPriority);
}

TEST_F(TaskQueueSelectorTest, TestDisableEnable) {
  MockObserver mock_observer;
  selector_.SetTaskQueueSelectorObserver(&mock_observer);

  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);
  task_queues_[2]->SetQueueEnabled(false);
  selector_.DisableQueue(task_queues_[2].get());
  task_queues_[4]->SetQueueEnabled(false);
  selector_.DisableQueue(task_queues_[4].get());
  // Disabling a queue should not affect its priority.
  EXPECT_EQ(TaskQueue::kNormalPriority, task_queues_[2]->GetQueuePriority());
  EXPECT_EQ(TaskQueue::kNormalPriority, task_queues_[4]->GetQueuePriority());
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(0, 1, 3));

  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(2);
  task_queues_[2]->SetQueueEnabled(true);
  selector_.EnableQueue(task_queues_[2].get());
  selector_.SetQueuePriority(task_queues_[2].get(),
                             TaskQueue::kBestEffortPriority);
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(2));
  task_queues_[4]->SetQueueEnabled(true);
  selector_.EnableQueue(task_queues_[4].get());
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(4));
}

TEST_F(TaskQueueSelectorTest, TestDisableChangePriorityThenEnable) {
  EXPECT_TRUE(task_queues_[2]->delayed_work_queue()->Empty());
  EXPECT_TRUE(task_queues_[2]->immediate_work_queue()->Empty());

  task_queues_[2]->SetQueueEnabled(false);
  selector_.SetQueuePriority(task_queues_[2].get(),
                             TaskQueue::kHighestPriority);

  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasks(queue_order, 5);

  EXPECT_TRUE(task_queues_[2]->delayed_work_queue()->Empty());
  EXPECT_FALSE(task_queues_[2]->immediate_work_queue()->Empty());
  task_queues_[2]->SetQueueEnabled(true);

  EXPECT_EQ(TaskQueue::kHighestPriority, task_queues_[2]->GetQueuePriority());
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(2, 0, 1, 3, 4));
}

TEST_F(TaskQueueSelectorTest, TestEmptyQueues) {
  EXPECT_EQ(nullptr, selector_.SelectWorkQueueToService());

  // Test only disabled queues.
  size_t queue_order[] = {0};
  PushTasks(queue_order, 1);
  task_queues_[0]->SetQueueEnabled(false);
  selector_.DisableQueue(task_queues_[0].get());
  EXPECT_EQ(nullptr, selector_.SelectWorkQueueToService());

  // These tests are unusual since there's no TQM. To avoid a later DCHECK when
  // deleting the task queue, we re-enable the queue here so the selector
  // doesn't get out of sync.
  task_queues_[0]->SetQueueEnabled(true);
  selector_.EnableQueue(task_queues_[0].get());
}

TEST_F(TaskQueueSelectorTest, TestAge) {
  size_t enqueue_order[] = {10, 1, 2, 9, 4};
  size_t queue_order[] = {0, 1, 2, 3, 4};
  PushTasksWithEnqueueOrder(queue_order, enqueue_order, 5);
  EXPECT_THAT(PopTasksAndReturnQueueIndices(), ElementsAre(1, 2, 4, 3, 0));
}

class TaskQueueSelectorStarvationTest : public TaskQueueSelectorTest {
 public:
  TaskQueueSelectorStarvationTest() = default;

 protected:
  void TestPriorityOrder(const size_t queue_order[], size_t num_tasks) {
    for (size_t i = 0; i < kTaskQueueCount; i++) {
      // Setting the queue priority to its current value causes a check to fail.
      if (task_queues_[i]->GetQueuePriority() !=
          static_cast<TaskQueue::QueuePriority>(i)) {
        selector_.SetQueuePriority(task_queues_[i].get(),
                                   static_cast<TaskQueue::QueuePriority>(i));
      }
    }

    ASSERT_EQ(num_tasks, kTaskQueueCount);
    PushTasks(queue_order, kTaskQueueCount);

    for (size_t priority = 0; priority < kTaskQueueCount; priority++) {
      for (int i = 0; i < 100; i++) {
        WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
        ASSERT_THAT(chosen_work_queue, NotNull());
        EXPECT_EQ(task_queues_[priority].get(),
                  chosen_work_queue->task_queue());
        // Don't remove task from queue to simulate all queues still being full.
      }

      // Simulate the highest priority queue becoming empty.
      WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
      chosen_work_queue->PopTaskForTesting();
      chosen_work_queue->work_queue_sets()->OnPopMinQueueInSet(
          chosen_work_queue);
    }
  }
};

TEST_F(TaskQueueSelectorStarvationTest,
       HigherPriorityWorkStarvesLowerPriorityWork) {
  size_t queue_order[kTaskQueueCount];
  for (size_t i = 0; i < kTaskQueueCount; i++)
    queue_order[i] = i;
  TestPriorityOrder(queue_order, kTaskQueueCount);
}

TEST_F(TaskQueueSelectorStarvationTest,
       NewHigherPriorityTasksStarveOldLowerPriorityTasks) {
  // Enqueue tasks in order from lowest to highest priority, and check that they
  // still run in order from highest to lowest priority.
  size_t queue_order[kTaskQueueCount];
  for (size_t i = 0; i < kTaskQueueCount; i++)
    queue_order[i] = (kTaskQueueCount - i) - 1;
  TestPriorityOrder(queue_order, kTaskQueueCount);
}

TEST_F(TaskQueueSelectorTest, GetHighestPendingPriority) {
  EXPECT_FALSE(selector_.GetHighestPendingPriority().has_value());
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);

  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);

  EXPECT_EQ(TaskQueue::kHighPriority, *selector_.GetHighestPendingPriority());
  PopTasksAndReturnQueueIndices();
  EXPECT_FALSE(selector_.GetHighestPendingPriority().has_value());

  PushTasks(queue_order, 1);
  EXPECT_EQ(TaskQueue::kNormalPriority, *selector_.GetHighestPendingPriority());
  PopTasksAndReturnQueueIndices();
  EXPECT_FALSE(selector_.GetHighestPendingPriority().has_value());
}

TEST_F(TaskQueueSelectorTest, ChooseWithPriority_Empty) {
  EXPECT_EQ(
      nullptr,
      selector_
          .ChooseWithPriority<TaskQueueSelectorForTest::SetOperationOldest>(
              TaskQueue::kNormalPriority));
}

TEST_F(TaskQueueSelectorTest, ChooseWithPriority_OnlyDelayed) {
  task_queues_[0]->delayed_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(2)));

  EXPECT_EQ(
      task_queues_[0]->delayed_work_queue(),
      selector_
          .ChooseWithPriority<TaskQueueSelectorForTest::SetOperationOldest>(
              TaskQueue::kNormalPriority));
}

TEST_F(TaskQueueSelectorTest, ChooseWithPriority_OnlyImmediate) {
  task_queues_[0]->immediate_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(2)));

  EXPECT_EQ(
      task_queues_[0]->immediate_work_queue(),
      selector_
          .ChooseWithPriority<TaskQueueSelectorForTest::SetOperationOldest>(
              TaskQueue::kNormalPriority));
}

TEST_F(TaskQueueSelectorTest,
       SelectWorkQueueToServiceImmediateOnlyWithoutImmediateTask) {
  task_queues_[0]->delayed_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(2)));

  EXPECT_EQ(nullptr,
            selector_.SelectWorkQueueToService(
                TaskQueueSelector::SelectTaskOption::kSkipDelayedTask));
  EXPECT_EQ(task_queues_[0]->delayed_work_queue(),
            selector_.SelectWorkQueueToService());
}

TEST_F(TaskQueueSelectorTest,
       SelectWorkQueueToServiceImmediateOnlyWithDelayedTasks) {
  task_queues_[0]->delayed_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(1)));
  task_queues_[0]->immediate_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(2)));

  EXPECT_EQ(task_queues_[0]->immediate_work_queue(),
            selector_.SelectWorkQueueToService(
                TaskQueueSelector::SelectTaskOption::kSkipDelayedTask));
  EXPECT_EQ(task_queues_[0]->delayed_work_queue(),
            selector_.SelectWorkQueueToService());
}

TEST_F(TaskQueueSelectorTest,
       SelectWorkQueueToServiceImmediateOnlyWithDisabledQueues) {
  task_queues_[0]->delayed_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(1)));
  task_queues_[0]->immediate_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(2)));
  task_queues_[1]->delayed_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(3)));
  task_queues_[2]->immediate_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(4)));

  EXPECT_EQ(task_queues_[0]->delayed_work_queue(),
            selector_.SelectWorkQueueToService());
  EXPECT_EQ(task_queues_[0]->immediate_work_queue(),
            selector_.SelectWorkQueueToService(
                TaskQueueSelector::SelectTaskOption::kSkipDelayedTask));

  task_queues_[0]->SetQueueEnabled(false);
  selector_.DisableQueue(task_queues_[0].get());

  EXPECT_EQ(task_queues_[1]->delayed_work_queue(),
            selector_.SelectWorkQueueToService());
  EXPECT_EQ(task_queues_[2]->immediate_work_queue(),
            selector_.SelectWorkQueueToService(
                TaskQueueSelector::SelectTaskOption::kSkipDelayedTask));

  task_queues_[1]->SetQueueEnabled(false);
  selector_.DisableQueue(task_queues_[1].get());

  EXPECT_EQ(task_queues_[2]->immediate_work_queue(),
            selector_.SelectWorkQueueToService(
                TaskQueueSelector::SelectTaskOption::kSkipDelayedTask));
  EXPECT_EQ(task_queues_[2]->immediate_work_queue(),
            selector_.SelectWorkQueueToService());
}

TEST_F(TaskQueueSelectorTest, TestObserverWithOneBlockedQueue) {
  TaskQueueSelectorForTest selector(associated_thread_);
  MockObserver mock_observer;
  selector.SetTaskQueueSelectorObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(1);

  std::unique_ptr<TaskQueueImpl> task_queue(NewTaskQueueWithBlockReporting());
  selector.AddQueue(task_queue.get());

  task_queue->SetQueueEnabled(false);
  selector.DisableQueue(task_queue.get());

  Task task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
            EnqueueOrder(), EnqueueOrder::FromIntForTesting(2));
  task_queue->immediate_work_queue()->Push(std::move(task));

  EXPECT_EQ(nullptr, selector.SelectWorkQueueToService());

  task_queue->SetQueueEnabled(true);
  selector.EnableQueue(task_queue.get());
  selector.RemoveQueue(task_queue.get());
  task_queue->UnregisterTaskQueue();
}

TEST_F(TaskQueueSelectorTest, TestObserverWithTwoBlockedQueues) {
  TaskQueueSelectorForTest selector(associated_thread_);
  MockObserver mock_observer;
  selector.SetTaskQueueSelectorObserver(&mock_observer);

  std::unique_ptr<TaskQueueImpl> task_queue(NewTaskQueueWithBlockReporting());
  std::unique_ptr<TaskQueueImpl> task_queue2(NewTaskQueueWithBlockReporting());
  selector.AddQueue(task_queue.get());
  selector.AddQueue(task_queue2.get());

  task_queue->SetQueueEnabled(false);
  task_queue2->SetQueueEnabled(false);
  selector.DisableQueue(task_queue.get());
  selector.DisableQueue(task_queue2.get());

  selector.SetQueuePriority(task_queue2.get(), TaskQueue::kControlPriority);

  Task task1(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
             EnqueueOrder::FromIntForTesting(2),
             EnqueueOrder::FromIntForTesting(2));
  Task task2(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
             EnqueueOrder::FromIntForTesting(3),
             EnqueueOrder::FromIntForTesting(3));
  task_queue->immediate_work_queue()->Push(std::move(task1));
  task_queue2->immediate_work_queue()->Push(std::move(task2));
  EXPECT_EQ(nullptr, selector.SelectWorkQueueToService());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnTaskQueueEnabled(_)).Times(2);

  task_queue->SetQueueEnabled(true);
  selector.EnableQueue(task_queue.get());

  selector.RemoveQueue(task_queue.get());
  task_queue->UnregisterTaskQueue();
  EXPECT_EQ(nullptr, selector.SelectWorkQueueToService());

  task_queue2->SetQueueEnabled(true);
  selector.EnableQueue(task_queue2.get());
  selector.RemoveQueue(task_queue2.get());
  task_queue2->UnregisterTaskQueue();
}

TEST_F(TaskQueueSelectorTest, CollectSkippedOverLowerPriorityTasks) {
  size_t queue_order[] = {0, 1, 2, 3, 2, 1, 0};
  PushTasks(queue_order, 7);
  selector_.SetQueuePriority(task_queues_[3].get(), TaskQueue::kHighPriority);

  std::vector<const Task*> result;
  selector_.CollectSkippedOverLowerPriorityTasks(
      task_queues_[3]->immediate_work_queue(), &result);

  ASSERT_EQ(3u, result.size());
  EXPECT_EQ(2u, result[0]->enqueue_order());  // The order here isn't important.
  EXPECT_EQ(3u, result[1]->enqueue_order());
  EXPECT_EQ(4u, result[2]->enqueue_order());
}

struct ChooseWithPriorityTestParam {
  int delayed_task_enqueue_order;
  int immediate_task_enqueue_order;
  int immediate_starvation_count;
  const char* expected_work_queue_name;
};

static const ChooseWithPriorityTestParam kChooseWithPriorityTestCases[] = {
    {1, 2, 0, "delayed"},   {1, 2, 1, "delayed"},   {1, 2, 2, "delayed"},
    {1, 2, 3, "immediate"}, {1, 2, 4, "immediate"}, {2, 1, 4, "immediate"},
    {2, 1, 4, "immediate"},
};

class ChooseWithPriorityTest
    : public TaskQueueSelectorTest,
      public testing::WithParamInterface<ChooseWithPriorityTestParam> {};

TEST_P(ChooseWithPriorityTest, RoundRobinTest) {
  task_queues_[0]->immediate_work_queue()->Push(Task(
      PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
      EnqueueOrder::FromIntForTesting(GetParam().immediate_task_enqueue_order),
      EnqueueOrder::FromIntForTesting(
          GetParam().immediate_task_enqueue_order)));

  task_queues_[0]->delayed_work_queue()->Push(Task(
      PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
      EnqueueOrder::FromIntForTesting(GetParam().delayed_task_enqueue_order),
      EnqueueOrder::FromIntForTesting(GetParam().delayed_task_enqueue_order)));

  selector_.SetImmediateStarvationCountForTest(
      GetParam().immediate_starvation_count);

  WorkQueue* chosen_work_queue =
      selector_
          .ChooseWithPriority<TaskQueueSelectorForTest::SetOperationOldest>(
              TaskQueue::kNormalPriority);
  EXPECT_EQ(chosen_work_queue->task_queue(), task_queues_[0].get());
  EXPECT_STREQ(chosen_work_queue->name(), GetParam().expected_work_queue_name);
}

INSTANTIATE_TEST_SUITE_P(ChooseWithPriorityTest,
                         ChooseWithPriorityTest,
                         testing::ValuesIn(kChooseWithPriorityTestCases));

class ActivePriorityTrackerTest : public testing::Test {
 protected:
  TaskQueueSelectorForTest::ActivePriorityTracker active_priority_tracker_;
};

TEST_F(ActivePriorityTrackerTest, SetPriorityActiveAndInactive) {
  EXPECT_FALSE(active_priority_tracker_.HasActivePriority());
  EXPECT_FALSE(active_priority_tracker_.IsActive(
      TaskQueue::QueuePriority::kNormalPriority));

  active_priority_tracker_.SetActive(TaskQueue::QueuePriority::kNormalPriority,
                                     true);

  EXPECT_TRUE(active_priority_tracker_.HasActivePriority());
  EXPECT_TRUE(active_priority_tracker_.IsActive(
      TaskQueue::QueuePriority::kNormalPriority));

  active_priority_tracker_.SetActive(TaskQueue::QueuePriority::kNormalPriority,
                                     false);

  EXPECT_FALSE(active_priority_tracker_.HasActivePriority());
  EXPECT_FALSE(active_priority_tracker_.IsActive(
      TaskQueue::QueuePriority::kNormalPriority));
}

TEST_F(ActivePriorityTrackerTest, HighestActivePriority) {
  EXPECT_FALSE(active_priority_tracker_.HasActivePriority());

  for (size_t i = 0; i < TaskQueue::QueuePriority::kQueuePriorityCount; i++) {
    TaskQueue::QueuePriority priority =
        static_cast<TaskQueue::QueuePriority>(i);
    EXPECT_FALSE(active_priority_tracker_.IsActive(priority));
    active_priority_tracker_.SetActive(priority, true);
    EXPECT_TRUE(active_priority_tracker_.IsActive(priority));
  }

  for (size_t i = 0; i < TaskQueue::QueuePriority::kQueuePriorityCount; i++) {
    EXPECT_TRUE(active_priority_tracker_.HasActivePriority());
    TaskQueue::QueuePriority priority =
        static_cast<TaskQueue::QueuePriority>(i);
    EXPECT_EQ(active_priority_tracker_.HighestActivePriority(), priority);
    active_priority_tracker_.SetActive(priority, false);
  }

  EXPECT_FALSE(active_priority_tracker_.HasActivePriority());
}

}  // namespace task_queue_selector_unittest
}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
