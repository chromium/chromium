// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/wall_clock_timer.h"

#include <memory>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/power_monitor_test.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class WallClockTimerTest : public ::testing::Test {
 protected:
  // Fast-forwards virtual time by |delta|. If |with_power| is true, both
  // |clock_| and |task_environment_| time will be fast-forwarded. Otherwise,
  // only |clock_| time will be changed to mimic the behavior when machine is
  // suspended.
  // Power event will be triggered if |with_power| is set to false.
  void FastForwardBy(base::TimeDelta delay, bool with_power = true) {
    if (!with_power)
      fake_power_monitor_source_.Suspend();

    clock_.Advance(delay);

    if (with_power) {
      task_environment_.FastForwardBy(delay);
    } else {
      fake_power_monitor_source_.Resume();
      task_environment_.RunUntilIdle();
    }
  }

  base::test::ScopedPowerMonitorTestSource fake_power_monitor_source_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::SimpleTestClock clock_;
};

TEST_F(WallClockTimerTest, PowerResume) {
  ::testing::StrictMock<base::MockOnceClosure> callback;
  // Set up a WallClockTimer that will fire in one minute.
  WallClockTimer wall_clock_timer(&clock_,
                                  task_environment_.GetMockTickClock());
  constexpr auto delay = base::TimeDelta::FromMinutes(1);
  const auto start_time = base::Time::Now();
  const auto run_time = start_time + delay;
  clock_.SetNow(start_time);
  wall_clock_timer.Start(FROM_HERE, run_time, callback.Get());
  EXPECT_EQ(wall_clock_timer.desired_run_time(), start_time + delay);

  // Pretend that time jumps forward 30 seconds while the machine is suspended.
  constexpr auto past_time = base::TimeDelta::FromSeconds(30);
  FastForwardBy(past_time, /*with_power=*/false);
  // Ensure that the timer has not yet fired.
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  EXPECT_EQ(wall_clock_timer.desired_run_time(), start_time + delay);

  // Expect that the timer fires at the desired run time.
  EXPECT_CALL(callback, Run());
  // Both Time::Now() and |task_environment_| MockTickClock::Now()
  // go forward by (|delay| - |past_time|):
  FastForwardBy(delay - past_time);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  EXPECT_FALSE(wall_clock_timer.IsRunning());
}

TEST_F(WallClockTimerTest, UseTimerTwiceInRow) {
  ::testing::StrictMock<base::MockOnceClosure> first_callback;
  ::testing::StrictMock<base::MockOnceClosure> second_callback;
  const auto start_time = base::Time::Now();
  clock_.SetNow(start_time);

  // Set up a WallClockTimer that will invoke |first_callback| in one minute.
  // Once it's done, it will invoke |second_callback| after the other minute.
  WallClockTimer wall_clock_timer(&clock_,
                                  task_environment_.GetMockTickClock());
  constexpr auto delay = base::TimeDelta::FromMinutes(1);
  wall_clock_timer.Start(FROM_HERE, clock_.Now() + delay, first_callback.Get());
  EXPECT_CALL(first_callback, Run())
      .WillOnce(::testing::InvokeWithoutArgs(
          [this, &wall_clock_timer, &second_callback, delay]() {
            wall_clock_timer.Start(FROM_HERE, clock_.Now() + delay,
                                   second_callback.Get());
          }));

  FastForwardBy(delay);
  ::testing::Mock::VerifyAndClearExpectations(&first_callback);
  ::testing::Mock::VerifyAndClearExpectations(&second_callback);

  // When the |wall_clock_time| is used for the second time, it can still handle
  // power suspension properly.
  constexpr auto past_time = base::TimeDelta::FromSeconds(30);
  FastForwardBy(past_time, /*with_power=*/false);
  ::testing::Mock::VerifyAndClearExpectations(&second_callback);

  EXPECT_CALL(second_callback, Run());
  FastForwardBy(delay - past_time);
  ::testing::Mock::VerifyAndClearExpectations(&second_callback);
}

TEST_F(WallClockTimerTest, Stop) {
  ::testing::StrictMock<base::MockOnceClosure> callback;
  clock_.SetNow(base::Time::Now());

  // Set up a WallClockTimer.
  WallClockTimer wall_clock_timer(&clock_,
                                  task_environment_.GetMockTickClock());
  constexpr auto delay = base::TimeDelta::FromMinutes(1);
  wall_clock_timer.Start(FROM_HERE, clock_.Now() + delay, callback.Get());

  // After 20 seconds, timer is stopped.
  constexpr auto past_time = base::TimeDelta::FromSeconds(20);
  FastForwardBy(past_time);
  EXPECT_TRUE(wall_clock_timer.IsRunning());
  wall_clock_timer.Stop();
  EXPECT_FALSE(wall_clock_timer.IsRunning());

  // When power is suspends and resumed, timer won't be resumed.
  FastForwardBy(past_time, /*with_power=*/false);
  EXPECT_FALSE(wall_clock_timer.IsRunning());

  // Timer won't fire when desired run time is reached.
  FastForwardBy(delay - past_time * 2);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
}

TEST_F(WallClockTimerTest, RestartRunningTimer) {
  ::testing::StrictMock<base::MockOnceClosure> first_callback;
  ::testing::StrictMock<base::MockOnceClosure> second_callback;
  constexpr auto delay = base::TimeDelta::FromMinutes(1);

  // Set up a WallClockTimer that will invoke |first_callback| in one minute.
  clock_.SetNow(base::Time::Now());
  WallClockTimer wall_clock_timer(&clock_,
                                  task_environment_.GetMockTickClock());
  wall_clock_timer.Start(FROM_HERE, clock_.Now() + delay, first_callback.Get());

  // After 30 seconds, replace the timer with |second_callback| with new one
  // minute delay.
  constexpr auto past_time = delay / 2;
  FastForwardBy(past_time);
  wall_clock_timer.Start(FROM_HERE, clock_.Now() + delay,
                         second_callback.Get());

  // |first_callback| is due but it won't be called because it's replaced.
  FastForwardBy(past_time);
  ::testing::Mock::VerifyAndClearExpectations(&first_callback);
  ::testing::Mock::VerifyAndClearExpectations(&second_callback);

  // Timer invokes the |second_callback|.
  EXPECT_CALL(second_callback, Run());
  FastForwardBy(past_time);
  ::testing::Mock::VerifyAndClearExpectations(&first_callback);
  ::testing::Mock::VerifyAndClearExpectations(&second_callback);
}

TEST_F(WallClockTimerTest, DoubleStop) {
  ::testing::StrictMock<base::MockOnceClosure> callback;
  clock_.SetNow(base::Time::Now());

  // Set up a WallClockTimer.
  WallClockTimer wall_clock_timer(&clock_,
                                  task_environment_.GetMockTickClock());
  constexpr auto delay = base::TimeDelta::FromMinutes(1);
  wall_clock_timer.Start(FROM_HERE, clock_.Now() + delay, callback.Get());

  // After 15 seconds, timer is stopped.
  constexpr auto past_time = delay / 4;
  FastForwardBy(past_time);
  EXPECT_TRUE(wall_clock_timer.IsRunning());
  wall_clock_timer.Stop();
  EXPECT_FALSE(wall_clock_timer.IsRunning());

  // And timer is stopped again later. The second stop should be a no-op.
  FastForwardBy(past_time);
  EXPECT_FALSE(wall_clock_timer.IsRunning());
  wall_clock_timer.Stop();
  EXPECT_FALSE(wall_clock_timer.IsRunning());

  // Timer won't fire after stop.
  FastForwardBy(past_time, /*with_power=*/false);
  FastForwardBy(delay - past_time * 3);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
}

// On some platforms, TickClock will never freeze. WallClockTimer are still
// supported on those platforms.
TEST_F(WallClockTimerTest, NonStopTickClock) {
  ::testing::StrictMock<base::MockOnceClosure> callback;
  // Set up a WallClockTimer that will fire in one minute.
  WallClockTimer wall_clock_timer(&clock_,
                                  task_environment_.GetMockTickClock());
  constexpr auto delay = base::TimeDelta::FromMinutes(1);
  const auto start_time = base::Time::Now();
  const auto run_time = start_time + delay;
  clock_.SetNow(start_time);
  wall_clock_timer.Start(FROM_HERE, run_time, callback.Get());
  EXPECT_EQ(wall_clock_timer.desired_run_time(), start_time + delay);

  // Pretend that time jumps forward 30 seconds while the machine is suspended.
  constexpr auto past_time = base::TimeDelta::FromSeconds(30);

  // Fastword with both clocks even the power is suspended.
  fake_power_monitor_source_.Suspend();
  clock_.SetNow(clock_.Now() + past_time);
  task_environment_.FastForwardBy(past_time);
  fake_power_monitor_source_.Resume();

  // Ensure that the timer has not yet fired.
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  EXPECT_EQ(wall_clock_timer.desired_run_time(), start_time + delay);

  // Expect that the timer fires at the desired run time.
  EXPECT_CALL(callback, Run());
  // Both Time::Now() and |task_environment_| MockTickClock::Now()
  // go forward by (|delay| - |past_time|):
  FastForwardBy(delay - past_time);
  ::testing::Mock::VerifyAndClearExpectations(&callback);
  EXPECT_FALSE(wall_clock_timer.IsRunning());
}

TEST_F(WallClockTimerTest, NonStopTickClockWithLongPause) {
  ::testing::StrictMock<base::MockOnceClosure> callback;
  // Set up a WallClockTimer that will fire in one minute.
  WallClockTimer wall_clock_timer(&clock_,
                                  task_environment_.GetMockTickClock());
  constexpr auto delay = base::TimeDelta::FromMinutes(1);
  const auto start_time = base::Time::Now();
  const auto run_time = start_time + delay;
  clock_.SetNow(start_time);
  wall_clock_timer.Start(FROM_HERE, run_time, callback.Get());
  EXPECT_EQ(wall_clock_timer.desired_run_time(), start_time + delay);

  // Pretend that time jumps forward 60 seconds while the machine is suspended.
  constexpr auto past_time = base::TimeDelta::FromSeconds(60);

  // Fastword with both clocks even the power is suspended. Timer fires at the
  // moment of power resume.
  EXPECT_CALL(callback, Run());
  fake_power_monitor_source_.Suspend();
  clock_.SetNow(clock_.Now() + past_time);
  task_environment_.FastForwardBy(past_time);
  fake_power_monitor_source_.Resume();

  ::testing::Mock::VerifyAndClearExpectations(&callback);
  EXPECT_FALSE(wall_clock_timer.IsRunning());
}

}  // namespace base
