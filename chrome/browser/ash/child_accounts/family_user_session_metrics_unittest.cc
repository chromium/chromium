// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/family_user_session_metrics.h"

#include <memory>

#include "base/logging.h"
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

namespace ash {

namespace {

constexpr base::TimeDelta kTenMinutes = base::Minutes(10);
constexpr base::TimeDelta kOneHour = base::Hours(1);
constexpr base::TimeDelta kOneDay = base::Days(1);

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

void CancelSuspend() {
  chromeos::FakePowerManagerClient::Get()->SendSuspendDone();
}

}  // namespace

class FamilyUserSessionMetricsTest : public testing::Test {
 protected:
  FamilyUserSessionMetricsTest() = default;
  ~FamilyUserSessionMetricsTest() override = default;

  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    InitiateFamilyUserSessionMetrics();
    FamilyUserSessionMetrics::RegisterProfilePrefs(pref_service_.registry());
  }

  void TearDown() override {
    DestructFamilyUserSessionMetrics();
    chromeos::PowerManagerClient::Shutdown();
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

  void SetSessionState(session_manager::SessionState state) {
    session_manager_.SetSessionState(state);
  }

  void SetActiveSessionStartTime(base::Time time) {
    family_user_session_metrics_->SetActiveSessionStartForTesting(time);
  }

  session_manager::SessionState GetSessionState() {
    return session_manager_.session_state();
  }

  void OnNewDay() { family_user_session_metrics_->OnNewDay(); }

  PrefService* pref_service() { return &pref_service_; }

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

  SetupTaskRunnerWithTime("1 Jan 2020 23:00");

  SetSessionState(session_manager::SessionState::ACTIVE);

  task_environment_.FastForwardBy(kTenMinutes);
  // Session locked at 23:10:00.
  SetSessionState(session_manager::SessionState::LOCKED);

  task_environment_.FastForwardBy(kOneHour);
  // Session activated at 00:10:00 on 2 Jan 2020.
  SetSessionState(session_manager::SessionState::ACTIVE);

  OnNewDay();

  task_environment_.FastForwardBy(kOneHour);
  // Session locked at 01:10:00 on 2 Jan 2020.
  SetSessionState(session_manager::SessionState::LOCKED);

  // Engagement start metric result:
  EXPECT_EQ(2,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  for (int i = 0; i <= 23; i++) {
    if (i == 0 || i == 1 || i == 23) {
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, i,
          1);
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, i, 1);
    } else {
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, i,
          0);
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, i, 0);
    }
  }

  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 3);

  // Duration metric result:
  histogram_tester.ExpectUniqueTimeSample(
      FamilyUserSessionMetrics::kSessionEngagementDurationHistogramName,
      kTenMinutes, 1);
  EXPECT_EQ(kOneHour, pref_service()->GetTimeDelta(
                          prefs::kFamilyUserMetricsSessionEngagementDuration));
}

TEST_F(FamilyUserSessionMetricsTest, ScreenStateChange) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  SetupTaskRunnerWithTime("3 Jan 2020 23:10");

  SetSessionState(session_manager::SessionState::ACTIVE);
  task_environment_.FastForwardBy(kOneHour);

  // Test screen off after midnight at 0:10:00 on 4 Jan 2020.
  SetScreenOff(true);

  OnNewDay();

  // Engagement start metric result:
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  // Engagement Hour metric result:
  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 23, 1);
  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekendHistogramName, 0, 1);
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 2);

  // Duration metric result:
  histogram_tester.ExpectUniqueTimeSample(
      FamilyUserSessionMetrics::kSessionEngagementDurationHistogramName,
      kOneHour, 1);
  EXPECT_EQ(base::TimeDelta(),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsSessionEngagementDuration));

  // Test screen on on 4 Jan 2020 0:10:00.
  SetScreenOff(false);

  task_environment_.FastForwardBy(base::Hours(25));
  // Test screen off on 5 Jan 2020 1:10:00.
  SetScreenOff(true);

  OnNewDay();

  // Engagement start metric result:
  EXPECT_EQ(2,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  // Engagement Hour metric result:
  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 23, 1);
  for (int i = 0; i <= 23; i++) {
    if (i == 0) {
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementWeekendHistogramName, i,
          3);
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, i, 3);
    } else if (i == 1) {
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementWeekendHistogramName, i,
          2);
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, i, 2);
    } else if (i == 23) {
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementWeekendHistogramName, i,
          1);
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, i, 2);

    } else {
      histogram_tester.ExpectBucketCount(
          FamilyUserSessionMetrics::kSessionEngagementWeekendHistogramName, i,
          1);
    }
  }

  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 28);

  // Duration metric result:
  histogram_tester.ExpectTimeBucketCount(
      FamilyUserSessionMetrics::kSessionEngagementDurationHistogramName,
      kOneHour, 1);
  histogram_tester.ExpectTimeBucketCount(
      FamilyUserSessionMetrics::kSessionEngagementDurationHistogramName,
      kOneDay, 1);
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementDurationHistogramName, 2);
  EXPECT_EQ(base::TimeDelta(),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsSessionEngagementDuration));
}

TEST_F(FamilyUserSessionMetricsTest, SuspendStateChange) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  SetupTaskRunnerWithTime("4 Jan 2020 6:00");

  SetSessionState(session_manager::SessionState::ACTIVE);
  task_environment_.FastForwardBy(kTenMinutes);

  // Test suspend at 6:10:00.
  SetSuspendImminent();
  task_environment_.FastForwardBy(kTenMinutes);

  // Test cancel at 6:20:00.
  CancelSuspend();

  task_environment_.FastForwardBy(kTenMinutes);

  // Test suspend at 6:30:00.
  SetSuspendImminent();
  task_environment_.FastForwardBy(kTenMinutes);

  // Session locked at 6:40:00.
  SetSessionState(session_manager::SessionState::LOCKED);

  // Engagement start metric result:
  EXPECT_EQ(2,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  // Engagement Hour metric result:
  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekendHistogramName, 6, 2);
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 2);

  // Duration metric result:
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementDurationHistogramName, 0);
  EXPECT_EQ(base::Minutes(20),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsSessionEngagementDuration));
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
  SetActiveSessionStartTime(mock_session_start);

  // Session locked at 10:00:00.
  SetSessionState(session_manager::SessionState::LOCKED);

  // Engagement start metric result:
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  // Engagement Hour metric result:
  // Engagement hour and duration data will be ignored if start time > end time.
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 0);
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 0);

  // Duration metric result:
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementDurationHistogramName, 0);
  EXPECT_EQ(base::TimeDelta(),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsSessionEngagementDuration));
}

// Tests destroying FamilyUserSessionMetrics without invoking
// OnUsageTimeStateChange(). It may happens during shutdown of device.
TEST_F(FamilyUserSessionMetricsTest,
       DestructionAndCreationOfFamilyUserSessionMetrics) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  SetupTaskRunnerWithTime("1 Jan 2020 10:00");

  SetSessionState(session_manager::SessionState::ACTIVE);

  task_environment_.FastForwardBy(kTenMinutes);

  DestructFamilyUserSessionMetrics();
  SetSessionState(session_manager::SessionState::UNKNOWN);

  // Engagement start metric result:
  EXPECT_EQ(1,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  // Engagement Hour metric result:
  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 10, 1);

  // Duration metric result:
  histogram_tester.ExpectTotalCount(
      prefs::kFamilyUserMetricsSessionEngagementDuration, 0);
  EXPECT_EQ(kTenMinutes,
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsSessionEngagementDuration));

  // Test restart.
  InitiateFamilyUserSessionMetrics();
  EXPECT_NE(session_manager::SessionState::ACTIVE, GetSessionState());
  SetSessionState(session_manager::SessionState::ACTIVE);

  // Engagement start metric result:
  EXPECT_EQ(2,
            user_action_tester.GetActionCount(
                FamilyUserSessionMetrics::kSessionEngagementStartActionName));

  task_environment_.FastForwardBy(kTenMinutes);
  SetSessionState(session_manager::SessionState::LOCKED);

  // Engagement Hour metric result:
  histogram_tester.ExpectUniqueSample(
      FamilyUserSessionMetrics::kSessionEngagementWeekdayHistogramName, 10, 2);
  histogram_tester.ExpectTotalCount(
      FamilyUserSessionMetrics::kSessionEngagementTotalHistogramName, 2);

  // Duration metric result:
  histogram_tester.ExpectTotalCount(
      prefs::kFamilyUserMetricsSessionEngagementDuration, 0);
  EXPECT_EQ(base::Minutes(20),
            pref_service()->GetTimeDelta(
                prefs::kFamilyUserMetricsSessionEngagementDuration));
}

}  // namespace ash
