// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_DEVICE_WEEKLY_SCHEDULED_SUSPEND_TEST_POLICY_BUILDER_H_
#define CHROME_BROWSER_ASH_APP_MODE_DEVICE_WEEKLY_SCHEDULED_SUSPEND_TEST_POLICY_BUILDER_H_

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
// builder.AppendSchedule(DayOfWeek::WEDNESDAY,
//                        base::Hours(7),  // 07:00 AM
//                        DayOfWeek::WEDNESDAY,
//                        base::Hours(21)) // 21:00 PM
//        .AppendSchedule(DayOfWeek::FRIDAY,
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
    MONDAY,
    TUESDAY,
    WEDNESDAY,
    THURSDAY,
    FRIDAY,
    SATURDAY,
    SUNDAY,
  };

  DeviceWeeklyScheduledSuspendTestPolicyBuilder();
  DeviceWeeklyScheduledSuspendTestPolicyBuilder(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder&& other) = default;
  DeviceWeeklyScheduledSuspendTestPolicyBuilder& operator=(
      DeviceWeeklyScheduledSuspendTestPolicyBuilder&& other) = default;

  ~DeviceWeeklyScheduledSuspendTestPolicyBuilder();

  // Appends a weekly schedule interval to the policy. The device will be
  // suspended during the specified interval. Returns a reference to the builder
  // to support method chaining.
  DeviceWeeklyScheduledSuspendTestPolicyBuilder&& AppendSchedule(
      DayOfWeek start_day_of_week,
      const base::TimeDelta& start_time_of_day,
      DayOfWeek end_day_of_week,
      const base::TimeDelta& end_time_of_day);

  base::Value::List GetAsPrefValue() const;

  std::vector<std::unique_ptr<WeeklyTimeInterval>> GetAsWeeklyTimeIntervals()
      const;

 private:
  // Represents the device suspend policy in a list-based format.
  base::Value::List policy_value_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_DEVICE_WEEKLY_SCHEDULED_SUSPEND_TEST_POLICY_BUILDER_H_
