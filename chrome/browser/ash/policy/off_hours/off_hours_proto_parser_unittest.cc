// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/policy/off_hours/off_hours_proto_parser.h"

#include <optional>
#include <utility>

#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy::off_hours {

namespace {

namespace em = ::enterprise_management;

constexpr em::WeeklyTimeProto_DayOfWeek kWeekdays[] = {
    em::WeeklyTimeProto::DAY_OF_WEEK_UNSPECIFIED,
    em::WeeklyTimeProto::MONDAY,
    em::WeeklyTimeProto::TUESDAY,
    em::WeeklyTimeProto::WEDNESDAY,
    em::WeeklyTimeProto::THURSDAY,
    em::WeeklyTimeProto::FRIDAY,
    em::WeeklyTimeProto::SATURDAY,
    em::WeeklyTimeProto::SUNDAY};

constexpr base::TimeDelta kHour = base::Hours(1);

const char kGmtTimezone[] = "GMT";
const char kLosAngelesTimezone[] = "America/Los_Angeles";

const int kDeviceAllowNewUsersTag = 3;
const int kDeviceGuestModeEnabledTag = 8;

const std::vector<int> kDefaultIgnoredPolicies = {kDeviceAllowNewUsersTag,
                                                  kDeviceGuestModeEnabledTag};

struct OffHoursPolicy {
  std::optional<std::string> timezone;
  std::vector<WeeklyTimeInterval> intervals;
  std::vector<int> ignored_policy_proto_tags;

  OffHoursPolicy(const std::optional<std::string>& timezone,
                 const std::vector<WeeklyTimeInterval>& intervals,
                 const std::vector<int>& ignored_policy_proto_tags)
      : timezone(timezone),
        intervals(intervals),
        ignored_policy_proto_tags(ignored_policy_proto_tags) {}
};

em::WeeklyTimeIntervalProto ConvertWeeklyTimeIntervalToProto(
    const WeeklyTimeInterval& weekly_time_interval) {
  em::WeeklyTimeIntervalProto interval_proto;
  em::WeeklyTimeProto* start = interval_proto.mutable_start();
  em::WeeklyTimeProto* end = interval_proto.mutable_end();
  start->set_day_of_week(kWeekdays[weekly_time_interval.start().day_of_week()]);
  start->set_time(weekly_time_interval.start().milliseconds());
  end->set_day_of_week(kWeekdays[weekly_time_interval.end().day_of_week()]);
  end->set_time(weekly_time_interval.end().milliseconds());
  return interval_proto;
}

void RemoveOffHoursPolicyFromProto(em::ChromeDeviceSettingsProto* proto) {
  proto->clear_device_off_hours();
}

void SetOffHoursPolicyToProto(em::ChromeDeviceSettingsProto* proto,
                              const OffHoursPolicy& off_hours_policy) {
  RemoveOffHoursPolicyFromProto(proto);
  auto* off_hours = proto->mutable_device_off_hours();
  for (auto interval : off_hours_policy.intervals) {
    auto interval_proto = ConvertWeeklyTimeIntervalToProto(interval);
    auto* cur = off_hours->add_intervals();
    *cur = interval_proto;
  }
  if (off_hours_policy.timezone)
    off_hours->set_timezone(*off_hours_policy.timezone);
  for (auto p : off_hours_policy.ignored_policy_proto_tags) {
    off_hours->add_ignored_policy_proto_tags(p);
  }
}

}  // namespace

class OffHoursParserTest : public testing::Test {};

TEST_F(OffHoursParserTest, ExtractWeeklyTimeIntervalsLosAngeles) {
  WeeklyTime start = WeeklyTime(1, kHour.InMilliseconds(), std::nullopt);
  WeeklyTime end = WeeklyTime(3, kHour.InMilliseconds() * 2, std::nullopt);
  std::vector<WeeklyTimeInterval> proto_intervals = {
      WeeklyTimeInterval(start, end)};

  em::ChromeDeviceSettingsProto proto;
  SetOffHoursPolicyToProto(&proto,
                           OffHoursPolicy(kLosAngelesTimezone, proto_intervals,
                                          kDefaultIgnoredPolicies));

  base::SimpleTestClock clock;
  std::vector<WeeklyTimeInterval> intervals =
      ExtractWeeklyTimeIntervalsFromProto(proto.device_off_hours(),
                                          kLosAngelesTimezone, &clock);

  // SimpleTestClock is at 1970-01-01 by default.
  // Los Angeles is 8 hours behind UTC.
  std::vector<WeeklyTimeInterval> expected_intervals = {WeeklyTimeInterval(
      {start.day_of_week(), start.milliseconds(), -kHour.InMilliseconds() * 8},
      {end.day_of_week(), end.milliseconds(), -kHour.InMilliseconds() * 8})};

  EXPECT_EQ(intervals, expected_intervals);
}

TEST_F(OffHoursParserTest, ConvertOffHoursProtoToValue) {
  WeeklyTime start = WeeklyTime(1, kHour.InMilliseconds(), 0);
  WeeklyTime end = WeeklyTime(3, kHour.InMilliseconds() * 2, 0);
  std::vector<WeeklyTimeInterval> intervals = {WeeklyTimeInterval(start, end)};

  em::ChromeDeviceSettingsProto proto;
  SetOffHoursPolicyToProto(
      &proto, OffHoursPolicy(kGmtTimezone, intervals, kDefaultIgnoredPolicies));

  std::optional<base::Value::Dict> off_hours_value =
      ConvertOffHoursProtoToValue(proto.device_off_hours());

  base::Value::Dict off_hours_expected;
  off_hours_expected.Set("timezone", kGmtTimezone);
  base::Value::List intervals_value;
  for (const auto& interval : intervals)
    intervals_value.Append(interval.ToValue());
  off_hours_expected.Set("intervals", std::move(intervals_value));
  base::Value::List ignored_policies_value;
  for (const auto& policy : kDefaultIgnoredPolicies)
    ignored_policies_value.Append(policy);
  off_hours_expected.Set("ignored_policy_proto_tags",
                         std::move(ignored_policies_value));

  EXPECT_EQ(*off_hours_value, off_hours_expected);
}

using OffHoursParserTimezoneFromProtoTest = testing::TestWithParam<
    std::tuple<bool,                        // has off hours
               std::optional<std::string>,  // off hours time zone
               std::optional<std::string>   // expected timezone
               >>;

TEST_P(OffHoursParserTimezoneFromProtoTest, Extract) {
  // Extract test parameters.
  bool has_off_hours = std::get<0>(GetParam());
  std::optional<std::string> off_hourse_timezone = std::get<1>(GetParam());
  std::optional<std::string> expected_timezone = std::get<2>(GetParam());

  em::ChromeDeviceSettingsProto proto;

  // Apply parameters.
  if (has_off_hours) {
    SetOffHoursPolicyToProto(&proto, OffHoursPolicy(off_hourse_timezone, {},
                                                    kDefaultIgnoredPolicies));
  }

  const std::optional<std::string> timezone =
      ExtractTimezoneFromProto(proto.device_off_hours());

  EXPECT_EQ(timezone, expected_timezone);
}

INSTANTIATE_TEST_SUITE_P(
    ExtractNoTimezoneForNoOffHours,
    OffHoursParserTimezoneFromProtoTest,
    ::testing::Combine(testing::Values(false),  // has no off hours
                       testing::Values<std::optional<std::string>>(
                           std::nullopt),  // off hours time zone
                       testing::Values<std::optional<std::string>>(
                           std::nullopt)  // expected timezone
                       ));

INSTANTIATE_TEST_SUITE_P(
    ExtractNoTimezoneForUnsetTimezone,
    OffHoursParserTimezoneFromProtoTest,
    ::testing::Combine(testing::Values(true),  // has off hours
                       testing::Values<std::optional<std::string>>(
                           std::nullopt),  // off hours time zone
                       testing::Values<std::optional<std::string>>(
                           std::nullopt)  // expected timezone
                       ));

INSTANTIATE_TEST_SUITE_P(
    ExtractTimezone,
    OffHoursParserTimezoneFromProtoTest,
    ::testing::Combine(testing::Values(true),  // has off hours
                       testing::Values<std::optional<std::string>>(
                           kLosAngelesTimezone),  // off hours time zone
                       testing::Values<std::optional<std::string>>(
                           kLosAngelesTimezone)  // expected timezone
                       ));

}  // namespace policy::off_hours
