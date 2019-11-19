// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/util/values/values_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace util {

namespace {

TEST(ValuesUtilTest, BasicLimits) {
  struct {
    int64_t input;
    const char* expected;
  } test_cases[] = {
      {0, "0"},
      {-1234, "-1234"},
      {5678, "5678"},
      {std::numeric_limits<int64_t>::lowest(), "-9223372036854775808"},
      {std::numeric_limits<int64_t>::max(), "9223372036854775807"},
  };
  for (const auto& test_case : test_cases) {
    int64_t input = test_case.input;
    base::TimeDelta time_delta_input = base::TimeDelta::FromMicroseconds(input);
    base::Time time_input =
        base::Time::FromDeltaSinceWindowsEpoch(time_delta_input);
    base::Value expected(test_case.expected);
    SCOPED_TRACE(testing::Message()
                 << "input: " << input << ", expected: " << expected);

    EXPECT_EQ(Int64ToValue(input), expected);
    EXPECT_EQ(*ValueToInt64(&expected), input);

    EXPECT_EQ(TimeDeltaToValue(time_delta_input), expected);
    EXPECT_EQ(*ValueToTimeDelta(&expected), time_delta_input);

    EXPECT_EQ(TimeToValue(time_input), expected);
    EXPECT_EQ(*ValueToTime(&expected), time_input);
  }
}

TEST(ValuesUtilTest, InvalidValues) {
  std::unique_ptr<base::Value> test_cases[] = {
      nullptr,
      std::make_unique<base::Value>(),
      std::make_unique<base::Value>(0),
      std::make_unique<base::Value>(1234),
      std::make_unique<base::Value>(true),
      std::make_unique<base::Value>(base::Value::Type::BINARY),
      std::make_unique<base::Value>(base::Value::Type::LIST),
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY),
      std::make_unique<base::Value>(""),
      std::make_unique<base::Value>("abcd"),
      std::make_unique<base::Value>("1234.0"),
      std::make_unique<base::Value>("1234a"),
      std::make_unique<base::Value>("a1234"),
  };
  for (const auto& test_case : test_cases) {
    EXPECT_FALSE(ValueToInt64(test_case.get()));
    EXPECT_FALSE(ValueToTimeDelta(test_case.get()));
    EXPECT_FALSE(ValueToTime(test_case.get()));
  }
}

}  // namespace

}  // namespace util
