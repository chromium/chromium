// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/elapsed_timer.h"

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

constexpr TimeDelta kSleepDuration = TimeDelta::FromMilliseconds(20);

}

TEST(ElapsedTimerTest, Simple) {
  ElapsedTimer timer;

  PlatformThread::Sleep(kSleepDuration);
  EXPECT_GE(timer.Elapsed(), kSleepDuration);

  // Can call |Elapsed()| multiple times.
  PlatformThread::Sleep(kSleepDuration);
  EXPECT_GE(timer.Elapsed(), 2 * kSleepDuration);
}

TEST(ElapsedTimerTest, Mocked) {
  ScopedMockElapsedTimersForTest mock_elapsed_timer;

  ElapsedTimer timer;
  EXPECT_EQ(timer.Elapsed(), ScopedMockElapsedTimersForTest::kMockElapsedTime);

  // Real-time doesn't matter.
  PlatformThread::Sleep(kSleepDuration);
  EXPECT_EQ(timer.Elapsed(), ScopedMockElapsedTimersForTest::kMockElapsedTime);
}

class ElapsedThreadTimerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (ThreadTicks::IsSupported())
      ThreadTicks::WaitUntilInitialized();
  }
};

TEST_F(ElapsedThreadTimerTest, IsSupported) {
  ElapsedThreadTimer timer;
  if (!ThreadTicks::IsSupported()) {
    EXPECT_FALSE(timer.is_supported());
    EXPECT_EQ(TimeDelta(), timer.Elapsed());
  } else {
    EXPECT_TRUE(timer.is_supported());
  }
}

TEST_F(ElapsedThreadTimerTest, Simple) {
  if (!ThreadTicks::IsSupported())
    return;

  ElapsedThreadTimer timer;
  EXPECT_TRUE(timer.is_supported());

  // 1ms of work.
  constexpr TimeDelta kLoopingTime = TimeDelta::FromMilliseconds(1);
  const ThreadTicks start_ticks = ThreadTicks::Now();
  while (ThreadTicks::Now() - start_ticks < kLoopingTime) {
  }

  EXPECT_GE(timer.Elapsed(), kLoopingTime);
}

TEST_F(ElapsedThreadTimerTest, DoesNotCountSleep) {
  if (!ThreadTicks::IsSupported())
    return;

  ElapsedThreadTimer timer;
  EXPECT_TRUE(timer.is_supported());

  PlatformThread::Sleep(kSleepDuration);
  // Sleep time is not accounted for.
  EXPECT_LT(timer.Elapsed(), kSleepDuration);
}

TEST_F(ElapsedThreadTimerTest, Mocked) {
  if (!ThreadTicks::IsSupported())
    return;

  ScopedMockElapsedTimersForTest mock_elapsed_timer;

  ElapsedThreadTimer timer;
  EXPECT_EQ(timer.Elapsed(), ScopedMockElapsedTimersForTest::kMockElapsedTime);

  // Real-time doesn't matter.
  PlatformThread::Sleep(kSleepDuration);
  EXPECT_EQ(timer.Elapsed(), ScopedMockElapsedTimersForTest::kMockElapsedTime);
}

}  // namespace base
