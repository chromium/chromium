// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/device_weekly_scheduled_suspend_controller.h"

#include <memory>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/device_weekly_scheduled_suspend_test_policy_builder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using policy::WeeklyTimeInterval;
using WeeklyTimeIntervals = std::vector<std::unique_ptr<WeeklyTimeInterval>>;

using DayOfWeek = DeviceWeeklyScheduledSuspendTestPolicyBuilder::DayOfWeek;

class DeviceWeeklyScheduledSuspendControllerTest : public testing::Test {
 protected:
  DeviceWeeklyScheduledSuspendControllerTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()),
        device_weekly_scheduled_suspend_controller_(
            testing_local_state_.Get()) {}

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
            .GetIntervalExecutorsForTesting();
    ASSERT_EQ(expected_intervals.size(), interval_executors.size());
    for (size_t i = 0; i < expected_intervals.size(); ++i) {
      ASSERT_TRUE(interval_executors[i]);
      EXPECT_EQ(*expected_intervals[i],
                interval_executors[i]->GetTimeInterval());
    }
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState testing_local_state_;
  DeviceWeeklyScheduledSuspendController
      device_weekly_scheduled_suspend_controller_;
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

}  // namespace ash
