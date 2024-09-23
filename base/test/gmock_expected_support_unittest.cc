// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/test/gmock_expected_support.h"

#include <string>

#include "base/types/expected.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {
namespace {

TEST(GmockExpectedSupportTest, AssertOkAndAssign) {
  const expected<int, std::string> e_int = 1;
  ASSERT_OK_AND_ASSIGN(int result, e_int);
  EXPECT_EQ(1, result);
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

TEST(GmockExpectedSupportTest, ValueIs) {
  const expected<int, std::string> e_int = 3;
  EXPECT_THAT(e_int, ValueIs(3));

  const expected<std::string, int> e_string = "OK";
  EXPECT_THAT(e_string, ValueIs("OK"));
  EXPECT_THAT(e_string, ::testing::Not(ValueIs("ERROR")));

  const expected<int, std::string> e_error = unexpected("ERROR");
  EXPECT_THAT(e_error, ::testing::Not(ValueIs(3)));
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

}  // namespace
}  // namespace base::test
