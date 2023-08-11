// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/types/id_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class Foo;
using FooId = IdType<Foo, int, 0>;

// A type that uses both 0 and -1 as invalid values.
using MultipleInvalidId = IdType<class MultipleInvalid, int, 0, 1, -1>;

}  // namespace

TEST(IdType, DefaultValueIsInvalid) {
  FooId foo_id;
  EXPECT_TRUE(foo_id.is_null());

  MultipleInvalidId multi_id;
  EXPECT_TRUE(multi_id.is_null());
}

TEST(IdType, NormalValueIsValid) {
  FooId foo_id = FooId::FromUnsafeValue(123);
  EXPECT_FALSE(foo_id.is_null());

  MultipleInvalidId multi_id = MultipleInvalidId::FromUnsafeValue(123);
  EXPECT_FALSE(multi_id.is_null());
}

TEST(IdType, ExtraInvalidValue) {
  MultipleInvalidId multi_id = MultipleInvalidId::FromUnsafeValue(-1);
  EXPECT_TRUE(multi_id.is_null());
}

TEST(IdType, Generator) {
  FooId::Generator foo_id_generator;
  for (int i = 1; i < 10; i++)
    EXPECT_EQ(foo_id_generator.GenerateNextId(), FooId::FromUnsafeValue(i));
}

TEST(IdType, GeneratorWithNonZeroInvalidValue) {
  using TestId = IdType<class TestIdTag, int, -1>;

  TestId::Generator test_id_generator;
  for (int i = 0; i < 10; i++)
    EXPECT_EQ(test_id_generator.GenerateNextId(), TestId::FromUnsafeValue(i));
}

TEST(IdType, GeneratorWithBigUnsignedInvalidValue) {
  using TestId =
      IdType<class TestIdTag, uint32_t, std::numeric_limits<uint32_t>::max()>;

  TestId::Generator test_id_generator;
  for (int i = 0; i < 10; i++) {
    TestId id = test_id_generator.GenerateNextId();
    EXPECT_FALSE(id.is_null());
    EXPECT_EQ(id, TestId::FromUnsafeValue(i));
  }
}

TEST(IdType, GeneratorWithDifferentStartingValue) {
  using TestId = IdType<class TestIdTag, int, -1, 1>;

  TestId::Generator test_id_generator;
  for (int i = 1; i < 10; i++)
    EXPECT_EQ(test_id_generator.GenerateNextId(), TestId::FromUnsafeValue(i));
}

TEST(IdType, EnsureConstexpr) {
  using TestId = IdType32<class TestTag>;

  // Test constructors.
  static constexpr TestId kZero;
  static constexpr auto kOne = TestId::FromUnsafeValue(1);

  // Test getting the underlying value.
  static_assert(kZero.value() == 0, "");
  static_assert(kOne.value() == 1, "");
  static_assert(kZero.GetUnsafeValue() == 0, "");
  static_assert(kOne.GetUnsafeValue() == 1, "");

  static constexpr MultipleInvalidId kMultiZero;
  static constexpr auto kMultiNegative = MultipleInvalidId::FromUnsafeValue(-1);
  static constexpr auto kMultiOne = MultipleInvalidId::FromUnsafeValue(1);

  // Test is_null().
  static_assert(kZero.is_null(), "");
  static_assert(!kOne.is_null(), "");
  static_assert(kMultiZero.is_null(), "");
  static_assert(kMultiNegative.is_null(), "");
  static_assert(!kMultiOne.is_null(), "");

  // Test operator bool.
  static_assert(!kZero, "");
  static_assert(kOne, "");
  static_assert(!kMultiZero, "");
  static_assert(!kMultiNegative, "");
  static_assert(kMultiOne, "");
}

class IdTypeSpecificValueTest : public ::testing::TestWithParam<int> {
 protected:
  FooId test_id() { return FooId::FromUnsafeValue(GetParam()); }

  FooId other_id() {
    if (GetParam() != std::numeric_limits<int>::max())
      return FooId::FromUnsafeValue(GetParam() + 1);
    else
      return FooId::FromUnsafeValue(std::numeric_limits<int>::min());
  }
};

TEST_P(IdTypeSpecificValueTest, UnsafeValueRoundtrips) {
  int original_value = GetParam();
  FooId id = FooId::FromUnsafeValue(original_value);
  int final_value = id.GetUnsafeValue();
  EXPECT_EQ(original_value, final_value);
}

INSTANTIATE_TEST_SUITE_P(All,
                         IdTypeSpecificValueTest,
                         ::testing::Values(std::numeric_limits<int>::min(),
                                           -1,
                                           0,
                                           1,
                                           123,
                                           std::numeric_limits<int>::max()));

}  // namespace base
