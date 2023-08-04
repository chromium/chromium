// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
