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
#include "base/macros.h"
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
  ~MockObserver() override = default;

  MOCK_METHOD1(OnTaskQueueEnabled, void(internal::TaskQueueImpl*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockObserver);
};

class TaskQueueSelectorForTest : public TaskQueueSelector {
 public:
  using TaskQueueSelector::ChooseWithPriority;
  using TaskQueueSelector::delayed_work_queue_sets;
  using TaskQueueSelector::immediate_work_queue_sets;
  using TaskQueueSelector::kMaxHighPriorityStarvationScore;
  using TaskQueueSelector::kMaxLowPriorityStarvationScore;
  using TaskQueueSelector::kMaxNormalPriorityStarvationScore;
  using TaskQueueSelector::SetImmediateStarvationCountForTest;
  using TaskQueueSelector::SetOperationOldest;
  using TaskQueueSelector::SmallPriorityQueue;

  TaskQueueSelectorForTest(scoped_refptr<AssociatedThreadId> associated_thread,
                           const SequenceManager::Settings& settings)
      : TaskQueueSelector(associated_thread, settings) {}
};

class TaskQueueSelectorTestBase : public testing::Test {
 public:
  explicit TaskQueueSelectorTestBase(const SequenceManager::Settings& settings)
      : test_closure_(BindRepeating(&TaskQueueSelectorTestBase::TestFunction)),
        associated_thread_(AssociatedThreadId::CreateBound()),
        selector_(associated_thread_, settings) {}
  ~TaskQueueSelectorTestBase() override = default;

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

  const size_t kTaskQueueCount = 5;
  RepeatingClosure test_closure_;
  scoped_refptr<AssociatedThreadId> associated_thread_;
  TaskQueueSelectorForTest selector_;
  std::unique_ptr<TimeDomain> time_domain_;
  std::vector<std::unique_ptr<TaskQueueImpl>> task_queues_;
  std::map<TaskQueueImpl*, size_t> queue_to_index_map_;
};

class TaskQueueSelectorTest : public TaskQueueSelectorTestBase {
 public:
  TaskQueueSelectorTest()
      : TaskQueueSelectorTestBase(
            SequenceManager::Settings::Builder()
                .SetAntiStarvationLogicForPrioritiesDisabled(false)
                .Build()) {}
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

TEST_F(TaskQueueSelectorTest, TestControlStarvesOthers) {
  size_t queue_order[] = {0, 1, 2, 3};
  PushTasks(queue_order, 4);
  selector_.SetQueuePriority(task_queues_[3].get(),
                             TaskQueue::kControlPriority);
  selector_.SetQueuePriority(task_queues_[2].get(),
                             TaskQueue::kHighestPriority);
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::kBestEffortPriority);
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    EXPECT_EQ(task_queues_[3].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }
}

TEST_F(TaskQueueSelectorTest, ControlTasksDontTriggerAntiStarvationLogic) {
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kControlPriority);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kLowPriority);
  // |task_queues_[2]| has normal priority by default.

  // Run a number of control tasks.
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    EXPECT_EQ(task_queues_[0].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }

  // Simulate the control queue becoming empty.
  WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
  chosen_work_queue->PopTaskForTesting();
  chosen_work_queue->work_queue_sets()->OnPopMinQueueInSet(chosen_work_queue);

  // Simulate posting a normal priority task.
  PushTask(2, 2);
  chosen_work_queue = selector_.SelectWorkQueueToService();
  ASSERT_THAT(chosen_work_queue, NotNull());

  // Check that the low priority task is not considered starved, and so the
  // normal priority task we just posted runs.
  EXPECT_EQ(chosen_work_queue->task_queue(), task_queues_[2].get());
}

TEST_F(TaskQueueSelectorTest, TestHighestPriorityDoesNotStarveHigh) {
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kHighestPriority);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);

  size_t counts[] = {0, 0};
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    size_t chosen_queue_index =
        queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }
  EXPECT_GT(counts[1], 0ul);        // Check highest doesn't starve high.
  EXPECT_GT(counts[0], counts[1]);  // Check highest gets more chance to run.
}

TEST_F(TaskQueueSelectorTest, TestHighestPriorityDoesNotStarveHighOrNormal) {
  size_t queue_order[] = {0, 1, 2};
  PushTasks(queue_order, 3);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kHighestPriority);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);

  size_t counts[] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    size_t chosen_queue_index =
        queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }

  // Check highest runs more frequently then high.
  EXPECT_GT(counts[0], counts[1]);

  // Check high runs at least as frequently as normal.
  EXPECT_GE(counts[1], counts[2]);

  // Check normal isn't starved.
  EXPECT_GT(counts[2], 0ul);
}

TEST_F(TaskQueueSelectorTest,
       TestHighestPriorityDoesNotStarveHighOrNormalOrLow) {
  size_t queue_order[] = {0, 1, 2, 3};
  PushTasks(queue_order, 4);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kHighestPriority);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);
  selector_.SetQueuePriority(task_queues_[3].get(), TaskQueue::kLowPriority);

  size_t counts[] = {0, 0, 0, 0};
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    size_t chosen_queue_index =
        queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }

  // Check highest runs more frequently then high.
  EXPECT_GT(counts[0], counts[1]);

  // Check high runs at least as frequently as normal.
  EXPECT_GE(counts[1], counts[2]);

  // Check normal runs more frequently than low.
  EXPECT_GT(counts[2], counts[3]);

  // Check low isn't starved.
  EXPECT_GT(counts[3], 0ul);
}

TEST_F(TaskQueueSelectorTest, TestHighPriorityDoesNotStarveNormal) {
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);

  selector_.SetQueuePriority(task_queues_[0].get(), TaskQueue::kHighPriority);

  size_t counts[] = {0, 0, 0, 0};
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    size_t chosen_queue_index =
        queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }

  // Check high runs more frequently then normal.
  EXPECT_GT(counts[0], counts[1]);

  // Check low isn't starved.
  EXPECT_GT(counts[1], 0ul);
}

TEST_F(TaskQueueSelectorTest, TestHighPriorityDoesNotStarveNormalOrLow) {
  size_t queue_order[] = {0, 1, 2};
  PushTasks(queue_order, 3);
  selector_.SetQueuePriority(task_queues_[0].get(), TaskQueue::kHighPriority);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kLowPriority);

  size_t counts[] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    size_t chosen_queue_index =
        queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }

  // Check high runs more frequently than normal.
  EXPECT_GT(counts[0], counts[1]);

  // Check normal runs more frequently than low.
  EXPECT_GT(counts[1], counts[2]);

  // Check low isn't starved.
  EXPECT_GT(counts[2], 0ul);
}

TEST_F(TaskQueueSelectorTest, TestNormalPriorityDoesNotStarveLow) {
  size_t queue_order[] = {0, 1, 2};
  PushTasks(queue_order, 3);
  selector_.SetQueuePriority(task_queues_[0].get(), TaskQueue::kLowPriority);
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::kBestEffortPriority);
  size_t counts[] = {0, 0, 0};
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    size_t chosen_queue_index =
        queue_to_index_map_.find(chosen_work_queue->task_queue())->second;
    counts[chosen_queue_index]++;
    // Don't remove task from queue to simulate all queues still being full.
  }
  EXPECT_GT(counts[0], 0ul);        // Check normal doesn't starve low.
  EXPECT_GT(counts[2], counts[0]);  // Check normal gets more chance to run.
  EXPECT_EQ(0ul, counts[1]);        // Check best effort is starved.
}

TEST_F(TaskQueueSelectorTest, TestBestEffortGetsStarved) {
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kBestEffortPriority);
  EXPECT_EQ(TaskQueue::kNormalPriority, task_queues_[1]->GetQueuePriority());

  // Check that normal priority tasks starve best effort.
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }

  // Check that highest priority tasks starve best effort.
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::kHighestPriority);
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }

  // Check that high priority tasks starve best effort.
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }

  // Check that low priority tasks starve best effort.
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kLowPriority);
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }

  // Check that control priority tasks starve best effort.
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::kControlPriority);
  for (int i = 0; i < 100; i++) {
    WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
    ASSERT_THAT(chosen_work_queue, NotNull());
    EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
    // Don't remove task from queue to simulate all queues still being full.
  }
}

TEST_F(TaskQueueSelectorTest,
       TestHighPriorityStarvationScoreIncreasedOnlyWhenTasksArePresent) {
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kHighestPriority);
  selector_.SetQueuePriority(task_queues_[1].get(),
                             TaskQueue::kHighestPriority);

  // Run a number of highest priority tasks needed to starve high priority
  // tasks (when present).
  for (size_t num_tasks = 0;
       num_tasks <= TaskQueueSelectorForTest::kMaxHighPriorityStarvationScore;
       num_tasks++) {
    ASSERT_THAT(selector_.SelectWorkQueueToService(), NotNull());
    // Don't remove task from queue to simulate the queue is still full.
  }

  // Simulate posting a high priority task.
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);
  WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
  ASSERT_THAT(chosen_work_queue, NotNull());

  // Check that the high priority task is not considered starved, and thus isn't
  // processed.
  EXPECT_NE(chosen_work_queue->task_queue(), task_queues_[1].get());
}

TEST_F(TaskQueueSelectorTest,
       TestNormalPriorityStarvationScoreIncreasedOnlyWhenTasksArePresent) {
  size_t queue_order[] = {0, 1, 2};
  PushTasks(queue_order, 3);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kHighestPriority);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kHighPriority);

  // Run a number of highest or high priority tasks needed to starve normal
  // priority tasks (when present).
  for (size_t num_tasks = 0;
       num_tasks <= TaskQueueSelectorForTest::kMaxNormalPriorityStarvationScore;
       num_tasks++) {
    ASSERT_THAT(selector_.SelectWorkQueueToService(), NotNull());
    // Don't remove task from queue to simulate the queue is still full.
  }

  // Simulate posting a normal priority task.
  selector_.SetQueuePriority(task_queues_[2].get(), TaskQueue::kNormalPriority);
  WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
  ASSERT_THAT(chosen_work_queue, NotNull());

  // Check that the normal priority task is not considered starved, and thus
  // isn't processed.
  EXPECT_NE(chosen_work_queue->task_queue(), task_queues_[2].get());
}

TEST_F(TaskQueueSelectorTest,
       TestLowPriorityStarvationScoreIncreasedOnlyWhenTasksArePresent) {
  size_t queue_order[] = {0, 1, 2, 3};
  PushTasks(queue_order, 4);
  selector_.SetQueuePriority(task_queues_[0].get(),
                             TaskQueue::kHighestPriority);
  selector_.SetQueuePriority(task_queues_[1].get(), TaskQueue::kHighPriority);
  // NOTE |task_queues_[2]_| and |task_queues_[2]_| are already kNormalPriority.

  // Run a number of highest, high or normal priority tasks needed to starve
  // low priority tasks (when present).
  for (size_t num_tasks = 0;
       num_tasks <= TaskQueueSelectorForTest::kMaxLowPriorityStarvationScore;
       num_tasks++) {
    ASSERT_THAT(selector_.SelectWorkQueueToService(), NotNull());
    // Don't remove task from queue to simulate the queue is still full.
  }

  // Simulate posting a low priority task.
  selector_.SetQueuePriority(task_queues_[3].get(), TaskQueue::kLowPriority);
  WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
  ASSERT_THAT(chosen_work_queue, NotNull());

  // Check that the low priority task is not considered starved, and thus
  // isn't processed.
  EXPECT_NE(chosen_work_queue->task_queue(), task_queues_[3].get());
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
  bool chose_delayed_over_immediate = false;
  EXPECT_EQ(
      nullptr,
      selector_
          .ChooseWithPriority<TaskQueueSelectorForTest::SetOperationOldest>(
              TaskQueue::kNormalPriority, &chose_delayed_over_immediate));
  EXPECT_FALSE(chose_delayed_over_immediate);
}

TEST_F(TaskQueueSelectorTest, ChooseWithPriority_OnlyDelayed) {
  task_queues_[0]->delayed_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(2)));

  bool chose_delayed_over_immediate = false;
  EXPECT_EQ(
      task_queues_[0]->delayed_work_queue(),
      selector_
          .ChooseWithPriority<TaskQueueSelectorForTest::SetOperationOldest>(
              TaskQueue::kNormalPriority, &chose_delayed_over_immediate));
  EXPECT_FALSE(chose_delayed_over_immediate);
}

TEST_F(TaskQueueSelectorTest, ChooseWithPriority_OnlyImmediate) {
  task_queues_[0]->immediate_work_queue()->Push(
      Task(PostedTask(nullptr, test_closure_, FROM_HERE), TimeTicks(),
           EnqueueOrder(), EnqueueOrder::FromIntForTesting(2)));

  bool chose_delayed_over_immediate = false;
  EXPECT_EQ(
      task_queues_[0]->immediate_work_queue(),
      selector_
          .ChooseWithPriority<TaskQueueSelectorForTest::SetOperationOldest>(
              TaskQueue::kNormalPriority, &chose_delayed_over_immediate));
  EXPECT_FALSE(chose_delayed_over_immediate);
}

TEST_F(TaskQueueSelectorTest, TestObserverWithOneBlockedQueue) {
  TaskQueueSelectorForTest selector(associated_thread_,
                                    SequenceManager::Settings());
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
  TaskQueueSelectorForTest selector(associated_thread_,
                                    SequenceManager::Settings());
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

class DisabledAntiStarvationLogicTaskQueueSelectorTest
    : public TaskQueueSelectorTestBase,
      public testing::WithParamInterface<TaskQueue::QueuePriority> {
 public:
  DisabledAntiStarvationLogicTaskQueueSelectorTest()
      : TaskQueueSelectorTestBase(
            SequenceManager::Settings::Builder()
                .SetAntiStarvationLogicForPrioritiesDisabled(true)
                .Build()) {}
};

TEST_P(DisabledAntiStarvationLogicTaskQueueSelectorTest,
       TestStarvedByHigherPriorities) {
  TaskQueue::QueuePriority priority_to_test = GetParam();
  size_t queue_order[] = {0, 1};
  PushTasks(queue_order, 2);

  // Setting the queue priority to its current value causes a check to fail.
  if (task_queues_[0]->GetQueuePriority() != priority_to_test) {
    selector_.SetQueuePriority(task_queues_[0].get(), priority_to_test);
  }

  // Test that |priority_to_test| is starved by all higher priorities.
  for (int higher_priority = static_cast<int>(priority_to_test) - 1;
       higher_priority >= 0; higher_priority--) {
    // Setting the queue priority to its current value causes a check to fail.
    if (task_queues_[1]->GetQueuePriority() != higher_priority) {
      selector_.SetQueuePriority(
          task_queues_[1].get(),
          static_cast<TaskQueue::QueuePriority>(higher_priority));
    }

    for (int i = 0; i < 100; i++) {
      WorkQueue* chosen_work_queue = selector_.SelectWorkQueueToService();
      ASSERT_THAT(chosen_work_queue, NotNull());
      EXPECT_EQ(task_queues_[1].get(), chosen_work_queue->task_queue());
      // Don't remove task from queue to simulate all queues still being full.
    }
  }
}

std::string GetPriorityTestNameSuffix(
    const testing::TestParamInfo<TaskQueue::QueuePriority>& info) {
  return TaskQueue::PriorityToString(info.param);
}

INSTANTIATE_TEST_SUITE_P(,
                         DisabledAntiStarvationLogicTaskQueueSelectorTest,
                         testing::Values(TaskQueue::kHighestPriority,
                                         TaskQueue::kVeryHighPriority,
                                         TaskQueue::kHighPriority,
                                         TaskQueue::kNormalPriority,
                                         TaskQueue::kLowPriority,
                                         TaskQueue::kBestEffortPriority),
                         GetPriorityTestNameSuffix);

struct ChooseWithPriorityTestParam {
  int delayed_task_enqueue_order;
  int immediate_task_enqueue_order;
  int immediate_starvation_count;
  const char* expected_work_queue_name;
  bool expected_did_starve_immediate_queue;
};

static const ChooseWithPriorityTestParam kChooseWithPriorityTestCases[] = {
    {1, 2, 0, "delayed", true},    {1, 2, 1, "delayed", true},
    {1, 2, 2, "delayed", true},    {1, 2, 3, "immediate", false},
    {1, 2, 4, "immediate", false}, {2, 1, 4, "immediate", false},
    {2, 1, 4, "immediate", false},
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

  bool chose_delayed_over_immediate = false;
  WorkQueue* chosen_work_queue =
      selector_
          .ChooseWithPriority<TaskQueueSelectorForTest::SetOperationOldest>(
              TaskQueue::kNormalPriority, &chose_delayed_over_immediate);
  EXPECT_EQ(chosen_work_queue->task_queue(), task_queues_[0].get());
  EXPECT_STREQ(chosen_work_queue->name(), GetParam().expected_work_queue_name);
  EXPECT_EQ(chose_delayed_over_immediate,
            GetParam().expected_did_starve_immediate_queue);
}

INSTANTIATE_TEST_SUITE_P(ChooseWithPriorityTest,
                         ChooseWithPriorityTest,
                         testing::ValuesIn(kChooseWithPriorityTestCases));

class SmallPriorityQueueTest : public testing::Test {
 public:
  TaskQueueSelectorForTest::SmallPriorityQueue queue_;

  std::vector<uint8_t> PopAllIds() {
    std::vector<uint8_t> result;
    while (!queue_.empty()) {
      result.push_back(queue_.min_id());
      queue_.erase(queue_.min_id());
    }
    return result;
  }
};

TEST_F(SmallPriorityQueueTest, Insert) {
  EXPECT_TRUE(queue_.empty());

  EXPECT_FALSE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(1)));
  queue_.insert(1000, static_cast<TaskQueue::QueuePriority>(1));
  EXPECT_TRUE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(1)));
  EXPECT_EQ(1, queue_.min_id());
  EXPECT_FALSE(queue_.empty());

  EXPECT_FALSE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(2)));
  queue_.insert(1002, static_cast<TaskQueue::QueuePriority>(2));
  EXPECT_TRUE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(2)));
  EXPECT_EQ(1, queue_.min_id());

  EXPECT_FALSE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(3)));
  queue_.insert(999, static_cast<TaskQueue::QueuePriority>(3));
  EXPECT_TRUE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(3)));
  EXPECT_EQ(3, queue_.min_id());

  EXPECT_FALSE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(4)));
  queue_.insert(1003, static_cast<TaskQueue::QueuePriority>(4));
  EXPECT_TRUE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(4)));
  EXPECT_EQ(3, queue_.min_id());
}

TEST_F(SmallPriorityQueueTest, EraseMin) {
  queue_.insert(1000, static_cast<TaskQueue::QueuePriority>(1));
  queue_.insert(1002, static_cast<TaskQueue::QueuePriority>(2));
  queue_.insert(999, static_cast<TaskQueue::QueuePriority>(3));
  queue_.insert(1003, static_cast<TaskQueue::QueuePriority>(4));

  EXPECT_EQ(3, queue_.min_id());
  EXPECT_TRUE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(4)));

  queue_.erase(static_cast<TaskQueue::QueuePriority>(4));
  EXPECT_FALSE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(4)));
  EXPECT_EQ(3, queue_.min_id());
  EXPECT_TRUE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(3)));

  queue_.erase(static_cast<TaskQueue::QueuePriority>(3));
  EXPECT_FALSE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(3)));
  EXPECT_EQ(1, queue_.min_id());
  EXPECT_TRUE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(2)));

  queue_.erase(static_cast<TaskQueue::QueuePriority>(2));
  EXPECT_FALSE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(2)));
  EXPECT_EQ(1, queue_.min_id());
  EXPECT_TRUE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(1)));

  queue_.erase(static_cast<TaskQueue::QueuePriority>(1));
  EXPECT_FALSE(queue_.IsInQueue(static_cast<TaskQueue::QueuePriority>(1)));
  EXPECT_TRUE(queue_.empty());
}

TEST_F(SmallPriorityQueueTest, EraseMiddle) {
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(1));
  queue_.insert(101, static_cast<TaskQueue::QueuePriority>(2));
  queue_.insert(102, static_cast<TaskQueue::QueuePriority>(3));
  queue_.insert(103, static_cast<TaskQueue::QueuePriority>(4));
  queue_.insert(104, static_cast<TaskQueue::QueuePriority>(5));

  queue_.erase(static_cast<TaskQueue::QueuePriority>(3));

  EXPECT_THAT(PopAllIds(), ElementsAre(1, 2, 4, 5));
}

TEST_F(SmallPriorityQueueTest, EraseAllButOne) {
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(1));
  queue_.insert(101, static_cast<TaskQueue::QueuePriority>(2));
  queue_.insert(102, static_cast<TaskQueue::QueuePriority>(3));
  queue_.insert(103, static_cast<TaskQueue::QueuePriority>(4));
  queue_.insert(104, static_cast<TaskQueue::QueuePriority>(5));

  queue_.erase(static_cast<TaskQueue::QueuePriority>(5));
  queue_.erase(static_cast<TaskQueue::QueuePriority>(1));
  queue_.erase(static_cast<TaskQueue::QueuePriority>(3));
  queue_.erase(static_cast<TaskQueue::QueuePriority>(2));

  EXPECT_THAT(PopAllIds(), ElementsAre(4));
}

TEST_F(SmallPriorityQueueTest, ChangeMinKeyNoOrderDifference) {
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(1));
  queue_.insert(101, static_cast<TaskQueue::QueuePriority>(2));
  queue_.insert(102, static_cast<TaskQueue::QueuePriority>(3));
  queue_.insert(105, static_cast<TaskQueue::QueuePriority>(4));
  queue_.insert(106, static_cast<TaskQueue::QueuePriority>(5));

  queue_.ChangeMinKey(99);

  EXPECT_THAT(PopAllIds(), ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(SmallPriorityQueueTest, ChangeMinKeyToMiddle) {
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(1));
  queue_.insert(101, static_cast<TaskQueue::QueuePriority>(2));
  queue_.insert(102, static_cast<TaskQueue::QueuePriority>(3));
  queue_.insert(105, static_cast<TaskQueue::QueuePriority>(4));
  queue_.insert(106, static_cast<TaskQueue::QueuePriority>(5));

  queue_.ChangeMinKey(103);

  EXPECT_THAT(PopAllIds(), ElementsAre(2, 3, 1, 4, 5));
}

TEST_F(SmallPriorityQueueTest, ChangeMinKeyToLast) {
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(1));
  queue_.insert(101, static_cast<TaskQueue::QueuePriority>(2));
  queue_.insert(102, static_cast<TaskQueue::QueuePriority>(3));
  queue_.insert(105, static_cast<TaskQueue::QueuePriority>(4));
  queue_.insert(106, static_cast<TaskQueue::QueuePriority>(5));

  queue_.ChangeMinKey(107);

  EXPECT_THAT(PopAllIds(), ElementsAre(2, 3, 4, 5, 1));
}

TEST_F(SmallPriorityQueueTest, StableSortingOrder) {
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(1));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(2));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(3));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(4));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(5));

  EXPECT_THAT(PopAllIds(), ElementsAre(1, 2, 3, 4, 5));
}

TEST_F(SmallPriorityQueueTest, StableSortingOrderRemoveMiddle) {
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(1));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(2));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(3));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(4));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(5));
  queue_.erase(static_cast<TaskQueue::QueuePriority>(3));

  EXPECT_THAT(PopAllIds(), ElementsAre(1, 2, 4, 5));
}

TEST_F(SmallPriorityQueueTest, StableSortingOrderChangeMinToLast) {
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(1));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(2));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(3));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(4));
  queue_.insert(100, static_cast<TaskQueue::QueuePriority>(5));
  queue_.ChangeMinKey(101);

  EXPECT_THAT(PopAllIds(), ElementsAre(2, 3, 4, 5, 1));
}

}  // namespace task_queue_selector_unittest
}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
