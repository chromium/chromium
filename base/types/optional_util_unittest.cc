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

// Basic test.
TEST(OptionalUtilTest, OptionalUnwrapTo_Basic) {
  int i = -404;
  EXPECT_FALSE(OptionalUnwrapTo(std::optional<int>(), i));
  EXPECT_EQ(i, -404);
  EXPECT_TRUE(OptionalUnwrapTo(std::optional<int>(5), i));
  EXPECT_EQ(i, 5);
}

// Test projection to a different type.
TEST(OptionalUtilTest, OptionalUnwrapTo_ProjectionLambda) {
  struct S {
    int i = -404;
  };
  S s;
  EXPECT_FALSE(
      OptionalUnwrapTo(std::optional<int>(), s, [](int i) { return S(i); }));
  EXPECT_EQ(s.i, -404);
  EXPECT_TRUE(
      OptionalUnwrapTo(std::optional<int>(5), s, [](int i) { return S(i); }));
  EXPECT_EQ(s.i, 5);
}

// Test projection through a non-lambda function.
TEST(OptionalUtilTest, OptionalUnwrapTo_ProjectionFunction) {
  struct S {
    int i = 0;

    static int IntoInt(S s) { return s.i; }
  };
  int i = -404;
  EXPECT_FALSE(OptionalUnwrapTo(std::optional<S>(), i, S::IntoInt));
  EXPECT_EQ(i, -404);
  EXPECT_TRUE(OptionalUnwrapTo(std::optional<S>(S{5}), i, S::IntoInt));
  EXPECT_EQ(i, 5);
}

// Test projection through a method.
TEST(OptionalUtilTest, OptionalUnwrapTo_ProjectionMethod) {
  struct S {
    int i = 0;

    int IntoInt() const { return i; }
  };
  int i = -404;
  EXPECT_FALSE(OptionalUnwrapTo(std::optional<S>(), i, &S::IntoInt));
  EXPECT_EQ(i, -404);
  EXPECT_TRUE(OptionalUnwrapTo(std::optional<S>(S{5}), i, &S::IntoInt));
  EXPECT_EQ(i, 5);
}

// Verify const ref of optional<T> are passed as const T& to projection.
TEST(OptionalUtilTest, OptionalUnwrapTo_ConstRefOptional) {
  struct NoCopyMove {
    explicit NoCopyMove(int i) : i(i) {}
    NoCopyMove(NoCopyMove&&) = delete;
    NoCopyMove& operator=(NoCopyMove&&) = delete;

    int i = 0;
  };
  std::optional<NoCopyMove> empty;
  int out = -404;
  EXPECT_FALSE(
      OptionalUnwrapTo(empty, out, [](const NoCopyMove& n) { return n.i; }));
  EXPECT_EQ(out, -404);
  std::optional<NoCopyMove> full(std::in_place, 5);
  EXPECT_TRUE(
      OptionalUnwrapTo(full, out, [](const NoCopyMove& n) { return n.i; }));
  EXPECT_EQ(out, 5);
}

// Verify rvalue of optional<T> are passed as rvalue T to projection.
TEST(OptionalUtilTest, OptionalUnwrapTo_RvalueOptional) {
  struct MoveOnly {
    explicit MoveOnly(int i) : i(i) {}
    MoveOnly(MoveOnly&&) = default;
    MoveOnly& operator=(MoveOnly&&) = default;

    int i = 0;
  };
  int out = -404;
  EXPECT_FALSE(OptionalUnwrapTo(std::optional<MoveOnly>(), out,
                                [](MoveOnly&& n) { return n.i; }));
  EXPECT_EQ(out, -404);
  EXPECT_TRUE(OptionalUnwrapTo(std::optional<MoveOnly>(std::in_place, 5), out,
                               [](MoveOnly&& n) { return n.i; }));
  EXPECT_EQ(out, 5);
}

// The output type is not the same, but it's assignable.
TEST(OptionalUtilTest, OptionalUnwrapTo_AssignableOutput) {
  struct Assignable {
    Assignable() = default;
    Assignable(int) = delete;

    void operator=(int ii) { i = ii; }

    int i = -404;
  };
  Assignable out;
  EXPECT_FALSE(OptionalUnwrapTo(std::optional<int>(), out));
  EXPECT_EQ(out.i, -404);
  EXPECT_TRUE(OptionalUnwrapTo(std::optional<int>(5), out));
  EXPECT_EQ(out.i, 5);
}

}  // namespace
}  // namespace base
