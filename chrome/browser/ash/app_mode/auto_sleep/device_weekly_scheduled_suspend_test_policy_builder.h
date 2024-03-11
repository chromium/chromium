// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_DEVICE_WEEKLY_SCHEDULED_SUSPEND_TEST_POLICY_BUILDER_H_
#define CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_DEVICE_WEEKLY_SCHEDULED_SUSPEND_TEST_POLICY_BUILDER_H_

#include <memory>
#include <vector>

#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"

namespace ash {

using policy::WeeklyTimeInterval;

// `DeviceWeeklyScheduledSuspendTestPolicyBuilder` provides a builder pattern
// for creating device weekly scheduled suspend policies in tests. A policy
// defines one or more time intervals during which the device should enter a
// suspended state.
//
// Example:
// DeviceWeeklyScheduledSuspendTestPolicyBuilder builder;
// builder.AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY,
//                        base::Hours(7),  // 07:00 AM
//                        DayOfWeek::WEDNESDAY,
//                        base::Hours(21)) // 21:00 PM
//        .AddWeeklySuspendInterval(DayOfWeek::FRIDAY,
//                        base::Hours(18), // 18:00 PM
//                        DayOfWeek::MONDAY,
//                        base::Hours(8)); // 08:00 AM
//
// Corresponding pref representation:
// [
//   {
//     "start": {
//       "day_of_week": "WEDNESDAY",
//       "time": 25200000   // 07:00 AM
//     },
//     "end": {
//       "day_of_week": "WEDNESDAY",
//       "time": 75600000  // 21:00 PM
//     }
//   },
//   {
//     "start": {
//       "day_of_week": "FRIDAY",
//       "time": 64800000   // 18:00 PM
//     },
//     "end": {
//       "day_of_week": "MONDAY",
//       "time": 28800000 // 08:00 AM
//     }
//   }
// ]
class DeviceWeeklyScheduledSuspendTestPolicyBuilder {
 public:
  enum DayOfWeek {
    MONDAY = 1,
    TUESDAY = 2,
    WEDNESDAY = 3,
    THURSDAY = 4,
    FRIDAY = 5,
    SATURDAY = 6,
    SUNDAY = 7,
  };

  DeviceWeeklyScheduledSuspendTestPolicyBuilder();
  DeviceWeeklyScheduledSuspendTestPolicyBuilder(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder&& other) = default;
  DeviceWeeklyScheduledSuspendTestPolicyBuilder& operator=(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder&& other) = default;

  ~DeviceWeeklyScheduledSuspendTestPolicyBuilder();

  // Adds a defined weekly time interval to the device suspension policy. The
  // device will be suspended during the specified interval. Returns a reference
  // to the builder to support method chaining.
  DeviceWeeklyScheduledSuspendTestPolicyBuilder&& AddWeeklySuspendInterval(
      DayOfWeek start_day_of_week,
      const base::TimeDelta& start_time_of_day,
      DayOfWeek end_day_of_week,
      const base::TimeDelta& end_time_of_day);

  // Adds an invalid weekly schedule entry with a missing start time. Returns a
  // reference to the builder to support method chaining.
  DeviceWeeklyScheduledSuspendTestPolicyBuilder&&
  AddInvalidScheduleMissingStart(DayOfWeek end_day_of_week,
                                 const base::TimeDelta& end_time_of_day);

  // Adds an invalid weekly schedule entry with a missing end time. Returns a
  // reference to the builder to support method chaining.
  DeviceWeeklyScheduledSuspendTestPolicyBuilder&& AddInvalidScheduleMissingEnd(
      DayOfWeek start_day_of_week,
      const base::TimeDelta& start_time_of_day);

  base::Value::List GetAsPrefValue() const;

  std::vector<std::unique_ptr<WeeklyTimeInterval>> GetAsWeeklyTimeIntervals()
      const;

 private:
  // Represents the device suspend policy in a list-based format.
  base::Value::List policy_value_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_DEVICE_WEEKLY_SCHEDULED_SUSPEND_TEST_POLICY_BUILDER_H_
