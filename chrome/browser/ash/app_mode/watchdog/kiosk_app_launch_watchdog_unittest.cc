// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/watchdog/kiosk_app_launch_watchdog.h"

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using Action = KioskAppLaunchWatchdog::Action;

constexpr base::TimeDelta kTimeout = base::Seconds(60);
constexpr int kMaxOsReboots = 3;

class KioskAppLaunchWatchdogTest : public testing::Test {
 public:
  KioskAppLaunchWatchdogTest() {
    local_state_.registry()->RegisterDictionaryPref(
        KioskChromeAppManager::kKioskDictionaryName);
  }

  std::unique_ptr<KioskAppLaunchWatchdog> CreateWatchdog() {
    reboot_future_ = std::make_unique<base::test::TestFuture<void>>();
    return std::make_unique<KioskAppLaunchWatchdog>(
        local_state_, reboot_future_->GetCallback());
  }

  bool IsRebootRequested() const { return reboot_future_->IsReady(); }

  // Arms a fresh watchdog and lets it time out. Returns whether a reboot was
  // requested.
  bool ArmAndTimeOut(base::TimeDelta timeout = kTimeout) {
    auto watchdog = CreateWatchdog();
    task_environment_.FastForwardBy(timeout);
    return IsRebootRequested();
  }

  int PersistedTimeouts() {
    return local_state_.GetDict(KioskChromeAppManager::kKioskDictionaryName)
        .FindInt(KioskAppLaunchWatchdog::kConsecutiveTimeoutsKey)
        .value_or(0);
  }

 protected:
  base::test::ScopedFeatureList feature_list_{
      features::kKioskAppLaunchWatchdog};
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<base::test::TestFuture<void>> reboot_future_;
};

TEST_F(KioskAppLaunchWatchdogTest, DoesNotFireBeforeTimeout) {
  auto watchdog = CreateWatchdog();

  task_environment_.FastForwardBy(kTimeout - base::Seconds(1));
  EXPECT_FALSE(IsRebootRequested());
}

TEST_F(KioskAppLaunchWatchdogTest, RebootsDeviceOnFirstTimeout) {
  base::HistogramTester histograms;

  EXPECT_TRUE(ArmAndTimeOut());
  EXPECT_EQ(PersistedTimeouts(), 1);
  histograms.ExpectUniqueSample("Kiosk.Launch.WatchdogAction",
                                Action::kRebootDevice, 1);
}

TEST_F(KioskAppLaunchWatchdogTest, StopsRebootingAfterMaxOsReboots) {
  base::HistogramTester histograms;

  for (int i = 0; i < kMaxOsReboots; ++i) {
    EXPECT_TRUE(ArmAndTimeOut()) << "attempt " << i;
    EXPECT_EQ(PersistedTimeouts(), i + 1);
  }

  // 4th timeout: reboot budget is exhausted, no further reboots.
  EXPECT_FALSE(ArmAndTimeOut());
  EXPECT_EQ(PersistedTimeouts(), kMaxOsReboots + 1);

  // Subsequent timeouts also do not reboot.
  EXPECT_FALSE(ArmAndTimeOut());
  EXPECT_EQ(PersistedTimeouts(), kMaxOsReboots + 2);

  histograms.ExpectBucketCount("Kiosk.Launch.WatchdogAction",
                               Action::kRebootDevice, kMaxOsReboots);
  histograms.ExpectBucketCount("Kiosk.Launch.WatchdogAction",
                               Action::kRebootsExhausted, 2);
}

TEST_F(KioskAppLaunchWatchdogTest, FullResetStopsTimerAndResetsCounter) {
  EXPECT_TRUE(ArmAndTimeOut());
  ASSERT_EQ(PersistedTimeouts(), 1);

  auto watchdog = CreateWatchdog();
  watchdog->ClearRebootCounter();
  watchdog.reset();
  EXPECT_EQ(PersistedTimeouts(), 0);

  task_environment_.FastForwardBy(kTimeout);
  EXPECT_FALSE(IsRebootRequested());
}

TEST_F(KioskAppLaunchWatchdogTest, DestructionStopsTimerButKeepsCounter) {
  EXPECT_TRUE(ArmAndTimeOut());
  ASSERT_EQ(PersistedTimeouts(), 1);

  auto watchdog = CreateWatchdog();
  watchdog.reset();
  EXPECT_EQ(PersistedTimeouts(), 1);

  task_environment_.FastForwardBy(kTimeout);
  EXPECT_FALSE(IsRebootRequested());
}

TEST_F(KioskAppLaunchWatchdogTest, RespectsCustomTimeoutAndMaxReboots) {
  constexpr base::TimeDelta kCustomTimeout = base::Seconds(30);
  base::test::ScopedFeatureList custom_feature_list;
  custom_feature_list.InitAndEnableFeatureWithParameters(
      features::kKioskAppLaunchWatchdog,
      {{features::kKioskAppLaunchWatchdogTimeout.name, "30s"},
       {features::kKioskAppLaunchWatchdogMaxOsReboots.name, "1"}});

  EXPECT_TRUE(ArmAndTimeOut(kCustomTimeout));
  EXPECT_EQ(PersistedTimeouts(), 1);

  EXPECT_FALSE(ArmAndTimeOut(kCustomTimeout));
  EXPECT_EQ(PersistedTimeouts(), 2);
}

}  // namespace

}  // namespace ash
