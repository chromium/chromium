// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/gmock_expected_support.h"

#include <optional>
#include <string>

#include "base/types/expected.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {
namespace {

template <typename MatcherType, typename Value>
std::string Explain(const MatcherType& matcher, const Value& value) {
  testing::StringMatchResultListener listener;
  testing::ExplainMatchResult(matcher, value, &listener);
  return listener.str();
}

TEST(GmockExpectedSupportTest, ExpectOk) {
  const expected<void, std::string> e_void;
  EXPECT_OK(e_void);

  const expected<int, std::string> e_int = 1;
  EXPECT_OK(e_int);
}

TEST(GmockExpectedSupportTest, ExpectOkOptional) {
  const std::optional<int> o_int = 1;
  EXPECT_OK(o_int);
}

TEST(GmockExpectedSupportTest, AssertOk) {
  const expected<void, std::string> e_void;
  ASSERT_OK(e_void);

  const expected<int, std::string> e_int = 1;
  ASSERT_OK(e_int);
}

TEST(GmockExpectedSupportTest, AssertOkOptional) {
  const std::optional<int> o_int = 1;
  ASSERT_OK(o_int);
}

TEST(GmockExpectedSupportTest, AssertOkAndAssign) {
  const expected<int, std::string> e_int = 1;
  ASSERT_OK_AND_ASSIGN(int result1, e_int);
  EXPECT_EQ(1, result1);

  const std::optional<int> o_int = 2;
  ASSERT_OK_AND_ASSIGN(int result2, o_int);
  EXPECT_EQ(2, result2);
}

TEST(GmockExpectedSupportTest, AssertOkAndAssignFailureExpected) {
  static const expected<int, std::string> e_int = unexpected("error_msg");
  EXPECT_FATAL_FAILURE(
      []() {
        ASSERT_OK_AND_ASSIGN(int result, e_int);
        (void)result;
      }(),
      "e_int returned error: error_msg");
}

TEST(GmockExpectedSupportTest, AssertOkAndAssignFailureOptional) {
  static const std::optional<int> o_int = std::nullopt;
  EXPECT_FATAL_FAILURE(
      []() {
        ASSERT_OK_AND_ASSIGN(int result, o_int);
        (void)result;
      }(),
      "o_int returned nullopt");
}

TEST(GmockExpectedSupportTest, VoidOkEquals) {
  EXPECT_EQ(ok(), ok());
  EXPECT_NE(ok(), ok("test"));
  const expected<void, std::string> is_ok = ok();
  EXPECT_EQ(ok(), is_ok);
  EXPECT_EQ(is_ok, ok());
  const expected<void, std::string> not_ok = unexpected("test");
  EXPECT_NE(ok(), not_ok);
  EXPECT_NE(not_ok, ok());
}

TEST(GmockExpectedSupportTest, PrintTest) {
  EXPECT_EQ(testing::PrintToString(ok()), "ok()");
  EXPECT_EQ(testing::PrintToString(ok("test")), "ok(test)");

  EXPECT_EQ(testing::PrintToString(unexpected<std::string>("test")),
            "Unexpected(test)");

  EXPECT_EQ(testing::PrintToString(expected<void, std::string>(ok())),
            "Expected()");
  EXPECT_EQ(
      testing::PrintToString(expected<std::string, std::string>(ok("test"))),
      "Expected(test)");
  EXPECT_EQ(testing::PrintToString(
                expected<std::string, std::string>(unexpected("test"))),
            "Unexpected(test)");
}

TEST(GmockExpectedSupportTest, HasValue) {
  const expected<void, std::string> e_void;
  EXPECT_THAT(e_void, HasValue());

  const expected<int, std::string> e_int = 3;
  EXPECT_THAT(e_int, HasValue());

  const expected<int, std::string> error = unexpected("Uh oh");
  EXPECT_THAT(error, ::testing::Not(HasValue()));
}

TEST(GmockExpectedSupportTest, HasValueOptional) {
  const std::optional<int> o_int = 3;
  EXPECT_THAT(o_int, HasValue());

  const std::optional<int> o_nullopt = std::nullopt;
  EXPECT_THAT(o_nullopt, ::testing::Not(HasValue()));
}

TEST(GmockExpectedSupportTest, ValueIs) {
  const expected<int, std::string> e_int = 3;
  EXPECT_THAT(e_int, ValueIs(3));

  const expected<std::string, int> e_string = "OK";
  EXPECT_THAT(e_string, ValueIs("OK"));
  EXPECT_THAT(e_string, ::testing::Not(ValueIs("ERROR")));

  const expected<int, std::string> e_error = unexpected("ERROR");
  EXPECT_THAT(e_error, ::testing::Not(ValueIs(3)));
}

TEST(GmockExpectedSupportTest, ValueIsOptional) {
  const std::optional<int> o_int = 3;
  EXPECT_THAT(o_int, ValueIs(3));

  const std::optional<std::string> o_string = "OK";
  EXPECT_THAT(o_string, ValueIs("OK"));
  EXPECT_THAT(o_string, ::testing::Not(ValueIs("ERROR")));

  const std::optional<int> o_nullopt = std::nullopt;
  EXPECT_THAT(o_nullopt, ::testing::Not(ValueIs(3)));
}

TEST(GmockExpectedSupportTest, ErrorIs) {
  const expected<std::string, int> e_int = unexpected(3);
  EXPECT_THAT(e_int, ErrorIs(3));

  const expected<int, std::string> e_string = unexpected("OK");
  EXPECT_THAT(e_string, ErrorIs("OK"));
  EXPECT_THAT(e_string, ::testing::Not(ErrorIs("ERROR")));

  const expected<std::string, int> e_value = "OK";
  EXPECT_THAT(e_value, ::testing::Not(ErrorIs(3)));
}

TEST(GmockExpectedSupportTest, MismatchExplanationOptional) {
  const std::optional<int> o_nullopt = std::nullopt;
  EXPECT_EQ(Explain(HasValue(), o_nullopt), "which is nullopt");
  EXPECT_EQ(Explain(ValueIs(3), o_nullopt), "which is nullopt");
}

TEST(GmockExpectedSupportTest, MismatchExplanationExpected) {
  const expected<int, std::string> e_error = unexpected("Uh oh");
  EXPECT_EQ(Explain(HasValue(), e_error), "which has the error Uh oh");
  EXPECT_EQ(Explain(ValueIs(3), e_error), "which has the error Uh oh");
}

}  // namespace
}  // namespace base::test
