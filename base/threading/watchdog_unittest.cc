// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/watchdog.h"

#include <atomic>

#include "base/logging.h"
#include "base/test/spin_wait.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

//------------------------------------------------------------------------------
// Provide a derived class to facilitate testing.

class WatchdogCounter : public Watchdog::Delegate {
 public:
  WatchdogCounter(const TimeDelta& duration,
                  const std::string& thread_watched_name,
                  bool enabled)
      : watchdog_(duration, thread_watched_name, enabled, this) {}

  WatchdogCounter(const WatchdogCounter&) = delete;
  WatchdogCounter& operator=(const WatchdogCounter&) = delete;

  ~WatchdogCounter() override = default;

  void Alarm() override {
    alarm_counter_++;
    watchdog_.DefaultAlarm();
  }

  Watchdog& watchdog() { return watchdog_; }
  int alarm_counter() { return alarm_counter_.load(); }

 private:
  std::atomic<int> alarm_counter_{0};
  Watchdog watchdog_;
};

class WatchdogTest : public testing::Test {
 public:
  void SetUp() override { Watchdog::ResetStaticData(); }
};

}  // namespace

//------------------------------------------------------------------------------
// Actual tests

// Minimal constructor/destructor test.
TEST_F(WatchdogTest, StartupShutdownTest) {
  Watchdog watchdog1(Milliseconds(300), "Disabled", false);
  Watchdog watchdog2(Milliseconds(300), "Enabled", true);
}

// Test ability to call Arm and Disarm repeatedly.
TEST_F(WatchdogTest, ArmDisarmTest) {
  Watchdog watchdog1(Milliseconds(300), "Disabled", false);
  watchdog1.Arm();
  watchdog1.Disarm();
  watchdog1.Arm();
  watchdog1.Disarm();

  Watchdog watchdog2(Milliseconds(300), "Enabled", true);
  watchdog2.Arm();
  watchdog2.Disarm();
  watchdog2.Arm();
  watchdog2.Disarm();
}

// Make sure a basic alarm fires when the time has expired.
TEST_F(WatchdogTest, AlarmTest) {
  WatchdogCounter watchdog(Milliseconds(10), "Enabled", true);
  watchdog.watchdog().Arm();
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(Minutes(5), watchdog.alarm_counter() > 0);
  EXPECT_EQ(1, watchdog.alarm_counter());
}

// Make sure a basic alarm fires when the time has expired.
TEST_F(WatchdogTest, AlarmPriorTimeTest) {
  WatchdogCounter watchdog(TimeDelta(), "Enabled2", true);
  // Set a time in the past.
  watchdog.watchdog().ArmSomeTimeDeltaAgo(Seconds(2));
  // It should instantly go off, but certainly in less than 5 minutes.
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(Minutes(5), watchdog.alarm_counter() > 0);

  EXPECT_EQ(1, watchdog.alarm_counter());
}

// Make sure a disable alarm does nothing, even if we arm it.
TEST_F(WatchdogTest, ConstructorDisabledTest) {
  WatchdogCounter watchdog(Milliseconds(10), "Disabled", false);
  watchdog.watchdog().Arm();
  // Alarm should not fire, as it was disabled.
  PlatformThread::Sleep(Milliseconds(500));
  EXPECT_EQ(0, watchdog.alarm_counter());
}

// Make sure Disarming will prevent firing, even after Arming.
TEST_F(WatchdogTest, DisarmTest) {
  WatchdogCounter watchdog(Seconds(1), "Enabled3", true);

  TimeTicks start = TimeTicks::Now();
  watchdog.watchdog().Arm();
  // Sleep a bit, but not past the alarm point.
  PlatformThread::Sleep(Milliseconds(100));
  watchdog.watchdog().Disarm();
  TimeTicks end = TimeTicks::Now();

  if (end - start > Milliseconds(500)) {
    LOG(WARNING) << "100ms sleep took over 500ms, making the results of this "
                 << "timing-sensitive test suspicious.  Aborting now.";
    return;
  }

  // Alarm should not have fired before it was disarmed.
  EXPECT_EQ(0, watchdog.alarm_counter());

  // Sleep past the point where it would have fired if it wasn't disarmed,
  // and verify that it didn't fire.
  PlatformThread::Sleep(Seconds(1));
  EXPECT_EQ(0, watchdog.alarm_counter());

  // ...but even after disarming, we can still use the alarm...
  // Set a time greater than the timeout into the past.
  watchdog.watchdog().ArmSomeTimeDeltaAgo(Seconds(10));
  // It should almost instantly go off, but certainly in less than 5 minutes.
  SPIN_FOR_TIMEDELTA_OR_UNTIL_TRUE(Minutes(5), watchdog.alarm_counter() > 0);

  EXPECT_EQ(1, watchdog.alarm_counter());
}

}  // namespace base
