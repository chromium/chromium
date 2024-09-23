// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/task_order.h"

#include <optional>

#include "base/task/sequence_manager/enqueue_order.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace sequence_manager {

class TaskOrderTest : public testing::Test {
 protected:
  static TaskOrder MakeImmediateTaskOrder(int enqueue_order) {
    return MakeTaskOrder(enqueue_order, TimeTicks(), enqueue_order);
  }

  static TaskOrder MakeDelayedTaskOrder(int enqueue_order,
                                        TimeTicks delayed_run_time,
                                        int sequence_num) {
    return MakeTaskOrder(enqueue_order, delayed_run_time, sequence_num);
  }

  void ExpectLessThan(TaskOrder& order1, TaskOrder& order2) {
    EXPECT_TRUE(order1 < order2);
    EXPECT_TRUE(order1 <= order2);
    EXPECT_FALSE(order1 == order2);
    EXPECT_TRUE(order1 != order2);
    EXPECT_FALSE(order1 >= order2);
    EXPECT_FALSE(order1 > order2);

    EXPECT_FALSE(order2 < order1);
    EXPECT_FALSE(order2 <= order1);
    EXPECT_FALSE(order2 == order1);
    EXPECT_TRUE(order1 != order2);
    EXPECT_TRUE(order2 >= order1);
    EXPECT_TRUE(order2 > order1);
  }

  void ExpectEqual(TaskOrder& order1, TaskOrder& order2) {
    EXPECT_FALSE(order1 < order2);
    EXPECT_TRUE(order1 <= order2);
    EXPECT_TRUE(order1 == order2);
    EXPECT_FALSE(order1 != order2);
    EXPECT_TRUE(order1 >= order2);
    EXPECT_FALSE(order1 > order2);

    EXPECT_FALSE(order2 < order1);
    EXPECT_TRUE(order2 <= order1);
    EXPECT_TRUE(order2 == order1);
    EXPECT_FALSE(order1 != order2);
    EXPECT_TRUE(order2 >= order1);
    EXPECT_FALSE(order2 > order1);
  }

 private:
  static TaskOrder MakeTaskOrder(int enqueue_order,
                                 TimeTicks delayed_run_time,
                                 int sequence_num) {
    return TaskOrder::CreateForTesting(
        EnqueueOrder::FromIntForTesting(enqueue_order), delayed_run_time,
        sequence_num);
  }
};

TEST_F(TaskOrderTest, ImmediateTasksNotEqual) {
  TaskOrder order1 = MakeImmediateTaskOrder(/*enqueue_order=*/10);
  TaskOrder order2 = MakeImmediateTaskOrder(/*enqueue_order=*/11);

  ExpectLessThan(order1, order2);
}

TEST_F(TaskOrderTest, ImmediateTasksEqual) {
  TaskOrder order1 = MakeImmediateTaskOrder(/*enqueue_order=*/10);
  TaskOrder order2 = MakeImmediateTaskOrder(/*enqueue_order=*/10);

  ExpectEqual(order1, order2);
}

TEST_F(TaskOrderTest, DelayedTasksOrderedByEnqueueNumberFirst) {
  // Enqueued earlier but has and a later delayed run time and posting order.
  TaskOrder order1 = MakeDelayedTaskOrder(
      /*enqueue_order=*/10, /*delayed_run_time=*/TimeTicks() + Seconds(2),
      /*sequence_num=*/2);
  TaskOrder order2 = MakeDelayedTaskOrder(
      /*enqueue_order=*/11, /*delayed_run_time=*/TimeTicks() + Seconds(1),
      /*sequence_num=*/1);

  ExpectLessThan(order1, order2);
}

TEST_F(TaskOrderTest, DelayedTasksSameEnqueueOrder) {
  TaskOrder order1 = MakeDelayedTaskOrder(
      /*enqueue_order=*/10, /*delayed_run_time=*/TimeTicks() + Seconds(1),
      /*sequence_num=*/2);
  TaskOrder order2 = MakeDelayedTaskOrder(
      /*enqueue_order=*/10, /*delayed_run_time=*/TimeTicks() + Seconds(2),
      /*sequence_num=*/1);

  ExpectLessThan(order1, order2);
}

TEST_F(TaskOrderTest, DelayedTasksSameEnqueueOrderAndRunTime) {
  TaskOrder order1 = MakeDelayedTaskOrder(
      /*enqueue_order=*/10, /*delayed_run_time=*/TimeTicks() + Seconds(1),
      /*sequence_num=*/1);
  TaskOrder order2 = MakeDelayedTaskOrder(
      /*enqueue_order=*/10, /*delayed_run_time=*/TimeTicks() + Seconds(1),
      /*sequence_num=*/2);

  ExpectLessThan(order1, order2);
}

TEST_F(TaskOrderTest, DelayedTasksEqual) {
  TaskOrder order1 = MakeDelayedTaskOrder(
      /*enqueue_order=*/10, /*delayed_run_time=*/TimeTicks() + Seconds(1),
      /*sequence_num=*/1);
  TaskOrder order2 = MakeDelayedTaskOrder(
      /*enqueue_order=*/10, /*delayed_run_time=*/TimeTicks() + Seconds(1),
      /*sequence_num=*/1);

  ExpectEqual(order1, order2);
}

}  // namespace sequence_manager
}  // namespace base
