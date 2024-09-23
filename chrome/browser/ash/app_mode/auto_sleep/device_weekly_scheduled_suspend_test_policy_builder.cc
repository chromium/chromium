// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_test_policy_builder.h"

#include <algorithm>
#include <string_view>

namespace ash {

using DayOfWeek = DeviceWeeklyScheduledSuspendTestPolicyBuilder::DayOfWeek;

namespace {

constexpr char kMonday[] = "MONDAY";
constexpr char kTuesday[] = "TUESDAY";
constexpr char kWednesday[] = "WEDNESDAY";
constexpr char kThursday[] = "THURSDAY";
constexpr char kFriday[] = "FRIDAY";
constexpr char kSaturday[] = "SATURDAY";
constexpr char kSunday[] = "SUNDAY";

constexpr char kStart[] = "start";
constexpr char kEnd[] = "end";
constexpr char kDayOfWeek[] = "day_of_week";
constexpr char kTime[] = "time";

std::string_view DayOfWeekToStringView(DayOfWeek day) {
  switch (day) {
    case DayOfWeek::MONDAY:
      return kMonday;
    case DayOfWeek::TUESDAY:
      return kTuesday;
    case DayOfWeek::WEDNESDAY:
      return kWednesday;
    case DayOfWeek::THURSDAY:
      return kThursday;
    case DayOfWeek::FRIDAY:
      return kFriday;
    case DayOfWeek::SATURDAY:
      return kSaturday;
    case DayOfWeek::SUNDAY:
      return kSunday;
  }
}

base::Value::Dict BuildScheduleTimePoint(DayOfWeek day_of_week,
                                         const base::TimeDelta& time_of_day) {
  return base::Value::Dict()
      .Set(kDayOfWeek, DayOfWeekToStringView(day_of_week))
      .Set(kTime, static_cast<int>(time_of_day.InMilliseconds()));
}

}  // namespace

DeviceWeeklyScheduledSuspendTestPolicyBuilder::
    DeviceWeeklyScheduledSuspendTestPolicyBuilder() = default;

DeviceWeeklyScheduledSuspendTestPolicyBuilder::
    ~DeviceWeeklyScheduledSuspendTestPolicyBuilder() = default;

DeviceWeeklyScheduledSuspendTestPolicyBuilder&&
DeviceWeeklyScheduledSuspendTestPolicyBuilder::AddWeeklySuspendInterval(
    DayOfWeek start_day_of_week,
    const base::TimeDelta& start_time_of_day,
    DayOfWeek end_day_of_week,
    const base::TimeDelta& end_time_of_day) {
  policy_value_.Append(
      base::Value::Dict()
          .Set(kStart,
               BuildScheduleTimePoint(start_day_of_week, start_time_of_day))
          .Set(kEnd, BuildScheduleTimePoint(end_day_of_week, end_time_of_day)));
  return std::move(*this);
}

DeviceWeeklyScheduledSuspendTestPolicyBuilder&&
DeviceWeeklyScheduledSuspendTestPolicyBuilder::AddInvalidScheduleMissingStart(
    DayOfWeek end_day_of_week,
    const base::TimeDelta& end_time_of_day) {
  policy_value_.Append(base::Value::Dict().Set(
      kEnd, BuildScheduleTimePoint(end_day_of_week, end_time_of_day)));
  return std::move(*this);
}

DeviceWeeklyScheduledSuspendTestPolicyBuilder&&
DeviceWeeklyScheduledSuspendTestPolicyBuilder::AddInvalidScheduleMissingEnd(
    DayOfWeek start_day_of_week,
    const base::TimeDelta& start_time_of_day) {
  policy_value_.Append(base::Value::Dict().Set(
      kStart, BuildScheduleTimePoint(start_day_of_week, start_time_of_day)));
  return std::move(*this);
}

base::Value::List
DeviceWeeklyScheduledSuspendTestPolicyBuilder::GetAsPrefValue() const {
  return policy_value_.Clone();
}

std::vector<std::unique_ptr<WeeklyTimeInterval>>
DeviceWeeklyScheduledSuspendTestPolicyBuilder::GetAsWeeklyTimeIntervals()
    const {
  std::vector<std::unique_ptr<WeeklyTimeInterval>> ret;
  std::ranges::transform(policy_value_, std::back_inserter(ret),
                         [](const base::Value& value) {
                           return WeeklyTimeInterval::ExtractFromDict(
                               value.GetDict(),
                               /*timezone_offset=*/std::nullopt);
                         });
  return ret;
}

}  // namespace ash
