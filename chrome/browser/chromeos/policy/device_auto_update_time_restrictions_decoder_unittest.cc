// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_auto_update_time_restrictions_decoder.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/policy/weekly_time/weekly_time.h"
#include "chromeos/policy/weekly_time/weekly_time_interval.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using std::tuple;
using std::string;
using std::vector;

namespace {
constexpr base::TimeDelta kHour = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kMinute = base::TimeDelta::FromMinutes(1);
}  // namespace

namespace policy {

using TestTimeTuple = tuple<string /* day_of_week string */,
                            int /* day_of_week_int */,
                            int /* hours */,
                            int /* minutes */>;

class WeeklyTimeDictionaryDecoderTest
    : public testing::TestWithParam<TestTimeTuple> {
 protected:
  string day_of_week_str() { return std::get<0>(GetParam()); }
  int day_of_week() { return std::get<1>(GetParam()); }
  int hours() { return std::get<2>(GetParam()); }
  int minutes() { return std::get<3>(GetParam()); }
};

TEST_P(WeeklyTimeDictionaryDecoderTest, WeeklyTimeFromDict) {
  base::DictionaryValue time_dict;
  ASSERT_TRUE(time_dict.SetKey("day_of_week", base::Value(day_of_week_str())));
  ASSERT_TRUE(time_dict.SetKey("hours", base::Value(hours())));
  ASSERT_TRUE(time_dict.SetKey("minutes", base::Value(minutes())));

  base::Optional<WeeklyTime> result = WeeklyTimeFromDictValue(time_dict);
  ASSERT_TRUE(result);
  EXPECT_EQ(day_of_week(), result.value().day_of_week());
  EXPECT_EQ(
      hours() * kHour.InMilliseconds() + minutes() * kMinute.InMilliseconds(),
      result.value().milliseconds());
}

INSTANTIATE_TEST_SUITE_P(ZeroHours,
                         WeeklyTimeDictionaryDecoderTest,
                         testing::Values(std::make_tuple("Monday", 1, 0, 20)));

INSTANTIATE_TEST_SUITE_P(ZeroMins,
                         WeeklyTimeDictionaryDecoderTest,
                         testing::Values(std::make_tuple("Sunday", 7, 10, 0)));

INSTANTIATE_TEST_SUITE_P(
    RandomCase,
    WeeklyTimeDictionaryDecoderTest,
    testing::Values(std::make_tuple("Thursday", 4, 3, 10)));

using TestIntervalTuple = tuple<string /* start day_of_week string */,
                                int /* start day_of_week_int */,
                                int /* start hours */,
                                int /* start minutes */,
                                string /* end day_of_week string */,
                                int /* end day_of_week_int */,
                                int /* end hours */,
                                int /* end minutes */>;

class WeeklyTimeIntervalListDecoderTest
    : public testing::TestWithParam<vector<TestIntervalTuple>> {
 protected:
  ParamType intervals() { return GetParam(); }

  base::ListValue GetIntervalList() {
    base::ListValue list_val;
    for (const auto& interval_tuple : intervals()) {
      base::DictionaryValue start;
      start.SetKey("day_of_week", base::Value(std::get<0>(interval_tuple)));
      start.SetKey("hours", base::Value(std::get<2>(interval_tuple)));
      start.SetKey("minutes", base::Value(std::get<3>(interval_tuple)));

      base::DictionaryValue end;
      end.SetKey("day_of_week", base::Value(std::get<4>(interval_tuple)));
      end.SetKey("hours", base::Value(std::get<6>(interval_tuple)));
      end.SetKey("minutes", base::Value(std::get<7>(interval_tuple)));

      base::DictionaryValue time_dict;
      time_dict.SetKey("start", std::move(start));
      time_dict.SetKey("end", std::move(end));
      list_val.Append(std::move(time_dict));
    }
    return list_val;
  }

  bool CompareIntervalToTuple(const WeeklyTimeInterval& interval,
                              const TestIntervalTuple& tup) {
    int start_time = std::get<2>(tup) * kHour.InMilliseconds() +
                     std::get<3>(tup) * kMinute.InMilliseconds();
    int end_time = std::get<6>(tup) * kHour.InMilliseconds() +
                   std::get<7>(tup) * kMinute.InMilliseconds();
    return interval.start().day_of_week() == std::get<1>(tup) &&
           interval.start().milliseconds() == start_time &&
           interval.end().day_of_week() == std::get<5>(tup) &&
           interval.end().milliseconds() == end_time;
  }
};

TEST_P(WeeklyTimeIntervalListDecoderTest, WeeklyTimeIntervalsFromListValue) {
  base::ListValue list_val = GetIntervalList();
  ParamType tuple_vector = intervals();
  vector<WeeklyTimeInterval> result;
  ASSERT_TRUE(WeeklyTimeIntervalsFromListValue(list_val, &result));
  ASSERT_TRUE(result.size() == tuple_vector.size());
  bool equal_vectors = true;
  for (size_t i = 0; i < result.size(); ++i) {
    if (!CompareIntervalToTuple(result[i], tuple_vector[i])) {
      equal_vectors = false;
    }
  }
  EXPECT_TRUE(equal_vectors);
}

INSTANTIATE_TEST_SUITE_P(EmptyList,
                         WeeklyTimeIntervalListDecoderTest,
                         testing::Values(vector<TestIntervalTuple>()));

INSTANTIATE_TEST_SUITE_P(
    OneInterval,
    WeeklyTimeIntervalListDecoderTest,
    testing::Values(vector<TestIntervalTuple>{
        std::make_tuple("Monday", 1, 10, 20, "Tuesday", 2, 20, 100),
    }));

INSTANTIATE_TEST_SUITE_P(
    MultipleIntervals,
    WeeklyTimeIntervalListDecoderTest,
    testing::Values(vector<TestIntervalTuple>{
        std::make_tuple("Monday", 1, 10, 20, "Tuesday", 2, 20, 10),
        std::make_tuple("Wednesday", 3, 20, 15, "Friday", 5, 10, 10),
        std::make_tuple("Monday", 1, 5, 20, "Saturday", 6, 10, 10),
        std::make_tuple("Sunday", 7, 4, 20, "Tuesday", 2, 10, 30)}));

}  // namespace policy
