// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_platform_metrics_service.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

constexpr char kStartTime[] = "1 Jan 2021 21:00";

void SetScreenOff(bool is_screen_off) {
  power_manager::ScreenIdleState screen_idle_state;
  screen_idle_state.set_off(is_screen_off);
  chromeos::FakePowerManagerClient::Get()->SendScreenIdleStateChanged(
      screen_idle_state);
}

void SetSuspendImminent() {
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
}

}  // namespace

class AppPlatformMetricsServiceTestObserver
    : public AppPlatformMetricsService::Observer {
 public:
  MOCK_METHOD(void, OnNewDay, (), (override));
};

// Tests for family user metrics service.
class AppPlatformMetricsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromString(kStartTime, &start_time));
    base::TimeDelta forward_by = start_time - base::Time::Now();
    EXPECT_LT(base::TimeDelta(), forward_by);
    task_environment_.AdvanceClock(forward_by);

    chromeos::PowerManagerClient::InitializeFake();
    app_platform_metrics_service_ =
        std::make_unique<AppPlatformMetricsService>(&testing_profile_);

    app_platform_metrics_service_->AddObserver(&mock_observer_);
    app_platform_metrics_service_->Start();
  }

  void TearDown() override {
    app_platform_metrics_service_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable* GetPrefService() {
    return testing_profile_.GetTestingPrefService();
  }

  int GetDayIdPref() {
    return GetPrefService()->GetInteger(kAppPlatformMetricsDayId);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  AppPlatformMetricsServiceTestObserver mock_observer_;

 private:
  TestingProfile testing_profile_;
  std::unique_ptr<AppPlatformMetricsService> app_platform_metrics_service_;
};

// Tests OnNewDay() is called after more than one day passes.
TEST_F(AppPlatformMetricsServiceTest, MoreThanOneDay) {
  EXPECT_CALL(mock_observer_, OnNewDay()).Times(1);
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1) +
                                  base::TimeDelta::FromHours(1));
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called at midnight.
TEST_F(AppPlatformMetricsServiceTest, UntilMidnight) {
  EXPECT_CALL(mock_observer_, OnNewDay()).Times(1);
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(3));
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is not called before midnight.
TEST_F(AppPlatformMetricsServiceTest, LessThanOneDay) {
  EXPECT_CALL(mock_observer_, OnNewDay()).Times(0);
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called after one day passes, even when the device is
// idle.
TEST_F(AppPlatformMetricsServiceTest, MoreThanOneDayDeviceIdle) {
  EXPECT_CALL(mock_observer_, OnNewDay()).Times(1);
  SetScreenOff(true);
  SetSuspendImminent();
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

}  // namespace apps
