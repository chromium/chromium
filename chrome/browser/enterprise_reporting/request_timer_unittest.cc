// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/request_timer.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
class RequestTimerTest : public ::testing::Test {
 public:
  RequestTimerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void RunTask() { task_count_ += 1; }

 protected:
  void StartTask(int first_delay, int repeat_delay) {
    timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(first_delay),
                 base::TimeDelta::FromSeconds(repeat_delay),
                 base::BindRepeating(&RequestTimerTest::RunTask,
                                     base::Unretained(this)));
  }

  base::test::TaskEnvironment task_environment_;
  int task_count_ = 0;
  RequestTimer timer_;
};

TEST_F(RequestTimerTest, StartTimer) {
  StartTask(5, 10);
  ASSERT_TRUE(timer_.IsFirstTimerRunning());
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
}

TEST_F(RequestTimerTest, StartTimerWithLargerFirstDelay) {
  StartTask(20, 10);
  ASSERT_FALSE(timer_.IsFirstTimerRunning());
  ASSERT_TRUE(timer_.IsRepeatTimerRunning());
}

TEST_F(RequestTimerTest, RunFirstTask) {
  StartTask(5, 10);

  // The first task hasn't been ran yet.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(4));
  ASSERT_TRUE(timer_.IsFirstTimerRunning());
  ASSERT_EQ(0, task_count_);

  // The first task is due.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_FALSE(timer_.IsFirstTimerRunning());
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
  ASSERT_EQ(1, task_count_);
}

TEST_F(RequestTimerTest, RunFirstTaskWithoutDelay) {
  StartTask(0, 10);

  // First task is posted and ran immediately.
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(timer_.IsFirstTimerRunning());
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
  ASSERT_EQ(1, task_count_);
}

TEST_F(RequestTimerTest, RunFirstTaskWithNegativeDelay) {
  StartTask(-1, 10);

  // First task is posted and ran immediately.
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(timer_.IsFirstTimerRunning());
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
  ASSERT_EQ(1, task_count_);
}

TEST_F(RequestTimerTest, RunRepeatingTaskTest) {
  StartTask(1, 10);

  // Run the first task.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  // Reset and wait for the second task.
  timer_.Reset();
  ASSERT_FALSE(timer_.IsFirstTimerRunning());
  ASSERT_TRUE(timer_.IsRepeatTimerRunning());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  ASSERT_FALSE(timer_.IsFirstTimerRunning());
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
  ASSERT_EQ(2, task_count_);

  // Reset and wait for the third task.
  timer_.Reset();
  ASSERT_TRUE(timer_.IsRepeatTimerRunning());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  ASSERT_FALSE(timer_.IsFirstTimerRunning());
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
  ASSERT_EQ(3, task_count_);
}

TEST_F(RequestTimerTest, NotRuningRepeatingTaskWithoutResetTest) {
  StartTask(1, 10);

  // Run the first task.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(1, task_count_);

  // The repeat task is not ran without reset.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(15));
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
  ASSERT_EQ(1, task_count_);

  // Reset the task, the repeat task is ran only once.
  timer_.Reset();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(60));
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
  ASSERT_EQ(2, task_count_);
}

TEST_F(RequestTimerTest, StopFirstTask) {
  StartTask(1, 10);

  timer_.Stop();

  // The task is stopped.
  ASSERT_FALSE(timer_.IsFirstTimerRunning());
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  ASSERT_EQ(0, task_count_);
}

TEST_F(RequestTimerTest, StopRepeatTask) {
  StartTask(1, 10);

  // Run the first task.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(1, task_count_);

  // Start the repeat task, stop after a while.
  timer_.Reset();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(3));
  timer_.Stop();

  // The task is stopped.
  ASSERT_FALSE(timer_.IsFirstTimerRunning());
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(60));
  ASSERT_EQ(1, task_count_);
}

TEST_F(RequestTimerTest, StopWithoutStart) {
  // Timer is able to be stopped without being started.
  timer_.Stop();
  ASSERT_FALSE(timer_.IsFirstTimerRunning());
  ASSERT_FALSE(timer_.IsRepeatTimerRunning());
}

}  // namespace enterprise_reporting
