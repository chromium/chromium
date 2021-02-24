// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/policy/device_auto_update_time_restrictions_utils.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/strings/string16.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chromeos/policy/weekly_time/weekly_time.h"
#include "chromeos/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/settings/cros_settings_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

using base::ListValue;
using base::Time;
using base::Value;
using std::tuple;
using std::vector;

namespace policy {

namespace {

enum {
  kMonday = 1,
  kTuesday = 2,
  kWednesday = 3,
  kThursday = 4,
  kFriday = 5,
  kSaturday = 6,
  kSunday = 7,
};

std::string DayToString(int day_of_week) {
  switch (day_of_week) {
    case kMonday:
      return "Monday";
    case kTuesday:
      return "Tuesday";
    case kWednesday:
      return "Wednesday";
    case kThursday:
      return "Thursday";
    case kFriday:
      return "Friday";
    case kSaturday:
      return "Saturday";
    case kSunday:
      return "Sunday";
  }
  return "";
}

const char* kNewYorkTimezone = "America/New_York";
constexpr int kMillisecondsInMinute = 60000;
constexpr int kMillisecondsInHour = 3600000;
constexpr int kNewYorkOffset = -4 * kMillisecondsInHour;
constexpr Time::Exploded kDaylightTime{2018, 8, 3, 8, 15, 0, 0, 0};

}  // namespace

class DeviceAutoUpdateTimeRestrictionsUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    timezone_.reset(icu::TimeZone::createDefault());
    icu::TimeZone::adoptDefault(
        icu::TimeZone::createTimeZone(kNewYorkTimezone));
    // Use Daylight savings to use EDT.
    Time test_time;
    ASSERT_TRUE(base::Time::FromUTCExploded(kDaylightTime, &test_time));
    test_clock_.SetNow(test_time);
  }

  void TearDown() override { icu::TimeZone::adoptDefault(timezone_.release()); }

  ListValue GetIntervalsAsList(const vector<WeeklyTimeInterval>& intervals) {
    ListValue list_val;
    for (const auto& interval : intervals) {
      base::DictionaryValue start;
      int start_hours = interval.start().milliseconds() / kMillisecondsInHour;
      int start_minutes = (interval.start().milliseconds() -
                           start_hours * kMillisecondsInHour) /
                          kMillisecondsInMinute;
      start.SetKey("day_of_week",
                   Value(DayToString(interval.start().day_of_week())));
      start.SetKey("hours", Value(start_hours));
      start.SetKey("minutes", Value(start_minutes));

      base::DictionaryValue end;
      int end_hours = interval.end().milliseconds() / kMillisecondsInHour;
      int end_minutes =
          (interval.end().milliseconds() - end_hours * kMillisecondsInHour) /
          kMillisecondsInMinute;
      end.SetKey("day_of_week",
                 Value(DayToString(interval.end().day_of_week())));
      end.SetKey("hours", Value(end_hours));
      end.SetKey("minutes", Value(end_minutes));

      base::DictionaryValue time_dict;
      time_dict.SetKey("start", std::move(start));
      time_dict.SetKey("end", std::move(end));
      list_val.Append(std::move(time_dict));
    }
    return list_val;
  }

  base::SimpleTestClock test_clock_;
  // These initialize CrosSettings and then tear down when the test is done.
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;

 private:
  std::unique_ptr<icu::TimeZone> timezone_;
};

TEST_F(DeviceAutoUpdateTimeRestrictionsUtilTest,
       GetDeviceAutoUpdateTimeRestrictionsIntervalsInLocalTimezone) {
  const vector<WeeklyTimeInterval> kExpected{
      WeeklyTimeInterval(
          WeeklyTime(kMonday, 5 * kMillisecondsInHour, kNewYorkOffset),
          WeeklyTime(kTuesday, 10 * kMillisecondsInHour, kNewYorkOffset)),
      WeeklyTimeInterval(
          WeeklyTime(kWednesday, 10 * kMillisecondsInHour, kNewYorkOffset),
          WeeklyTime(kWednesday, 15 * kMillisecondsInHour, kNewYorkOffset))};

  scoped_testing_cros_settings_.device_settings()->Set(
      chromeos::kDeviceAutoUpdateTimeRestrictions,
      GetIntervalsAsList(kExpected));

  vector<WeeklyTimeInterval> result;
  ASSERT_TRUE(GetDeviceAutoUpdateTimeRestrictionsIntervalsInLocalTimezone(
      &test_clock_, &result));
  EXPECT_EQ(result, kExpected);
}

}  // namespace policy
