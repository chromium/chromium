// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/optional_util.h"

#include <memory>
#include <optional>
#include <string>

#include "base/types/expected.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(OptionalUtilTest, OptionalToPtr) {
  std::optional<float> optional;
  EXPECT_EQ(nullptr, OptionalToPtr(optional));

  optional = 0.1f;
  EXPECT_EQ(&optional.value(), OptionalToPtr(optional));
  EXPECT_NE(nullptr, OptionalToPtr(optional));
}

TEST(OptionalUtilTest, OptionalFromPtr) {
  float* f_ptr = nullptr;
  EXPECT_EQ(std::nullopt, OptionalFromPtr(f_ptr));

  float f = 0.1f;
  std::optional<float> optional_f(f);
  EXPECT_EQ(optional_f, OptionalFromPtr(&f));
}

TEST(OptionalUtilTest, OptionalToExpected) {
  std::optional<int> i_opt;

  // No conversions.
  base::expected<int, int> i_exp = OptionalToExpected(i_opt, -1);
  EXPECT_EQ(i_exp, base::unexpected(-1));

  // Error type converted.
  i_exp = OptionalToExpected(i_opt, -1.0);
  EXPECT_EQ(i_exp, base::unexpected(-1));

  i_opt = 2;

  // No conversions.
  i_exp = OptionalToExpected(i_opt, -1);
  EXPECT_EQ(i_exp, base::ok(2));

  // Value type converted.
  base::expected<float, int> f_exp = OptionalToExpected(i_opt, -1);
  EXPECT_EQ(f_exp, base::ok(2.0));

  // Non-movable error type. "is null" is a const char array, which must be
  // copied before converting to a string. Forces the compiler to choose the
  // OptionalToExpected override that copies its error argument, to validate
  // that it's copied correctly.
  auto exp_with_str_error =
      OptionalToExpected<int, std::string>(std::nullopt, "is null");
  EXPECT_EQ(exp_with_str_error, base::unexpected("is null"));

  // Non-copyable error type. Forces the compiler to choose the
  // OptionalToExpected override that moves its error argument, to validate that
  // it's moved correctly.
  auto exp_with_ptr_error = OptionalToExpected<int, std::unique_ptr<int>>(
      std::nullopt, std::make_unique<int>(-1));
  ASSERT_FALSE(exp_with_ptr_error.has_value());
  EXPECT_EQ(*(exp_with_ptr_error.error()), -1);
}

TEST(OptionalUtilTest, OptionalFromExpected) {
  base::expected<int, std::string> i_exp = base::unexpected("uninitialized");

  // No conversion.
  std::optional<int> i_opt = OptionalFromExpected(i_exp);
  EXPECT_EQ(i_opt, std::nullopt);

  // Value type converted.
  std::optional<float> f_opt = OptionalFromExpected(i_exp);
  EXPECT_EQ(f_opt, std::nullopt);

  i_exp = base::ok(1);

  // No conversion.
  i_opt = OptionalFromExpected(i_exp);
  EXPECT_EQ(i_opt, 1);

  // Value type converted.
  f_opt = OptionalFromExpected(i_exp);
  EXPECT_EQ(f_opt, 1.0);
}

}  // namespace
}  // namespace base
