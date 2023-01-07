// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_metrics_service.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/session_manager/core/session_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kStartTime[] = "1 Jan 2020 21:00";

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

class FamilyUserMetricsServiceTestObserver
    : public FamilyUserMetricsService::Observer {
 public:
  MOCK_METHOD(void, OnNewDay, (), (override));
};

// Tests for family user metrics service.
class FamilyUserMetricsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromString(kStartTime, &start_time));
    base::TimeDelta forward_by = start_time - base::Time::Now();
    EXPECT_LT(base::TimeDelta(), forward_by);
    task_environment_.AdvanceClock(forward_by);

    chromeos::PowerManagerClient::InitializeFake();
    family_user_metrics_service_ =
        std::make_unique<FamilyUserMetricsService>(&testing_profile_);

    family_user_metrics_service_->AddObserver(&mock_observer_);
  }

  void TearDown() override {
    family_user_metrics_service_->Shutdown();
    family_user_metrics_service_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable* GetPrefService() {
    return testing_profile_.GetTestingPrefService();
  }

  int GetDayIdPref() {
    return GetPrefService()->GetInteger(prefs::kFamilyUserMetricsDayId);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  FamilyUserMetricsServiceTestObserver mock_observer_;

 private:
  // We need this member variable, even if it's unused, so
  // FamilyUserSessionMetrics doesn't crash.
  session_manager::SessionManager session_manager_;
  TestingProfile testing_profile_;
  std::unique_ptr<FamilyUserMetricsService> family_user_metrics_service_;
};

using DetectingNewDayTest = FamilyUserMetricsServiceTest;

// Tests OnNewDay() is called after more than one day passes.
TEST_F(DetectingNewDayTest, MoreThanOneDay) {
  EXPECT_CALL(mock_observer_, OnNewDay()).Times(1);
  task_environment_.FastForwardBy(base::Days(1) + base::Hours(1));
  EXPECT_EQ(FamilyUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called at midnight.
TEST_F(DetectingNewDayTest, UntilMidnight) {
  EXPECT_CALL(mock_observer_, OnNewDay()).Times(1);
  task_environment_.FastForwardBy(base::Hours(3));
  EXPECT_EQ(FamilyUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is not called before midnight.
TEST_F(DetectingNewDayTest, LessThanOneDay) {
  EXPECT_CALL(mock_observer_, OnNewDay()).Times(0);
  task_environment_.FastForwardBy(base::Hours(1));
  EXPECT_EQ(FamilyUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called after one day passes, even when the device is
// idle.
TEST_F(DetectingNewDayTest, MoreThanOneDayDeviceIdle) {
  EXPECT_CALL(mock_observer_, OnNewDay()).Times(1);
  SetScreenOff(true);
  SetSuspendImminent();
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_EQ(FamilyUserMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

}  // namespace ash
