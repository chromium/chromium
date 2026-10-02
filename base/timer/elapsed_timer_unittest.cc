// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/elapsed_timer.h"

#include <concepts>
#include <optional>

#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

constexpr TimeDelta kSleepDuration = Milliseconds(20);
}

static_assert(std::movable<ElapsedTimer>);
static_assert(std::copyable<ElapsedTimer>);

static_assert(std::movable<ElapsedThreadTimer>);
static_assert(std::copyable<ElapsedThreadTimer>);

static_assert(std::movable<ElapsedLiveTimer>);
static_assert(std::copyable<ElapsedLiveTimer>);

static_assert(std::movable<ElapsedNoSleepTimer>);
static_assert(std::copyable<ElapsedNoSleepTimer>);

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

TEST(ElapsedTimerTest, ElapsedWithNowOut) {
  ElapsedTimer timer;

  PlatformThread::Sleep(kSleepDuration);
  TimeTicks now1;
  const TimeDelta elapsed1 = timer.Elapsed(&now1);
  EXPECT_GE(elapsed1, kSleepDuration);
  EXPECT_EQ(now1 - timer.start_time(), elapsed1);

  // Can call |Elapsed()| multiple times.
  PlatformThread::Sleep(kSleepDuration);
  TimeTicks now2;
  const TimeDelta elapsed2 = timer.Elapsed(&now2);
  EXPECT_GE(elapsed2, 2 * kSleepDuration);
  EXPECT_EQ(now2 - timer.start_time(), elapsed2);
  EXPECT_GT(now2, now1);
}

TEST(ElapsedTimerTest, MockedWithNowOut) {
  ScopedMockElapsedTimersForTest mock_elapsed_timer;

  ElapsedTimer timer;
  TimeTicks now;
  EXPECT_EQ(timer.Elapsed(&now),
            ScopedMockElapsedTimersForTest::kMockElapsedTime);
  EXPECT_EQ(now, timer.start_time() +
                     ScopedMockElapsedTimersForTest::kMockElapsedTime);

  // Real-time doesn't matter.
  PlatformThread::Sleep(kSleepDuration);
  TimeTicks now2;
  EXPECT_EQ(timer.Elapsed(&now2),
            ScopedMockElapsedTimersForTest::kMockElapsedTime);
  EXPECT_EQ(now2, timer.start_time() +
                      ScopedMockElapsedTimersForTest::kMockElapsedTime);
}

class ElapsedThreadTimerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    if (ThreadTicks::IsSupported()) {
      ThreadTicks::WaitUntilInitialized();
    }
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
  if (!ThreadTicks::IsSupported()) {
    return;
  }

  ElapsedThreadTimer timer;
  EXPECT_TRUE(timer.is_supported());

  // 1ms of work.
  constexpr TimeDelta kLoopingTime = Milliseconds(1);
  const ThreadTicks start_ticks = ThreadTicks::Now();
  while (ThreadTicks::Now() - start_ticks < kLoopingTime) {
  }

  EXPECT_GE(timer.Elapsed(), kLoopingTime);
}

TEST_F(ElapsedThreadTimerTest, DoesNotCountSleep) {
  if (!ThreadTicks::IsSupported()) {
    return;
  }

  ElapsedThreadTimer timer;
  EXPECT_TRUE(timer.is_supported());

  PlatformThread::Sleep(kSleepDuration);
  // Sleep time is not accounted for.
  EXPECT_LT(timer.Elapsed(), kSleepDuration);
}

TEST_F(ElapsedThreadTimerTest, Mocked) {
  if (!ThreadTicks::IsSupported()) {
    return;
  }

  ScopedMockElapsedTimersForTest mock_elapsed_timer;

  ElapsedThreadTimer timer;
  EXPECT_EQ(timer.Elapsed(), ScopedMockElapsedTimersForTest::kMockElapsedTime);

  // Real-time doesn't matter.
  PlatformThread::Sleep(kSleepDuration);
  EXPECT_EQ(timer.Elapsed(), ScopedMockElapsedTimersForTest::kMockElapsedTime);
}

TEST_F(ElapsedThreadTimerTest, ElapsedWithNowOut) {
  if (!ThreadTicks::IsSupported()) {
    ElapsedThreadTimer timer;
    ThreadTicks now;
    EXPECT_EQ(timer.Elapsed(&now), TimeDelta());
    EXPECT_TRUE(now.is_null());
    return;
  }

  ElapsedThreadTimer timer;
  EXPECT_TRUE(timer.is_supported());

  // 1ms of work.
  constexpr TimeDelta kLoopingTime = Milliseconds(1);
  const ThreadTicks start_ticks = ThreadTicks::Now();
  while (ThreadTicks::Now() - start_ticks < kLoopingTime) {
  }

  ThreadTicks now;
  const TimeDelta elapsed = timer.Elapsed(&now);
  EXPECT_GE(elapsed, kLoopingTime);
  EXPECT_EQ(now - timer.start_time(), elapsed);
}

TEST_F(ElapsedThreadTimerTest, MockedWithNowOut) {
  if (!ThreadTicks::IsSupported()) {
    return;
  }

  ScopedMockElapsedTimersForTest mock_elapsed_timer;

  ElapsedThreadTimer timer;
  ThreadTicks now;
  EXPECT_EQ(timer.Elapsed(&now),
            ScopedMockElapsedTimersForTest::kMockElapsedTime);
  EXPECT_EQ(now, timer.start_time() +
                     ScopedMockElapsedTimersForTest::kMockElapsedTime);

  // Real-time doesn't matter.
  PlatformThread::Sleep(kSleepDuration);
  ThreadTicks now2;
  EXPECT_EQ(timer.Elapsed(&now2),
            ScopedMockElapsedTimersForTest::kMockElapsedTime);
  EXPECT_EQ(now2, timer.start_time() +
                      ScopedMockElapsedTimersForTest::kMockElapsedTime);
}

TEST(ElapsedLiveTimerTest, Simple) {
  ElapsedLiveTimer timer;

  PlatformThread::Sleep(kSleepDuration);
  EXPECT_GE(timer.Elapsed(), kSleepDuration);

  // Can call |Elapsed()| multiple times.
  PlatformThread::Sleep(kSleepDuration);
  EXPECT_GE(timer.Elapsed(), 2 * kSleepDuration);
}

TEST(ElapsedLiveTimerTest, Mocked) {
  ScopedMockElapsedTimersForTest mock_elapsed_timer;

  ElapsedLiveTimer timer;
  EXPECT_EQ(timer.Elapsed(), ScopedMockElapsedTimersForTest::kMockElapsedTime);

  // Real-time doesn't matter.
  PlatformThread::Sleep(kSleepDuration);
  EXPECT_EQ(timer.Elapsed(), ScopedMockElapsedTimersForTest::kMockElapsedTime);
}

TEST(ElapsedLiveTimerTest, ElapsedWithNowOut) {
  ElapsedLiveTimer timer;

  PlatformThread::Sleep(kSleepDuration);
  LiveTicks now1;
  const TimeDelta elapsed1 = timer.Elapsed(&now1);
  EXPECT_GE(elapsed1, kSleepDuration);
  EXPECT_EQ(now1 - timer.start_time(), elapsed1);

  // Can call |Elapsed()| multiple times.
  PlatformThread::Sleep(kSleepDuration);
  LiveTicks now2;
  const TimeDelta elapsed2 = timer.Elapsed(&now2);
  EXPECT_GE(elapsed2, 2 * kSleepDuration);
  EXPECT_EQ(now2 - timer.start_time(), elapsed2);
  EXPECT_GT(now2, now1);
}

TEST(ElapsedLiveTimerTest, MockedWithNowOut) {
  ScopedMockElapsedTimersForTest mock_elapsed_timer;

  ElapsedLiveTimer timer;
  LiveTicks now;
  EXPECT_EQ(timer.Elapsed(&now),
            ScopedMockElapsedTimersForTest::kMockElapsedTime);
  EXPECT_EQ(now, timer.start_time() +
                     ScopedMockElapsedTimersForTest::kMockElapsedTime);

  // Real-time doesn't matter.
  PlatformThread::Sleep(kSleepDuration);
  LiveTicks now2;
  EXPECT_EQ(timer.Elapsed(&now2),
            ScopedMockElapsedTimersForTest::kMockElapsedTime);
  EXPECT_EQ(now2, timer.start_time() +
                      ScopedMockElapsedTimersForTest::kMockElapsedTime);
}

class ElapsedNoSleepTimerTest : public ::testing::Test {
 protected:
  test::TaskEnvironment task_environment_{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ElapsedNoSleepTimerTest, Mocked) {
  ScopedMockElapsedTimersForTest mock_elapsed_timer;

  ElapsedNoSleepTimer timer;
  EXPECT_EQ(timer.Elapsed(), ScopedMockElapsedTimersForTest::kMockElapsedTime);

  // Real-time doesn't matter.
  task_environment_.AdvanceClock(kSleepDuration);
  EXPECT_EQ(timer.Elapsed(), ScopedMockElapsedTimersForTest::kMockElapsedTime);
}

TEST_F(ElapsedNoSleepTimerTest, MockedWithNowOut) {
  ScopedMockElapsedTimersForTest mock_elapsed_timer;

  ElapsedNoSleepTimer timer;
  LiveTicks now;
  EXPECT_EQ(timer.Elapsed(&now),
            ScopedMockElapsedTimersForTest::kMockElapsedTime);
  EXPECT_EQ(now, timer.start_time() +
                     ScopedMockElapsedTimersForTest::kMockElapsedTime);

  // Real-time doesn't matter.
  task_environment_.AdvanceClock(kSleepDuration);
  LiveTicks now2;
  EXPECT_EQ(timer.Elapsed(&now2),
            ScopedMockElapsedTimersForTest::kMockElapsedTime);
  EXPECT_EQ(now2, timer.start_time() +
                      ScopedMockElapsedTimersForTest::kMockElapsedTime);
}

TEST_F(ElapsedNoSleepTimerTest, ElapsedWithNowOut) {
  ElapsedNoSleepTimer timer;
  task_environment_.AdvanceClock(Seconds(2));

  LiveTicks now;
  std::optional<TimeDelta> elapsed = timer.Elapsed(&now);
  ASSERT_TRUE(elapsed.has_value());
  EXPECT_EQ(*elapsed, Seconds(2));
  EXPECT_EQ(now - timer.start_time(), Seconds(2));
  EXPECT_EQ(now, task_environment_.NowLiveTicks());
}

TEST_F(ElapsedNoSleepTimerTest, ReturnsElapsedWhenClocksAgree) {
  ElapsedNoSleepTimer timer;
  task_environment_.AdvanceClock(Seconds(2));

  std::optional<TimeDelta> elapsed = timer.Elapsed();
  ASSERT_TRUE(elapsed.has_value());
  EXPECT_EQ(*elapsed, Seconds(2));
}

TEST_F(ElapsedNoSleepTimerTest, FastForwardByAdvancesTimer) {
  ElapsedNoSleepTimer timer;
  task_environment_.FastForwardBy(Seconds(5));

  std::optional<TimeDelta> elapsed = timer.Elapsed();
  ASSERT_TRUE(elapsed.has_value());
  EXPECT_EQ(*elapsed, Seconds(5));
}

TEST_F(ElapsedNoSleepTimerTest, ReturnsNulloptWhenClocksDivergeDuringSleep) {
  ElapsedNoSleepTimer timer;
  // AdvanceClock simulates normal awake time.
  task_environment_.AdvanceClock(Seconds(1));
  // SuspendedAdvanceClock simulates system suspension where LiveTicks pauses
  // while RealTicks continues advancing.
  task_environment_.SuspendedAdvanceClock(Seconds(9));

  EXPECT_EQ(timer.Elapsed(), std::nullopt);
}

TEST_F(ElapsedNoSleepTimerTest, SuspendedFastForwardByDetectsSleep) {
  ElapsedNoSleepTimer timer;
  task_environment_.SuspendedFastForwardBy(Seconds(5));

  EXPECT_EQ(timer.Elapsed(), std::nullopt);
}

TEST_F(ElapsedNoSleepTimerTest, MultipleCallsToElapsed) {
  ElapsedNoSleepTimer timer;
  task_environment_.AdvanceClock(Seconds(1));
  EXPECT_EQ(timer.Elapsed(), Seconds(1));

  task_environment_.AdvanceClock(Seconds(2));
  EXPECT_EQ(timer.Elapsed(), Seconds(3));

  // Simulating sleep invalidates the interval for this timer.
  task_environment_.SuspendedAdvanceClock(Seconds(1));
  EXPECT_EQ(timer.Elapsed(), std::nullopt);

  // Subsequent Elapsed() calls still return nullopt because the sleep occurred
  // since timer creation.
  task_environment_.AdvanceClock(Seconds(1));
  EXPECT_EQ(timer.Elapsed(), std::nullopt);

  // A new timer created after sleep behaves normally.
  ElapsedNoSleepTimer timer2;
  task_environment_.AdvanceClock(Seconds(2));
  EXPECT_EQ(timer2.Elapsed(), Seconds(2));
}

TEST_F(ElapsedNoSleepTimerTest, StartTimeMatchesLiveTicks) {
  ElapsedNoSleepTimer timer;
  EXPECT_EQ(timer.start_time(), task_environment_.NowLiveTicks());

  task_environment_.AdvanceClock(Seconds(3));
  ElapsedNoSleepTimer timer2;
  EXPECT_EQ(timer2.start_time(), task_environment_.NowLiveTicks());
  EXPECT_EQ(timer2.start_time() - timer.start_time(), Seconds(3));
}

class ElapsedNoSleepTimerSamplingMarginTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_live_ticks_ = LiveTicks() + Microseconds(1000000);
    mock_real_ticks_ = time_internal::RealTicks() + Microseconds(2000000);
    inter_clock_read_duration_ = TimeDelta();
  }

  static LiveTicks MockLiveTicksNow() { return mock_live_ticks_; }
  static time_internal::RealTicks MockRealTicksNow() {
    mock_live_ticks_ += inter_clock_read_duration_;
    return mock_real_ticks_;
  }

  void AdvanceLiveTicks(TimeDelta delta) { mock_live_ticks_ += delta; }
  void AdvanceRealTicks(TimeDelta delta) { mock_real_ticks_ += delta; }

  void AdvanceBothClocks(TimeDelta delta) {
    AdvanceLiveTicks(delta);
    AdvanceRealTicks(delta);
  }

  static LiveTicks mock_live_ticks_;
  static time_internal::RealTicks mock_real_ticks_;
  static TimeDelta inter_clock_read_duration_;

  subtle::ScopedTimeClockOverrides time_overrides_{
      /*time_override=*/nullptr,
      /*time_ticks_override=*/nullptr,
      /*thread_ticks_override=*/nullptr,
      &ElapsedNoSleepTimerSamplingMarginTest::MockLiveTicksNow,
      /*time_ticks_low_resolution_override=*/nullptr,
      &ElapsedNoSleepTimerSamplingMarginTest::MockRealTicksNow};
};

// static
LiveTicks ElapsedNoSleepTimerSamplingMarginTest::mock_live_ticks_ =
    LiveTicks() + Microseconds(1000000);
// static
time_internal::RealTicks
    ElapsedNoSleepTimerSamplingMarginTest::mock_real_ticks_ =
        time_internal::RealTicks() + Microseconds(2000000);
// static
TimeDelta ElapsedNoSleepTimerSamplingMarginTest::inter_clock_read_duration_ =
    TimeDelta();

TEST_F(ElapsedNoSleepTimerSamplingMarginTest,
       ReturnsPrimaryElapsedWithinSamplingErrorMargin) {
  // Simulate 2 µs elapsed between reading LiveTicks and RealTicks during clock
  // sampling.
  inter_clock_read_duration_ = Microseconds(2);

  ElapsedNoSleepTimer timer;
  AdvanceBothClocks(Seconds(1));

  // With a 2 µs read duration at start and 2 µs at end, the dynamic max error
  // is 4 µs. A clock discrepancy within this margin must be tolerated.
  std::optional<TimeDelta> elapsed = timer.Elapsed();
  ASSERT_TRUE(elapsed.has_value());
  EXPECT_EQ(*elapsed, Seconds(1) + Microseconds(2));
}

TEST_F(ElapsedNoSleepTimerSamplingMarginTest,
       ReturnsNulloptWhenDivergenceExceedsSamplingErrorMargin) {
  // Simulate 2 µs elapsed between reading LiveTicks and RealTicks during clock
  // sampling.
  inter_clock_read_duration_ = Microseconds(2);

  ElapsedNoSleepTimer timer;
  AdvanceLiveTicks(Seconds(1));
  // Advance RealTicks beyond the 4 µs dynamic error margin (start: 2 µs, end:
  // 2 µs) to simulate sleep.
  AdvanceRealTicks(Seconds(1) + Microseconds(10));

  EXPECT_EQ(timer.Elapsed(), std::nullopt);
}

TEST_F(ElapsedNoSleepTimerSamplingMarginTest,
       ReturnsNulloptWhenSamplingErrorIsExcessive) {
  // Simulate excessive read duration exceeding allowed threshold (e.g. thread
  // preemption during clock sampling across all retry attempts).
  inter_clock_read_duration_ =
      ElapsedNoSleepTimer::kMaxAllowedSamplingError + Microseconds(1);

  ElapsedNoSleepTimer timer;
  AdvanceBothClocks(Seconds(1));

  // Even though no sleep occurred, sampling uncertainty was too large for
  // reliable sleep detection.
  EXPECT_EQ(timer.Elapsed(), std::nullopt);
}

// Disabled on Android x86: In the 32-bit x86 emulator clock read latency
// occasionally exceeds kMaxAllowedSamplingError.
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_X86)
#define MAYBE_ReturnsValidElapsedWithoutSleep \
  DISABLED_ReturnsValidElapsedWithoutSleep
#else
#define MAYBE_ReturnsValidElapsedWithoutSleep ReturnsValidElapsedWithoutSleep
#endif
TEST(ElapsedNoSleepTimerRealClockTest, MAYBE_ReturnsValidElapsedWithoutSleep) {
  ElapsedNoSleepTimer timer;
  std::optional<TimeDelta> elapsed = timer.Elapsed();
  ASSERT_TRUE(elapsed.has_value());
  EXPECT_GE(*elapsed, TimeDelta());
}

}  // namespace base
