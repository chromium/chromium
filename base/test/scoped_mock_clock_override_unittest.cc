// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_mock_clock_override.h"

#include "base/build_time.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Disabled on Android due to flakes; see https://crbug.com/1474884.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_Time DISABLED_Time
#else
#define MAYBE_Time Time
#endif
TEST(ScopedMockClockOverrideTest, MAYBE_Time) {
  // Choose a reference time that we know to be in the past but close to now.
  Time build_time = GetBuildTime();

  // Override is not active. All Now() methods should return a time greater than
  // the build time.
  EXPECT_LT(build_time, Time::Now());
  EXPECT_GT(Time::Max(), Time::Now());
  EXPECT_LT(build_time, Time::NowFromSystemTime());
  EXPECT_GT(Time::Max(), Time::NowFromSystemTime());

  {
    // Set override.
    ScopedMockClockOverride mock_clock;

    EXPECT_NE(Time(), Time::Now());
    Time start = Time::Now();
    mock_clock.Advance(Seconds(1));
    EXPECT_EQ(start + Seconds(1), Time::Now());
  }

  // All methods return real time again.
  EXPECT_LT(build_time, Time::Now());
  EXPECT_GT(Time::Max(), Time::Now());
  EXPECT_LT(build_time, Time::NowFromSystemTime());
  EXPECT_GT(Time::Max(), Time::NowFromSystemTime());
}

TEST(ScopedMockClockOverrideTest, TimeTicks) {
  // Override is not active. All Now() methods should return a sensible value.
  EXPECT_LT(TimeTicks::UnixEpoch(), TimeTicks::Now());
  EXPECT_GT(TimeTicks::Max(), TimeTicks::Now());
  EXPECT_LT(TimeTicks::UnixEpoch() + Days(365), TimeTicks::Now());

  {
    // Set override.
    ScopedMockClockOverride mock_clock;

    EXPECT_NE(TimeTicks(), TimeTicks::Now());
    TimeTicks start = TimeTicks::Now();
    mock_clock.Advance(Seconds(1));
    EXPECT_EQ(start + Seconds(1), TimeTicks::Now());
  }

  // All methods return real ticks again.
  EXPECT_LT(TimeTicks::UnixEpoch(), TimeTicks::Now());
  EXPECT_GT(TimeTicks::Max(), TimeTicks::Now());
  EXPECT_LT(TimeTicks::UnixEpoch() + Days(365), TimeTicks::Now());
}

TEST(ScopedMockClockOverrideTest, ThreadTicks) {
  if (ThreadTicks::IsSupported()) {
    ThreadTicks::WaitUntilInitialized();

    // Override is not active. All Now() methods should return a sensible value.
    ThreadTicks initial_thread_ticks = ThreadTicks::Now();
    EXPECT_LE(initial_thread_ticks, ThreadTicks::Now());
    EXPECT_GT(ThreadTicks::Max(), ThreadTicks::Now());
    EXPECT_LT(ThreadTicks(), ThreadTicks::Now());

    {
      // Set override.
      ScopedMockClockOverride mock_clock;

      EXPECT_NE(ThreadTicks(), ThreadTicks::Now());
      ThreadTicks start = ThreadTicks::Now();
      mock_clock.Advance(Seconds(1));
      EXPECT_EQ(start + Seconds(1), ThreadTicks::Now());
    }

    // All methods return real ticks again.
    EXPECT_LE(initial_thread_ticks, ThreadTicks::Now());
    EXPECT_GT(ThreadTicks::Max(), ThreadTicks::Now());
    EXPECT_LT(ThreadTicks(), ThreadTicks::Now());
  }
}

}  // namespace

}  // namespace base
