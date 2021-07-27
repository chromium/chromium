// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/values_util.h"

#include <limits>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

TEST(ValuesUtilTest, BasicInt64Limits) {
  constexpr struct {
    int64_t input;
    StringPiece expected;
  } kTestCases[] = {
      {0, "0"},
      {-1234, "-1234"},
      {5678, "5678"},
      {std::numeric_limits<int64_t>::lowest(), "-9223372036854775808"},
      {std::numeric_limits<int64_t>::max(), "9223372036854775807"},
  };
  for (const auto& test_case : kTestCases) {
    int64_t input = test_case.input;
    TimeDelta time_delta_input = TimeDelta::FromMicroseconds(input);
    Time time_input = Time::FromDeltaSinceWindowsEpoch(time_delta_input);
    Value expected(test_case.expected);
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

TEST(ValuesUtilTest, InvalidInt64Values) {
  const std::unique_ptr<Value> kTestCases[] = {
      nullptr,
      std::make_unique<Value>(),
      std::make_unique<Value>(0),
      std::make_unique<Value>(1234),
      std::make_unique<Value>(true),
      std::make_unique<Value>(Value::Type::BINARY),
      std::make_unique<Value>(Value::Type::LIST),
      std::make_unique<Value>(Value::Type::DICTIONARY),
      std::make_unique<Value>(""),
      std::make_unique<Value>("abcd"),
      std::make_unique<Value>("1234.0"),
      std::make_unique<Value>("1234a"),
      std::make_unique<Value>("a1234"),
  };
  for (const auto& test_case : kTestCases) {
    EXPECT_FALSE(ValueToInt64(test_case.get()));
    EXPECT_FALSE(ValueToTimeDelta(test_case.get()));
    EXPECT_FALSE(ValueToTime(test_case.get()));
  }
}

TEST(ValuesUtilTest, FilePath) {
  // Ω is U+03A9 GREEK CAPITAL LETTER OMEGA, a non-ASCII character.
  constexpr StringPiece kTestCases[] = {
      "/unix/Ω/path.dat",
      "C:\\windows\\Ω\\path.dat",
  };
  for (auto test_case : kTestCases) {
    FilePath input = FilePath::FromUTF8Unsafe(test_case);
    Value expected(test_case);
    SCOPED_TRACE(testing::Message() << "test_case: " << test_case);

    EXPECT_EQ(FilePathToValue(input), expected);
    EXPECT_EQ(*ValueToFilePath(&expected), input);
  }
}

TEST(ValuesUtilTest, UnguessableToken) {
  constexpr struct {
    uint64_t high;
    uint64_t low;
    StringPiece expected;
  } kTestCases[] = {
      {0x123456u, 0x9ABCu, "5634120000000000BC9A000000000000"},
  };
  for (const auto& test_case : kTestCases) {
    UnguessableToken input =
        UnguessableToken::Deserialize(test_case.high, test_case.low);
    Value expected(test_case.expected);
    SCOPED_TRACE(testing::Message() << "expected: " << test_case.expected);

    EXPECT_EQ(UnguessableTokenToValue(input), expected);
    EXPECT_EQ(*ValueToUnguessableToken(&expected), input);
  }
}

}  // namespace

}  // namespace base
