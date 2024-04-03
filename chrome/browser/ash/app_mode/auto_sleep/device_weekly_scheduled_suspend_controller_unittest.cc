// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_controller.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_test_policy_builder.h"
#include "chrome/browser/ash/app_mode/auto_sleep/fake_repeating_time_interval_task_executor.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using policy::WeeklyTimeInterval;
using WeeklyTimeIntervals = std::vector<std::unique_ptr<WeeklyTimeInterval>>;

using DayOfWeek = DeviceWeeklyScheduledSuspendTestPolicyBuilder::DayOfWeek;

class DeviceWeeklyScheduledSuspendControllerTest : public testing::Test {
 protected:
  DeviceWeeklyScheduledSuspendControllerTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  // testing::Test:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    InitController();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(
        task_environment_.GetMockTickClock());
    device_weekly_scheduled_suspend_controller_
        ->SetTaskExecutorFactoryForTesting(
            std::make_unique<FakeRepeatingTimeIntervalTaskExecutor::Factory>(
                task_environment_.GetMockClock()));
    user_activity_calls_ = 0;
    chromeos::FakePowerManagerClient::Get()->set_user_activity_callback(
        base::BindRepeating([](int& count) { ++count; },
                            std::ref(user_activity_calls_)));
  }

  void TearDown() override {
    // Clear the policy so that task executors can be cleaned up before shutting
    // down the fake power manager.
    UpdatePolicyPref({});
    device_weekly_scheduled_suspend_controller_.reset();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(nullptr);
    chromeos::FakePowerManagerClient::Get()->set_user_activity_callback(
        base::NullCallback());
    chromeos::PowerManagerClient::Shutdown();
  }

  void UpdatePolicyPref(base::Value::List schedule_list) {
    testing_local_state_.Get()->SetList(prefs::kDeviceWeeklyScheduledSuspend,
                                        std::move(schedule_list));
  }

  void UpdatePolicyAndCheckIntervals(
      const DeviceWeeklyScheduledSuspendTestPolicyBuilder& policy_builder) {
    UpdatePolicyPref(policy_builder.GetAsPrefValue());
    CheckIntervalsInController(policy_builder.GetAsWeeklyTimeIntervals());
  }

  void CheckIntervalsInController(
      const WeeklyTimeIntervals& expected_intervals) {
    const RepeatingTimeIntervalTaskExecutors& interval_executors =
        device_weekly_scheduled_suspend_controller_
            ->GetIntervalExecutorsForTesting();
    ASSERT_EQ(expected_intervals.size(), interval_executors.size());
    for (size_t i = 0; i < expected_intervals.size(); ++i) {
      ASSERT_TRUE(interval_executors[i]);
      EXPECT_EQ(*expected_intervals[i], interval_executors[i]->time_interval());
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
            testing_local_state_.Get());
  }

  int user_activity_calls() const { return user_activity_calls_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ScopedTestingLocalState testing_local_state_;
  std::unique_ptr<DeviceWeeklyScheduledSuspendController>
      device_weekly_scheduled_suspend_controller_;
  int user_activity_calls_;
};

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
       HandlesEmptyIntervalsWithNoPolicy) {
  WeeklyTimeIntervals empty_intervals;
  CheckIntervalsInController(empty_intervals);
}

TEST_F(DeviceWeeklyScheduledSuspendControllerTest, HandlesEmptyPolicy) {
  DeviceWeeklyScheduledSuspendTestPolicyBuilder empty_policy;
  UpdatePolicyAndCheckIntervals(empty_policy);
}

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
       HandlesValidPolicyWithMultipleIntervals) {
  UpdatePolicyAndCheckIntervals(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(12),
                                    DayOfWeek::WEDNESDAY, base::Hours(15))
          .AddWeeklySuspendInterval(DayOfWeek::FRIDAY, base::Hours(20),
                                    DayOfWeek::MONDAY, base::Hours(8)));
}

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
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

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
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

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
       HandlesInvalidPolicyWithMissingStartTime) {
  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddInvalidScheduleMissingStart(DayOfWeek::FRIDAY, base::Hours(6))
          .GetAsPrefValue());

  CheckIntervalsInController({});
}

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
       HandlesInvalidPolicyWithMissingEndTime) {
  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddInvalidScheduleMissingEnd(DayOfWeek::MONDAY, base::Hours(20))
          .GetAsPrefValue());

  CheckIntervalsInController({});
}

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
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

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
       DoesNotCallSuspendWhenOutsideOfInterval) {
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

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
       CallsSuspendOnIntervalStartEveryWeek) {
  auto builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::MONDAY, base::Hours(21), DayOfWeek::TUESDAY,
          base::Hours(7));
  auto intervals = builder.GetAsWeeklyTimeIntervals();
  FastForwardTimeTo(intervals[0]->start(), -base::Minutes(5));
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 0);
  UpdatePolicyPref(builder.GetAsPrefValue());
  FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 1);
  FastForwardTimeTo(intervals[0]->end());
  FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_suspend_calls(), 2);
}

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
       CallsSuspendForEverySuspendInterval) {
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

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
       DeviceFullyResumesWhenIntervalEnds) {
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

TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
       DeviceDoesNotFullyResumeBeforeIntervalEnds) {
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

TEST_F(DeviceWeeklyScheduledSuspendControllerPowerServiceTest,
       PolicyNotSetWhenPowerServiceIsNotAvailable) {
  UpdatePolicyPref(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(12),
                                    DayOfWeek::WEDNESDAY, base::Hours(15))
          .AddWeeklySuspendInterval(DayOfWeek::FRIDAY, base::Hours(20),
                                    DayOfWeek::MONDAY, base::Hours(8))
          .GetAsPrefValue());
  CheckIntervalsInController({});
}

TEST_F(DeviceWeeklyScheduledSuspendControllerPowerServiceTest,
       PolicyGetsSetWhenPowerManagerIsAvailable) {
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

}  // namespace ash
