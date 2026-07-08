// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_controller.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_test_policy_builder.h"
#include "chrome/browser/ash/app_mode/auto_sleep/weekly_interval_timer.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using policy::WeeklyTimeInterval;
using WeeklyTimeIntervals = std::vector<std::unique_ptr<WeeklyTimeInterval>>;

using DayOfWeek = DeviceWeeklyScheduledSuspendTestPolicyBuilder::DayOfWeek;

enum class TestUserType { kKiosk, kMgs, kRegular, kNotLoggedIn };

// Simulates a login into the given `TestUserType` and verifies the
// `DeviceWeeklyScheduledSuspend` policy.
class DeviceWeeklyScheduledSuspendControllerTest
    : public testing::TestWithParam<std::tuple<bool, TestUserType>> {
 protected:
  DeviceWeeklyScheduledSuspendControllerTest() = default;

  bool IsEnabledInMgs() const { return std::get<0>(GetParam()); }
  TestUserType GetTestUserType() const { return std::get<1>(GetParam()); }

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        ash::features::kDeviceWeeklyScheduledSuspendMgs, IsEnabledInMgs());
    test_user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(
            TestingBrowserProcess::GetGlobal()->local_state());

    // Add all potential users before starting any session.
    ASSERT_TRUE(test_user_session_manager_->AddRegularUser(
        AccountId::FromUserEmailGaiaId("user@example.com",
                                       GaiaId("1234567890"))));
    ASSERT_TRUE(test_user_session_manager_->AddPublicAccountUser(
        policy::GenerateDeviceLocalAccountUserId(
            "mgs", policy::DeviceLocalAccountType::kPublicSession)));
    ASSERT_TRUE(test_user_session_manager_->AddKioskWebAppUser(
        policy::GenerateDeviceLocalAccountUserId(
            "kiosk", policy::DeviceLocalAccountType::kWebKioskApp)));

    chromeos::PowerManagerClient::InitializeFake();
    InitController();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(
        task_environment_.GetMockTickClock());
    device_weekly_scheduled_suspend_controller_
        ->SetWeeklyIntervalTimerFactoryForTesting(
            std::make_unique<WeeklyIntervalTimer::Factory>(
                task_environment_.GetMockClock(),
                task_environment_.GetMockTickClock()));
    user_activity_calls_ = 0;
    chromeos::FakePowerManagerClient::Get()->set_user_activity_callback(
        base::BindRepeating([](int& count) { ++count; },
                            std::ref(user_activity_calls_)));

    LoginUser(GetTestUserType());
  }

  void TearDown() override {
    // Clear the policy so that timers can be cleaned up before shutting
    // down the fake power manager.
    UpdatePolicyPref({});
    device_weekly_scheduled_suspend_controller_.reset();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(nullptr);
    chromeos::FakePowerManagerClient::Get()->set_user_activity_callback(
        base::NullCallback());
    chromeos::PowerManagerClient::Shutdown();
    test_user_session_manager_.reset();
  }

  void LoginUser(TestUserType user_type) {
    AccountId account_id;
    switch (user_type) {
      case TestUserType::kRegular: {
        account_id = AccountId::FromUserEmailGaiaId("user@example.com",
                                                    GaiaId("1234567890"));
        break;
      }
      case TestUserType::kMgs: {
        account_id =
            AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
                "mgs", policy::DeviceLocalAccountType::kPublicSession));
        break;
      }
      case TestUserType::kKiosk: {
        account_id =
            AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
                "kiosk", policy::DeviceLocalAccountType::kWebKioskApp));
        break;
      }
      case TestUserType::kNotLoggedIn: {
        session_manager::SessionManager::Get()->SetSessionState(
            session_manager::SessionState::LOGIN_PRIMARY);
        return;
      }
    }

    ASSERT_FALSE(account_id.empty());
    test_user_session_manager_->LogIn(account_id);
    if (!session_manager::SessionManager::Get()->GetActiveSession() ||
        session_manager::SessionManager::Get()
                ->GetActiveSession()
                ->account_id() != account_id) {
      session_manager::SessionManager::Get()->SwitchActiveSession(account_id);
    }
  }

  void UpdatePolicyPref(base::ListValue schedule_list) {
    TestingBrowserProcess::GetGlobal()->local_state()->SetList(
        ash::prefs::kDeviceWeeklyScheduledSuspend, std::move(schedule_list));
  }

  bool IsPolicyActive() const {
    switch (GetTestUserType()) {
      case TestUserType::kKiosk:
        return true;
      case TestUserType::kMgs:
      case TestUserType::kNotLoggedIn:
        return IsEnabledInMgs();
      case TestUserType::kRegular:
        return false;
    }
  }

  void UpdatePolicyAndCheckIntervals(
      const DeviceWeeklyScheduledSuspendTestPolicyBuilder& policy_builder) {
    UpdatePolicyPref(policy_builder.GetAsPrefValue());
    if (IsPolicyActive()) {
      CheckIntervalsInController(policy_builder.GetAsWeeklyTimeIntervals());
    } else {
      CheckIntervalsInController({});
    }
  }

  void CheckIntervalsInController(
      const WeeklyTimeIntervals& expected_intervals) {
    const WeeklyIntervalTimers& interval_timers =
        device_weekly_scheduled_suspend_controller_
            ->GetWeeklyIntervalTimersForTesting();
    ASSERT_EQ(expected_intervals.size(), interval_timers.size());
    for (size_t i = 0; i < expected_intervals.size(); ++i) {
      ASSERT_TRUE(interval_timers[i]);
      EXPECT_EQ(*expected_intervals[i], interval_timers[i]->time_interval());
    }
  }

  void FastForwardTimeTo(const policy::WeeklyTime& future_weekly_time,
                         base::TimeDelta dt = base::TimeDelta()) {
    base::Time current_time = task_environment_.GetMockClock()->Now();
    policy::WeeklyTime current_weekly_time =
        policy::WeeklyTime::GetLocalWeeklyTime(current_time);

    auto delta_to_future =
        current_weekly_time.GetDurationTo(future_weekly_time);
    auto delta_to_future_plus_dt = delta_to_future + dt;
    ASSERT_TRUE(delta_to_future_plus_dt.is_positive());
    task_environment_.FastForwardBy(delta_to_future_plus_dt);
  }

  DeviceWeeklyScheduledSuspendController*
  device_weekly_scheduled_suspend_controller() {
    return device_weekly_scheduled_suspend_controller_.get();
  }

  void InitController() {
    device_weekly_scheduled_suspend_controller_ =
        std::make_unique<DeviceWeeklyScheduledSuspendController>(
            TestingBrowserProcess::GetGlobal()->local_state());
    device_weekly_scheduled_suspend_controller_->InitUserManagerObservation(
        user_manager::UserManager::Get());
    device_weekly_scheduled_suspend_controller_->InitSessionObservation();
  }

  int user_activity_calls() const { return user_activity_calls_; }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<ash::test::TestUserSessionManager> test_user_session_manager_;
  std::unique_ptr<DeviceWeeklyScheduledSuspendController>
      device_weekly_scheduled_suspend_controller_;
  int user_activity_calls_;
};

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       HandlesEmptyIntervalsWithNoPolicy) {
  WeeklyTimeIntervals empty_intervals;
  CheckIntervalsInController(empty_intervals);
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest, HandlesEmptyPolicy) {
  DeviceWeeklyScheduledSuspendTestPolicyBuilder empty_policy;
  UpdatePolicyAndCheckIntervals(empty_policy);
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       HandlesValidPolicyWithMultipleIntervals) {
  UpdatePolicyAndCheckIntervals(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(12),
                                    DayOfWeek::WEDNESDAY, base::Hours(15))
          .AddWeeklySuspendInterval(DayOfWeek::FRIDAY, base::Hours(20),
                                    DayOfWeek::MONDAY, base::Hours(8)));
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       UpdatesIntervalsOnPolicyChange) {
  UpdatePolicyAndCheckIntervals(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(12),
                                    DayOfWeek::WEDNESDAY, base::Hours(15))
          .AddWeeklySuspendInterval(DayOfWeek::FRIDAY, base::Hours(20),
                                    DayOfWeek::MONDAY, base::Hours(8)));

  UpdatePolicyAndCheckIntervals(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::FRIDAY, base::Hours(20), DayOfWeek::MONDAY,
          base::Hours(8)));
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       ClearsIntervalsOnEmptyPolicy) {
  UpdatePolicyAndCheckIntervals(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(12),
                                    DayOfWeek::WEDNESDAY, base::Hours(15))
          .AddWeeklySuspendInterval(DayOfWeek::FRIDAY, base::Hours(20),
                                    DayOfWeek::MONDAY, base::Hours(8)));

  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().GetAsPrefValue());
  CheckIntervalsInController({});
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       HandlesInvalidPolicyWithMissingStartTime) {
  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddInvalidScheduleMissingStart(DayOfWeek::FRIDAY, base::Hours(6))
          .GetAsPrefValue());

  CheckIntervalsInController({});
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       HandlesInvalidPolicyWithMissingEndTime) {
  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddInvalidScheduleMissingEnd(DayOfWeek::MONDAY, base::Hours(20))
          .GetAsPrefValue());

  CheckIntervalsInController({});
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       HandlesInvalidPolicyWithOverlappingIntervals) {
  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::FRIDAY, base::Hours(20),
                                    DayOfWeek::SATURDAY, base::Hours(15))
          .AddWeeklySuspendInterval(DayOfWeek::SATURDAY, base::Hours(6),
                                    DayOfWeek::SUNDAY, base::Hours(20))
          .GetAsPrefValue());

  CheckIntervalsInController({});
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       DoesNotCallSuspendWhenOutsideOfInterval) {
  if (!IsPolicyActive()) {
    return;
  }
  auto builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::FRIDAY, base::Hours(0), DayOfWeek::FRIDAY, base::Hours(9));
  auto intervals = builder.GetAsWeeklyTimeIntervals();
  FastForwardTimeTo(intervals[0]->start(), -base::Hours(5));
  UpdatePolicyPref(builder.GetAsPrefValue());
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 0);
  FastForwardTimeTo(intervals[0]->start(), -base::Hours(1));
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 0);
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       CallsSuspendOnIntervalStartEveryWeek) {
  if (!IsPolicyActive()) {
    return;
  }
  auto builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::MONDAY, base::Hours(21), DayOfWeek::TUESDAY,
          base::Hours(7));
  auto intervals = builder.GetAsWeeklyTimeIntervals();
  FastForwardTimeTo(intervals[0]->start(), -base::Minutes(5));
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 0);
  UpdatePolicyPref(builder.GetAsPrefValue());
  EXPECT_EQ(device_weekly_scheduled_suspend_controller()
                ->GetWeeklyIntervalTimersForTesting()
                .size(),
            1u);
  FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 1);
  FastForwardTimeTo(intervals[0]->end());
  FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 2);
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       CallsSuspendForEverySuspendInterval) {
  if (!IsPolicyActive()) {
    return;
  }
  auto builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::MONDAY, base::Hours(0),
                                    DayOfWeek::MONDAY, base::Hours(9))
          .AddWeeklySuspendInterval(DayOfWeek::TUESDAY, base::Hours(0),
                                    DayOfWeek::TUESDAY, base::Hours(9))
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(0),
                                    DayOfWeek::WEDNESDAY, base::Hours(9));
  auto intervals = builder.GetAsWeeklyTimeIntervals();
  FastForwardTimeTo(intervals[0]->start(), -base::Minutes(5));
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 0);
  UpdatePolicyPref(builder.GetAsPrefValue());
  int expected_suspend_calls = 0;
  for (auto& interval : intervals) {
    FastForwardTimeTo(interval->start());
    EXPECT_EQ(
        chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(),
        ++expected_suspend_calls);
    FastForwardTimeTo(interval->end());
  }
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       DeviceFullyResumesWhenIntervalEnds) {
  if (!IsPolicyActive()) {
    return;
  }
  auto builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::THURSDAY, base::Hours(21), DayOfWeek::FRIDAY,
          base::Hours(7));
  auto intervals = builder.GetAsWeeklyTimeIntervals();
  UpdatePolicyPref(builder.GetAsPrefValue());

  // Assert the device is not sleeping before the interval starts.
  FastForwardTimeTo(intervals[0]->start(), -base::Seconds(5));
  ASSERT_EQ(user_activity_calls(), 0);
  ASSERT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 0);

  // Assert the device sleeps when the interval starts.
  FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(user_activity_calls(), 0);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 1);

  // Device should fully resume (as signaled by user activity) when the interval
  // ends.
  FastForwardTimeTo(intervals[0]->end());
  chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  EXPECT_EQ(user_activity_calls(), 1);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 1);
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       DeviceDoesNotFullyResumeBeforeIntervalEnds) {
  if (!IsPolicyActive()) {
    return;
  }
  auto builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::THURSDAY, base::Hours(21), DayOfWeek::FRIDAY,
          base::Hours(7));
  auto intervals = builder.GetAsWeeklyTimeIntervals();
  UpdatePolicyPref(builder.GetAsPrefValue());

  // Assert the device is not sleeping before the interval starts.
  FastForwardTimeTo(intervals[0]->start(), -base::Seconds(5));
  ASSERT_EQ(user_activity_calls(), 0);
  ASSERT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 0);

  // Assert the device sleeps when the interval starts.
  FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(user_activity_calls(), 0);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 1);

  // Dark resumes should not trigger a full resume before the interval ends.
  FastForwardTimeTo(policy::WeeklyTime(DayOfWeek::FRIDAY, 0, std::nullopt));
  chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  EXPECT_EQ(user_activity_calls(), 0);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 1);

  FastForwardTimeTo(intervals[0]->end(), -base::Seconds(5));
  chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  EXPECT_EQ(user_activity_calls(), 0);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 1);

  // Device should fully resume (as signaled by user activity) when the interval
  // ends.
  FastForwardTimeTo(intervals[0]->end());
  chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  EXPECT_EQ(user_activity_calls(), 1);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 1);
}

TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
       ClearsIntervalsWhenSessionEnds) {
  if (!IsPolicyActive()) {
    return;
  }
  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(12),
                                    DayOfWeek::WEDNESDAY, base::Hours(15))
          .GetAsPrefValue());
  EXPECT_EQ(device_weekly_scheduled_suspend_controller()
                ->GetWeeklyIntervalTimersForTesting()
                .size(),
            1u);

  // Simulate session end by changing user type to regular user.
  LoginUser(TestUserType::kRegular);
  CheckIntervalsInController({});
}

// Simulates a login into the given `TestUserType` and verifies the
// `DeviceWeeklyScheduledSuspend` policy behavior when the PowerManager
// service availability changes.
class DeviceWeeklyScheduledSuspendControllerPowerServiceTest
    : public DeviceWeeklyScheduledSuspendControllerTest {
 public:
  // testing::Test:
  void SetUp() override {
    DeviceWeeklyScheduledSuspendControllerTest::SetUp();
    chromeos::FakePowerManagerClient::Get()->SetServiceAvailability(false);
    // Recreate the controller after changing service availability to apply it.
    InitController();
  }
};

TEST_P(DeviceWeeklyScheduledSuspendControllerPowerServiceTest,
       PolicyNotSetWhenPowerServiceIsNotAvailable) {
  if (!IsPolicyActive()) {
    return;
  }
  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(12),
                                    DayOfWeek::WEDNESDAY, base::Hours(15))
          .AddWeeklySuspendInterval(DayOfWeek::FRIDAY, base::Hours(20),
                                    DayOfWeek::MONDAY, base::Hours(8))
          .GetAsPrefValue());
  CheckIntervalsInController({});
}

TEST_P(DeviceWeeklyScheduledSuspendControllerPowerServiceTest,
       PolicyGetsSetWhenPowerManagerIsAvailable) {
  if (!IsPolicyActive()) {
    return;
  }
  auto policy_builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(12),
                                    DayOfWeek::WEDNESDAY, base::Hours(15))
          .AddWeeklySuspendInterval(DayOfWeek::FRIDAY, base::Hours(20),
                                    DayOfWeek::MONDAY, base::Hours(8));
  UpdatePolicyPref(policy_builder.GetAsPrefValue());
  CheckIntervalsInController({});
  chromeos::FakePowerManagerClient::Get()->SetServiceAvailability(true);
  CheckIntervalsInController(policy_builder.GetAsWeeklyTimeIntervals());
}

// Simulates a login into the login screen and verifies the
// `DeviceWeeklyScheduledSuspend` policy.
using DeviceWeeklyScheduledSuspendControllerLoginScreenTest =
    DeviceWeeklyScheduledSuspendControllerTest;

TEST_P(DeviceWeeklyScheduledSuspendControllerLoginScreenTest,
       AppliesPolicyOnLoginScreen) {
  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(12),
                                    DayOfWeek::WEDNESDAY, base::Hours(15))
          .GetAsPrefValue());
  EXPECT_EQ(device_weekly_scheduled_suspend_controller()
                ->GetWeeklyIntervalTimersForTesting()
                .size(),
            IsEnabledInMgs() ? 1u : 0u);
}

// Simulates a login into a regular user session and verifies the
// `DeviceWeeklyScheduledSuspend` policy.
using DeviceWeeklyScheduledSuspendControllerRegularUserTest =
    DeviceWeeklyScheduledSuspendControllerTest;

TEST_P(DeviceWeeklyScheduledSuspendControllerRegularUserTest,
       DoesNotApplyPolicyForRegularUsers) {
  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(12),
                                    DayOfWeek::WEDNESDAY, base::Hours(15))
          .GetAsPrefValue());
  CheckIntervalsInController({});
}

INSTANTIATE_TEST_SUITE_P(All,
                         DeviceWeeklyScheduledSuspendControllerTest,
                         testing::Combine(testing::Bool(),
                                          testing::Values(TestUserType::kKiosk,
                                                          TestUserType::kMgs)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceWeeklyScheduledSuspendControllerLoginScreenTest,
    testing::Combine(testing::Bool(),
                     testing::Values(TestUserType::kNotLoggedIn)));

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceWeeklyScheduledSuspendControllerRegularUserTest,
    testing::Combine(testing::Bool(),
                     testing::Values(TestUserType::kRegular)));

INSTANTIATE_TEST_SUITE_P(All,
                         DeviceWeeklyScheduledSuspendControllerPowerServiceTest,
                         testing::Combine(testing::Bool(),
                                          testing::Values(TestUserType::kKiosk,
                                                          TestUserType::kMgs)));

}  // namespace ash
