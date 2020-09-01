// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/family_user_session_metrics.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

void SetScreenOff(bool is_screen_off) {
  power_manager::ScreenIdleState screen_idle_state;
  screen_idle_state.set_off(is_screen_off);
  FakePowerManagerClient::Get()->SendScreenIdleStateChanged(screen_idle_state);
}

void SetSuspendImminent() {
  FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
}

void CancelSuspend() {
  FakePowerManagerClient::Get()->SendSuspendDone();
}

}  // namespace

class FamilyUserSessionMetricsTest : public testing::Test {
 protected:
  FamilyUserSessionMetricsTest() = default;
  ~FamilyUserSessionMetricsTest() override = default;

  void SetUp() override {
    PowerManagerClient::InitializeFake();
    InitiateFamilyUserSessionMetrics();
    FamilyUserSessionMetrics::RegisterProfilePrefs(pref_service_.registry());
  }

  void TearDown() override {
    DestructFamilyUserSessionMetrics();
    PowerManagerClient::Shutdown();
  }

  void DestructFamilyUserSessionMetrics() {
    family_user_session_metrics_.reset();
  }

  void InitiateFamilyUserSessionMetrics() {
    family_user_session_metrics_ =
        std::make_unique<FamilyUserSessionMetrics>(&pref_service_);
  }

  void SetupTaskRunnerWithTime(const std::string& start_time_str) {
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromString(start_time_str.c_str(), &start_time));
    base::TimeDelta forward_by = start_time - base::Time::Now();
    EXPECT_LT(base::TimeDelta(), forward_by);
    task_environment_.FastForwardBy(forward_by);
  }

  void SetSessionEngagementStartPref(base::Time start) {
    pref_service_.SetTime(prefs::kFamilyUserMetricsSessionEngagementStartTime,
                          start);
  }

  void SetSessionState(session_manager::SessionState state) {
    session_manager_.SetSessionState(state);
  }

  session_manager::SessionState GetSessionState() {
    return session_manager_.session_state();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  session_manager::SessionManager session_manager_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<FamilyUserSessionMetrics> family_user_session_metrics_;
};

TEST_F(FamilyUserSessionMetricsTest, SessionStateChange) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Should see 0 in user session metrics at first.
  EXPECT_EQ(0,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  SetupTaskRunnerWithTime("1 Jan 2020 10:00");

  SetSessionState(session_manager::SessionState::ACTIVE);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));

  // Session locked at 10:10:00.
  SetSessionState(session_manager::SessionState::LOCKED);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));

  // Session activated at 10:20:00.
  SetSessionState(session_manager::SessionState::ACTIVE);

  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));

  // Session locked at 10:20:00 on the second day.
  SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_EQ(2,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  histogram_tester.ExpectBucketCount(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 10, 3);

  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 26);
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 26);
}

TEST_F(FamilyUserSessionMetricsTest, ScreenStateChange) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  SetupTaskRunnerWithTime("3 Jan 2020 23:00");

  SetSessionState(session_manager::SessionState::ACTIVE);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(60));

  // Test screen off at 0:00:00.
  SetScreenOff(true);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));

  // Test screen on at 0:10:00.
  SetScreenOff(false);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));

  // Session locked at 0:20:00.
  SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_EQ(2,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 23, 1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekendHistogramName, 0, 2);

  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 3);
}

TEST_F(FamilyUserSessionMetricsTest, SuspendStateChange) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  SetupTaskRunnerWithTime("4 Jan 2020 6:00");

  SetSessionState(session_manager::SessionState::ACTIVE);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));

  // Test suspend at 6:10:00.
  SetSuspendImminent();
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));

  // Test cancel at 6:20:00.
  CancelSuspend();

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));

  // Test suspend at 6:30:00.
  SetSuspendImminent();
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));

  // Session locked at 6:40:00.
  SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_EQ(2,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekendHistogramName, 6, 2);
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 2);
}

TEST_F(FamilyUserSessionMetricsTest, ClockBackward) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  SetupTaskRunnerWithTime("1 Jan 2020 10:00");

  SetSessionState(session_manager::SessionState::ACTIVE);

  base::Time mock_session_start;
  ASSERT_TRUE(base::Time::FromString("1 Jan 2020 11:00", &mock_session_start));

  // Set session start prefs to 11:00:00. Mock a state that start time > end
  // time.
  SetSessionEngagementStartPref(mock_session_start);

  // Session locked at 10:00:00.
  SetSessionState(session_manager::SessionState::LOCKED);

  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  // Engagement hour data will be ignored if start time > end time.
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 0);
}

// Tests destroying FamilyUserSessionMetrics without invoking
// OnUsageTimeStateChange(). It may happens during shutdown of device.
TEST_F(FamilyUserSessionMetricsTest,
       DestructionAndCreationOfFamilyUserSessionMetrics) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  SetupTaskRunnerWithTime("1 Jan 2020 10:00");

  SetSessionState(session_manager::SessionState::ACTIVE);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(1));

  DestructFamilyUserSessionMetrics();
  SetSessionState(session_manager::SessionState::UNKNOWN);

  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));
  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 10, 1);

  // Test restart.
  InitiateFamilyUserSessionMetrics();
  EXPECT_NE(session_manager::SessionState::ACTIVE, GetSessionState());
  SetSessionState(session_manager::SessionState::ACTIVE);

  EXPECT_EQ(2,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(1));
  SetSessionState(session_manager::SessionState::LOCKED);

  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 10, 2);
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 2);
}

}  // namespace chromeos
