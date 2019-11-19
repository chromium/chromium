// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/lap_timer.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

// This file contains a minimal unit test for LapTimer, used for benchmarking.
// This file is supposed to match closely with the example code, documented in
// lap_timer.h. Please update that documentation if you need to change things.

namespace base {

namespace test {

namespace {

constexpr base::TimeDelta kTimeLimit = base::TimeDelta::FromMilliseconds(15);
constexpr base::TimeDelta kTimeAdvance = base::TimeDelta::FromMilliseconds(1);
constexpr int kWarmupRuns = 5;
constexpr int kTimeCheckInterval = 10;

}  // namespace

TEST(LapTimer, UsageExample) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);

  LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);

  EXPECT_FALSE(timer.HasTimeLimitExpired());
  EXPECT_FALSE(timer.IsWarmedUp());

  do {
    task_environment.FastForwardBy(kTimeAdvance);
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  EXPECT_NEAR(timer.LapsPerSecond(), 1000, 0.1);
  EXPECT_NEAR(timer.TimePerLap().InMillisecondsF(), 1.0f, 0.1);
  // Output number of laps is 20, because the warm up runs are ignored and the
  // timer is only checked every kTimeInterval laps.
  EXPECT_EQ(timer.NumLaps(), 20);

  EXPECT_TRUE(timer.HasTimeLimitExpired());
  EXPECT_TRUE(timer.IsWarmedUp());
}

#if !defined(OS_IOS)
// iOS simulator does not support using ThreadTicks.
TEST(LapTimer, ThreadTicksUsageExample) {
  TaskEnvironment task_environment(TaskEnvironment::TimeSource::MOCK_TIME);
  LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval,
                 LapTimer::TimerMethod::kUseThreadTicks);

  EXPECT_FALSE(timer.HasTimeLimitExpired());
  EXPECT_FALSE(timer.IsWarmedUp());

  do {
    task_environment.FastForwardBy(kTimeAdvance);
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  // Because advancing the TaskEnvironment time won't affect the
  // ThreadTicks, laps will be much faster than the regular UsageExample.
  EXPECT_GT(timer.LapsPerSecond(), 1000);
  EXPECT_LT(timer.TimePerLap().InMillisecondsF(), 1.0f);
  EXPECT_GT(timer.NumLaps(), 20);

  EXPECT_TRUE(timer.HasTimeLimitExpired());
  EXPECT_TRUE(timer.IsWarmedUp());
}
#endif

}  // namespace test
}  // namespace base
