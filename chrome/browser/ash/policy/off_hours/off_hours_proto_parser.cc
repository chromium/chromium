// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/off_hours/off_hours_proto_parser.h"

#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/time_utils.h"

namespace em = enterprise_management;

namespace policy {
namespace off_hours {

std::vector<WeeklyTimeInterval> ExtractWeeklyTimeIntervalsFromProto(
    const em::DeviceOffHoursProto& container,
    const std::string& timezone,
    base::Clock* clock) {
  int offset;
  if (!weekly_time_utils::GetOffsetFromTimezoneToGmt(timezone, clock,
                                                     &offset)) {
    return {};
  }
  std::vector<WeeklyTimeInterval> intervals;
  for (const auto& entry : container.intervals()) {
    // The offset is to convert from |timezone| to GMT. Negate it to get the
    // offset from GMT to |timezone|.
    auto interval = WeeklyTimeInterval::ExtractFromProto(entry, -offset);
    if (interval) {
      intervals.push_back(*interval);
    }
  }
  return intervals;
}

std::vector<int> ExtractIgnoredPolicyProtoTagsFromProto(
    const em::DeviceOffHoursProto& container) {
  return std::vector<int>(container.ignored_policy_proto_tags().begin(),
                          container.ignored_policy_proto_tags().end());
}

std::optional<std::string> ExtractTimezoneFromProto(
    const em::DeviceOffHoursProto& container) {
  if (!container.has_timezone()) {
    return std::nullopt;
  }
  return std::make_optional(container.timezone());
}

std::optional<base::Value::Dict> ConvertOffHoursProtoToValue(
    const em::DeviceOffHoursProto& container) {
  std::optional<std::string> timezone = ExtractTimezoneFromProto(container);
  if (!timezone) {
    return std::nullopt;
  }
  auto off_hours = base::Value::Dict().Set("timezone", *timezone);
  std::vector<WeeklyTimeInterval> intervals =
      ExtractWeeklyTimeIntervalsFromProto(container, *timezone,
                                          base::DefaultClock::GetInstance());
  base::Value::List intervals_value;
  for (const auto& interval : intervals) {
    intervals_value.Append(interval.ToValue());
  }
  off_hours.Set("intervals", std::move(intervals_value));
  std::vector<int> ignored_policy_proto_tags =
      ExtractIgnoredPolicyProtoTagsFromProto(container);
  base::Value::List ignored_policies_value;
  for (const auto& policy : ignored_policy_proto_tags) {
    ignored_policies_value.Append(policy);
  }
  off_hours.Set("ignored_policy_proto_tags", std::move(ignored_policies_value));
  return off_hours;
}

}  // namespace off_hours
}  // namespace policy
