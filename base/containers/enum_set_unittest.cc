// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/enum_set.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

enum class TestEnum {
  TEST_1 = 1,
  TEST_MIN = TEST_1,
  TEST_2,
  TEST_3,
  TEST_4,
  TEST_5,
  TEST_MAX = TEST_5,
  TEST_6_OUT_OF_BOUNDS
};
using TestEnumSet = EnumSet<TestEnum, TestEnum::TEST_MIN, TestEnum::TEST_MAX>;

enum class TestEnumExtreme {
  TEST_0 = 0,
  TEST_MIN = TEST_0,
  TEST_63 = 63,
  TEST_MAX = TEST_63,
  TEST_64_OUT_OF_BOUNDS,
};
using TestEnumExtremeSet = EnumSet<TestEnumExtreme,
                                   TestEnumExtreme::TEST_MIN,
                                   TestEnumExtreme::TEST_MAX>;

class EnumSetTest : public ::testing::Test {};

TEST_F(EnumSetTest, ClassConstants) {
  TestEnumSet enums;
  EXPECT_EQ(TestEnum::TEST_MIN, TestEnumSet::kMinValue);
  EXPECT_EQ(TestEnum::TEST_MAX, TestEnumSet::kMaxValue);
  EXPECT_EQ(static_cast<size_t>(5), TestEnumSet::kValueCount);
}

// Use static_assert to check that functions we expect to be compile time
// evaluatable are really that way.
TEST_F(EnumSetTest, ConstexprsAreValid) {
  static_assert(TestEnumSet::All().Has(TestEnum::TEST_2),
                "expected All() to be integral constant expression");
  static_assert(TestEnumSet::FromRange(TestEnum::TEST_2, TestEnum::TEST_4)
                    .Has(TestEnum::TEST_2),
                "expected FromRange() to be integral constant expression");
  static_assert(TestEnumSet(TestEnum::TEST_2).Has(TestEnum::TEST_2),
                "expected TestEnumSet() to be integral constant expression");
  static_assert(
      TestEnumSet::FromEnumBitmask(1 << static_cast<uint64_t>(TestEnum::TEST_2))
          .Has(TestEnum::TEST_2),
      "Expected TestEnumSet() to be integral constant expression");
}

TEST_F(EnumSetTest, DefaultConstructor) {
  const TestEnumSet enums;
  EXPECT_TRUE(enums.Empty());
  EXPECT_EQ(static_cast<size_t>(0), enums.Size());
  EXPECT_FALSE(enums.Has(TestEnum::TEST_1));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_2));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_3));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_4));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_5));
}

TEST_F(EnumSetTest, OneArgConstructor) {
  const TestEnumSet enums(TestEnum::TEST_4);
  EXPECT_FALSE(enums.Empty());
  EXPECT_EQ(static_cast<size_t>(1), enums.Size());
  EXPECT_FALSE(enums.Has(TestEnum::TEST_1));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_2));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_3));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_4));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_5));
}

TEST_F(EnumSetTest, OneArgConstructorSize) {
  TestEnumExtremeSet enums(TestEnumExtreme::TEST_0);
  EXPECT_TRUE(enums.Has(TestEnumExtreme::TEST_0));
}

TEST_F(EnumSetTest, TwoArgConstructor) {
  const TestEnumSet enums(TestEnum::TEST_4, TestEnum::TEST_2);
  EXPECT_FALSE(enums.Empty());
  EXPECT_EQ(static_cast<size_t>(2), enums.Size());
  EXPECT_FALSE(enums.Has(TestEnum::TEST_1));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_2));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_3));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_4));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_5));
}

TEST_F(EnumSetTest, ThreeArgConstructor) {
  const TestEnumSet enums(TestEnum::TEST_4, TestEnum::TEST_2, TestEnum::TEST_1);
  EXPECT_FALSE(enums.Empty());
  EXPECT_EQ(static_cast<size_t>(3), enums.Size());
  EXPECT_TRUE(enums.Has(TestEnum::TEST_1));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_2));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_3));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_4));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_5));
}

TEST_F(EnumSetTest, DuplicatesInConstructor) {
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_4, TestEnum::TEST_2, TestEnum::TEST_1,
                        TestEnum::TEST_4, TestEnum::TEST_2, TestEnum::TEST_4),
            TestEnumSet(TestEnum::TEST_1, TestEnum::TEST_2, TestEnum::TEST_4));
}

TEST_F(EnumSetTest, All) {
  const TestEnumSet enums(TestEnumSet::All());
  EXPECT_FALSE(enums.Empty());
  EXPECT_EQ(static_cast<size_t>(5), enums.Size());
  EXPECT_TRUE(enums.Has(TestEnum::TEST_1));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_2));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_3));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_4));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_5));
}

TEST_F(EnumSetTest, FromRange) {
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_2, TestEnum::TEST_3, TestEnum::TEST_4),
            TestEnumSet::FromRange(TestEnum::TEST_2, TestEnum::TEST_4));
  EXPECT_EQ(TestEnumSet::All(),
            TestEnumSet::FromRange(TestEnum::TEST_1, TestEnum::TEST_5));
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_2),
            TestEnumSet::FromRange(TestEnum::TEST_2, TestEnum::TEST_2));

  using RestrictedRangeSet =
      EnumSet<TestEnum, TestEnum::TEST_2, TestEnum::TEST_MAX>;
  EXPECT_EQ(
      RestrictedRangeSet(TestEnum::TEST_2, TestEnum::TEST_3, TestEnum::TEST_4),
      RestrictedRangeSet::FromRange(TestEnum::TEST_2, TestEnum::TEST_4));
  EXPECT_EQ(RestrictedRangeSet::All(),
            RestrictedRangeSet::FromRange(TestEnum::TEST_2, TestEnum::TEST_5));
}

TEST_F(EnumSetTest, Put) {
  TestEnumSet enums(TestEnum::TEST_4);
  enums.Put(TestEnum::TEST_3);
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_3, TestEnum::TEST_4), enums);
  enums.Put(TestEnum::TEST_5);
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_3, TestEnum::TEST_4, TestEnum::TEST_5),
            enums);
}

TEST_F(EnumSetTest, PutAll) {
  TestEnumSet enums(TestEnum::TEST_4, TestEnum::TEST_5);
  enums.PutAll(TestEnumSet(TestEnum::TEST_3, TestEnum::TEST_4));
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_3, TestEnum::TEST_4, TestEnum::TEST_5),
            enums);
}

TEST_F(EnumSetTest, PutRange) {
  TestEnumSet enums;
  enums.PutRange(TestEnum::TEST_2, TestEnum::TEST_4);
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_2, TestEnum::TEST_3, TestEnum::TEST_4),
            enums);
}

TEST_F(EnumSetTest, RetainAll) {
  TestEnumSet enums(TestEnum::TEST_4, TestEnum::TEST_5);
  enums.RetainAll(TestEnumSet(TestEnum::TEST_3, TestEnum::TEST_4));
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_4), enums);
}

TEST_F(EnumSetTest, Remove) {
  TestEnumSet enums(TestEnum::TEST_4, TestEnum::TEST_5);
  enums.Remove(TestEnum::TEST_1);
  enums.Remove(TestEnum::TEST_3);
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_4, TestEnum::TEST_5), enums);
  enums.Remove(TestEnum::TEST_4);
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_5), enums);
  enums.Remove(TestEnum::TEST_5);
  enums.Remove(TestEnum::TEST_6_OUT_OF_BOUNDS);
  EXPECT_TRUE(enums.Empty());
}

TEST_F(EnumSetTest, RemoveAll) {
  TestEnumSet enums(TestEnum::TEST_4, TestEnum::TEST_5);
  enums.RemoveAll(TestEnumSet(TestEnum::TEST_3, TestEnum::TEST_4));
  EXPECT_EQ(TestEnumSet(TestEnum::TEST_5), enums);
}

TEST_F(EnumSetTest, Clear) {
  TestEnumSet enums(TestEnum::TEST_4, TestEnum::TEST_5);
  enums.Clear();
  EXPECT_TRUE(enums.Empty());
}

TEST_F(EnumSetTest, Has) {
  const TestEnumSet enums(TestEnum::TEST_4, TestEnum::TEST_5);
  EXPECT_FALSE(enums.Has(TestEnum::TEST_1));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_2));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_3));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_4));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_5));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_6_OUT_OF_BOUNDS));
}

TEST_F(EnumSetTest, HasAll) {
  const TestEnumSet enums1(TestEnum::TEST_4, TestEnum::TEST_5);
  const TestEnumSet enums2(TestEnum::TEST_3, TestEnum::TEST_4);
  const TestEnumSet enums3 = Union(enums1, enums2);
  EXPECT_TRUE(enums1.HasAll(enums1));
  EXPECT_FALSE(enums1.HasAll(enums2));
  EXPECT_FALSE(enums1.HasAll(enums3));

  EXPECT_FALSE(enums2.HasAll(enums1));
  EXPECT_TRUE(enums2.HasAll(enums2));
  EXPECT_FALSE(enums2.HasAll(enums3));

  EXPECT_TRUE(enums3.HasAll(enums1));
  EXPECT_TRUE(enums3.HasAll(enums2));
  EXPECT_TRUE(enums3.HasAll(enums3));
}

TEST_F(EnumSetTest, Iterators) {
  const TestEnumSet enums1(TestEnum::TEST_4, TestEnum::TEST_5);
  TestEnumSet enums2;
  for (TestEnumSet::Iterator it = enums1.begin(); it != enums1.end(); it++) {
    enums2.Put(*it);
  }
  EXPECT_EQ(enums2, enums1);
}

TEST_F(EnumSetTest, RangeBasedForLoop) {
  const TestEnumSet enums1(TestEnum::TEST_2, TestEnum::TEST_5,
                           TestEnum::TEST_6_OUT_OF_BOUNDS);
  TestEnumSet enums2;
  for (TestEnum e : enums1) {
    enums2.Put(e);
  }
  EXPECT_EQ(enums2, enums1);
}

TEST_F(EnumSetTest, IteratorComparisonOperators) {
  const TestEnumSet enums(TestEnum::TEST_2, TestEnum::TEST_4,
                          TestEnum::TEST_6_OUT_OF_BOUNDS);
  const auto first_it = enums.begin();
  const auto second_it = ++enums.begin();

  // Copy for equality testing.
  const auto first_it_copy = first_it;

  // Sanity check, as the rest of the test relies on |first_it| and
  // |first_it_copy| pointing to the same element and |first_it| and |second_it|
  // pointing to different elements.
  ASSERT_EQ(*first_it, *first_it_copy);
  ASSERT_NE(*first_it, *second_it);

  EXPECT_TRUE(first_it == first_it_copy);
  EXPECT_FALSE(first_it != first_it_copy);

  EXPECT_TRUE(first_it != second_it);
  EXPECT_FALSE(first_it == second_it);
}

TEST_F(EnumSetTest, IteratorIncrementOperators) {
  const TestEnumSet enums(TestEnum::TEST_2, TestEnum::TEST_4,
                          TestEnum::TEST_6_OUT_OF_BOUNDS);
  const auto begin = enums.begin();

  auto post_inc_it = begin;
  auto pre_inc_it = begin;

  auto post_inc_return_it = post_inc_it++;
  auto pre_inc_return_it = ++pre_inc_it;

  // |pre_inc_it| and |post_inc_it| should point to the same element.
  EXPECT_EQ(pre_inc_it, post_inc_it);
  EXPECT_EQ(*pre_inc_it, *post_inc_it);

  // |pre_inc_it| should NOT point to the first element.
  EXPECT_NE(begin, pre_inc_it);
  EXPECT_NE(*begin, *pre_inc_it);

  // |post_inc_it| should NOT point to the first element.
  EXPECT_NE(begin, post_inc_it);
  EXPECT_NE(*begin, *post_inc_it);

  // Prefix increment should return new iterator.
  EXPECT_EQ(pre_inc_return_it, post_inc_it);
  EXPECT_EQ(*pre_inc_return_it, *post_inc_it);

  // Postfix increment should return original iterator.
  EXPECT_EQ(post_inc_return_it, begin);
  EXPECT_EQ(*post_inc_return_it, *begin);
}

TEST_F(EnumSetTest, Union) {
  const TestEnumSet enums1(TestEnum::TEST_4, TestEnum::TEST_5);
  const TestEnumSet enums2(TestEnum::TEST_3, TestEnum::TEST_4);
  const TestEnumSet enums3 = Union(enums1, enums2);

  EXPECT_EQ(TestEnumSet(TestEnum::TEST_3, TestEnum::TEST_4, TestEnum::TEST_5),
            enums3);
}

TEST_F(EnumSetTest, Intersection) {
  const TestEnumSet enums1(TestEnum::TEST_4, TestEnum::TEST_5);
  const TestEnumSet enums2(TestEnum::TEST_3, TestEnum::TEST_4);
  const TestEnumSet enums3 = Intersection(enums1, enums2);

  EXPECT_EQ(TestEnumSet(TestEnum::TEST_4), enums3);
}

TEST_F(EnumSetTest, Difference) {
  const TestEnumSet enums1(TestEnum::TEST_4, TestEnum::TEST_5);
  const TestEnumSet enums2(TestEnum::TEST_3, TestEnum::TEST_4);
  const TestEnumSet enums3 = Difference(enums1, enums2);

  EXPECT_EQ(TestEnumSet(TestEnum::TEST_5), enums3);
}

TEST_F(EnumSetTest, ToFromEnumBitmask) {
  const TestEnumSet empty;
  EXPECT_EQ(empty.ToEnumBitmask(), 0ULL);
  EXPECT_EQ(TestEnumSet::FromEnumBitmask(0), empty);

  const TestEnumSet enums1(TestEnum::TEST_2);
  const uint64_t val1 = 1ULL << static_cast<uint64_t>(TestEnum::TEST_2);
  EXPECT_EQ(enums1.ToEnumBitmask(), val1);
  EXPECT_EQ(TestEnumSet::FromEnumBitmask(val1), enums1);

  const TestEnumSet enums2(TestEnum::TEST_3, TestEnum::TEST_4);
  const uint64_t val2 = 1ULL << static_cast<uint64_t>(TestEnum::TEST_3) |
                        1ULL << static_cast<uint64_t>(TestEnum::TEST_4);
  EXPECT_EQ(enums2.ToEnumBitmask(), val2);
  EXPECT_EQ(TestEnumSet::FromEnumBitmask(val2), enums2);
}

TEST_F(EnumSetTest, ToFromEnumBitmaskExtreme) {
  const TestEnumExtremeSet empty;
  EXPECT_EQ(empty.ToEnumBitmask(), 0ULL);
  EXPECT_EQ(TestEnumExtremeSet::FromEnumBitmask(0ULL), empty);

  const TestEnumExtremeSet enums1(TestEnumExtreme::TEST_63);
  const uint64_t val1 = 1ULL << static_cast<uint64_t>(TestEnumExtreme::TEST_63);
  EXPECT_EQ(enums1.ToEnumBitmask(), val1);
  EXPECT_EQ(TestEnumExtremeSet::FromEnumBitmask(val1), enums1);
}

}  // namespace
}  // namespace base
