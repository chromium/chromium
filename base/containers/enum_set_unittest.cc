// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/enum_set.h"

#include <stddef.h>

#include "base/containers/to_vector.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

enum class TestEnum {
  TEST_BELOW_MIN_NEGATIVE = -1,
  TEST_BELOW_MIN = 0,
  TEST_1 = 1,
  TEST_MIN = TEST_1,
  TEST_2,
  TEST_3,
  TEST_4,
  TEST_5,
  TEST_MAX = TEST_5,
  TEST_6_OUT_OF_BOUNDS,
  TEST_7_OUT_OF_BOUNDS
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
class EnumSetDeathTest : public ::testing::Test {};

TEST_F(EnumSetTest, ClassConstants) {
  EXPECT_EQ(TestEnum::TEST_MIN, TestEnumSet::kMinValue);
  EXPECT_EQ(TestEnum::TEST_MAX, TestEnumSet::kMaxValue);
  EXPECT_EQ(5u, TestEnumSet::kValueCount);
}

// Use static_assert to check that functions we expect to be compile time
// evaluatable are really that way.
TEST_F(EnumSetTest, ConstexprsAreValid) {
  static_assert(TestEnumSet::All().Has(TestEnum::TEST_2),
                "Expected All() to be integral constant expression");
  static_assert(TestEnumSet::FromRange(TestEnum::TEST_2, TestEnum::TEST_4)
                    .Has(TestEnum::TEST_2),
                "Expected FromRange() to be integral constant expression");
  static_assert(TestEnumSet{TestEnum::TEST_2}.Has(TestEnum::TEST_2),
                "Expected TestEnumSet() to be integral constant expression");
  static_assert(
      TestEnumSet::FromEnumBitmask(1 << static_cast<uint64_t>(TestEnum::TEST_2))
          .Has(TestEnum::TEST_2),
      "Expected TestEnumSet() to be integral constant expression");
}

TEST_F(EnumSetTest, DefaultConstructor) {
  const TestEnumSet enums;
  EXPECT_TRUE(enums.Empty());
  EXPECT_EQ(0u, enums.Size());
  EXPECT_FALSE(enums.Has(TestEnum::TEST_1));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_2));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_3));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_4));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_5));
}

TEST_F(EnumSetTest, OneArgConstructor) {
  const TestEnumSet enums = {TestEnum::TEST_4};
  EXPECT_FALSE(enums.Empty());
  EXPECT_EQ(1u, enums.Size());
  EXPECT_FALSE(enums.Has(TestEnum::TEST_1));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_2));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_3));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_4));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_5));
}

TEST_F(EnumSetTest, OneArgConstructorSize) {
  TestEnumExtremeSet enums = {TestEnumExtreme::TEST_0};
  EXPECT_TRUE(enums.Has(TestEnumExtreme::TEST_0));
}

TEST_F(EnumSetTest, TwoArgConstructor) {
  const TestEnumSet enums = {TestEnum::TEST_4, TestEnum::TEST_2};
  EXPECT_FALSE(enums.Empty());
  EXPECT_EQ(2u, enums.Size());
  EXPECT_FALSE(enums.Has(TestEnum::TEST_1));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_2));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_3));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_4));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_5));
}

TEST_F(EnumSetTest, ThreeArgConstructor) {
  const TestEnumSet enums = {TestEnum::TEST_4, TestEnum::TEST_2,
                             TestEnum::TEST_1};
  EXPECT_FALSE(enums.Empty());
  EXPECT_EQ(3u, enums.Size());
  EXPECT_TRUE(enums.Has(TestEnum::TEST_1));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_2));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_3));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_4));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_5));
}

TEST_F(EnumSetTest, DuplicatesInConstructor) {
  EXPECT_EQ(
      TestEnumSet({TestEnum::TEST_4, TestEnum::TEST_2, TestEnum::TEST_1,
                   TestEnum::TEST_4, TestEnum::TEST_2, TestEnum::TEST_4}),
      TestEnumSet({TestEnum::TEST_1, TestEnum::TEST_2, TestEnum::TEST_4}));
}

TEST_F(EnumSetTest, All) {
  const TestEnumSet enums(TestEnumSet::All());
  EXPECT_FALSE(enums.Empty());
  EXPECT_EQ(5u, enums.Size());
  EXPECT_TRUE(enums.Has(TestEnum::TEST_1));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_2));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_3));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_4));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_5));
}

TEST_F(EnumSetTest, AllExtreme) {
  const TestEnumExtremeSet enums(TestEnumExtremeSet::All());
  EXPECT_FALSE(enums.Empty());
  EXPECT_EQ(64u, enums.Size());
  EXPECT_TRUE(enums.Has(TestEnumExtreme::TEST_0));
  EXPECT_TRUE(enums.Has(TestEnumExtreme::TEST_63));
  EXPECT_FALSE(enums.Has(TestEnumExtreme::TEST_64_OUT_OF_BOUNDS));
}

TEST_F(EnumSetTest, FromRange) {
  EXPECT_EQ(TestEnumSet({TestEnum::TEST_2, TestEnum::TEST_3, TestEnum::TEST_4}),
            TestEnumSet::FromRange(TestEnum::TEST_2, TestEnum::TEST_4));
  EXPECT_EQ(TestEnumSet::All(),
            TestEnumSet::FromRange(TestEnum::TEST_1, TestEnum::TEST_5));
  EXPECT_EQ(TestEnumSet({TestEnum::TEST_2}),
            TestEnumSet::FromRange(TestEnum::TEST_2, TestEnum::TEST_2));

  using RestrictedRangeSet =
      EnumSet<TestEnum, TestEnum::TEST_2, TestEnum::TEST_MAX>;
  EXPECT_EQ(RestrictedRangeSet(
                {TestEnum::TEST_2, TestEnum::TEST_3, TestEnum::TEST_4}),
            RestrictedRangeSet::FromRange(TestEnum::TEST_2, TestEnum::TEST_4));
  EXPECT_EQ(RestrictedRangeSet::All(),
            RestrictedRangeSet::FromRange(TestEnum::TEST_2, TestEnum::TEST_5));
}

TEST_F(EnumSetTest, Put) {
  TestEnumSet enums = {TestEnum::TEST_4};
  enums.Put(TestEnum::TEST_3);
  EXPECT_EQ(TestEnumSet({TestEnum::TEST_3, TestEnum::TEST_4}), enums);
  enums.Put(TestEnum::TEST_5);
  EXPECT_EQ(TestEnumSet({TestEnum::TEST_3, TestEnum::TEST_4, TestEnum::TEST_5}),
            enums);
}

TEST_F(EnumSetTest, PutAll) {
  TestEnumSet enums = {TestEnum::TEST_4, TestEnum::TEST_5};
  enums.PutAll({TestEnum::TEST_3, TestEnum::TEST_4});
  EXPECT_EQ(TestEnumSet({TestEnum::TEST_3, TestEnum::TEST_4, TestEnum::TEST_5}),
            enums);
}

TEST_F(EnumSetTest, PutRange) {
  TestEnumSet enums;
  enums.PutRange(TestEnum::TEST_2, TestEnum::TEST_4);
  EXPECT_EQ(TestEnumSet({TestEnum::TEST_2, TestEnum::TEST_3, TestEnum::TEST_4}),
            enums);
}

TEST_F(EnumSetTest, RetainAll) {
  TestEnumSet enums = {TestEnum::TEST_4, TestEnum::TEST_5};
  enums.RetainAll(TestEnumSet({TestEnum::TEST_3, TestEnum::TEST_4}));
  EXPECT_EQ(TestEnumSet({TestEnum::TEST_4}), enums);
}

TEST_F(EnumSetTest, Remove) {
  TestEnumSet enums = {TestEnum::TEST_4, TestEnum::TEST_5};
  enums.Remove(TestEnum::TEST_1);
  enums.Remove(TestEnum::TEST_3);
  EXPECT_EQ(TestEnumSet({TestEnum::TEST_4, TestEnum::TEST_5}), enums);
  enums.Remove(TestEnum::TEST_4);
  EXPECT_EQ(TestEnumSet({TestEnum::TEST_5}), enums);
  enums.Remove(TestEnum::TEST_5);
  enums.Remove(TestEnum::TEST_6_OUT_OF_BOUNDS);
  EXPECT_TRUE(enums.Empty());
}

TEST_F(EnumSetTest, RemoveAll) {
  TestEnumSet enums = {TestEnum::TEST_4, TestEnum::TEST_5};
  enums.RemoveAll(TestEnumSet({TestEnum::TEST_3, TestEnum::TEST_4}));
  EXPECT_EQ(TestEnumSet({TestEnum::TEST_5}), enums);
}

TEST_F(EnumSetTest, Clear) {
  TestEnumSet enums = {TestEnum::TEST_4, TestEnum::TEST_5};
  enums.Clear();
  EXPECT_TRUE(enums.Empty());
}

TEST_F(EnumSetTest, Set) {
  TestEnumSet enums;
  EXPECT_TRUE(enums.Empty());

  enums.PutOrRemove(TestEnum::TEST_3, false);
  EXPECT_TRUE(enums.Empty());

  enums.PutOrRemove(TestEnum::TEST_4, true);
  EXPECT_EQ(enums, TestEnumSet({TestEnum::TEST_4}));

  enums.PutOrRemove(TestEnum::TEST_5, true);
  EXPECT_EQ(enums, TestEnumSet({TestEnum::TEST_4, TestEnum::TEST_5}));
  enums.PutOrRemove(TestEnum::TEST_5, true);
  EXPECT_EQ(enums, TestEnumSet({TestEnum::TEST_4, TestEnum::TEST_5}));

  enums.PutOrRemove(TestEnum::TEST_4, false);
  EXPECT_EQ(enums, TestEnumSet({TestEnum::TEST_5}));
}

TEST_F(EnumSetTest, Has) {
  const TestEnumSet enums = {TestEnum::TEST_4, TestEnum::TEST_5};
  EXPECT_FALSE(enums.Has(TestEnum::TEST_1));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_2));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_3));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_4));
  EXPECT_TRUE(enums.Has(TestEnum::TEST_5));
  EXPECT_FALSE(enums.Has(TestEnum::TEST_6_OUT_OF_BOUNDS));
}

TEST_F(EnumSetTest, HasAll) {
  const TestEnumSet enums1 = {TestEnum::TEST_4, TestEnum::TEST_5};
  const TestEnumSet enums2 = {TestEnum::TEST_3, TestEnum::TEST_4};
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

TEST_F(EnumSetTest, HasAny) {
  const TestEnumSet enums1 = {TestEnum::TEST_4, TestEnum::TEST_5};
  const TestEnumSet enums2 = {TestEnum::TEST_3, TestEnum::TEST_4};
  const TestEnumSet enums3 = {TestEnum::TEST_1, TestEnum::TEST_2};
  EXPECT_TRUE(enums1.HasAny(enums1));
  EXPECT_TRUE(enums1.HasAny(enums2));
  EXPECT_FALSE(enums1.HasAny(enums3));

  EXPECT_TRUE(enums2.HasAny(enums1));
  EXPECT_TRUE(enums2.HasAny(enums2));
  EXPECT_FALSE(enums2.HasAny(enums3));

  EXPECT_FALSE(enums3.HasAny(enums1));
  EXPECT_FALSE(enums3.HasAny(enums2));
  EXPECT_TRUE(enums3.HasAny(enums3));
}

TEST_F(EnumSetTest, Iterators) {
  const TestEnumSet enums1 = {TestEnum::TEST_4, TestEnum::TEST_5};
  TestEnumSet enums2;
  for (TestEnum e : enums1) {
    enums2.Put(e);
  }
  EXPECT_EQ(enums2, enums1);
}

TEST_F(EnumSetTest, RangeBasedForLoop) {
  const TestEnumSet enums1 = {TestEnum::TEST_2, TestEnum::TEST_5};
  TestEnumSet enums2;
  for (TestEnum e : enums1) {
    enums2.Put(e);
  }
  EXPECT_EQ(enums2, enums1);
}

TEST_F(EnumSetTest, IteratorComparisonOperators) {
  const TestEnumSet enums = {TestEnum::TEST_2, TestEnum::TEST_4};
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
  const TestEnumSet enums = {TestEnum::TEST_2, TestEnum::TEST_4};
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
  const TestEnumSet enums1 = {TestEnum::TEST_4, TestEnum::TEST_5};
  const TestEnumSet enums2 = {TestEnum::TEST_3, TestEnum::TEST_4};
  const TestEnumSet enums3 = Union(enums1, enums2);

  EXPECT_EQ(TestEnumSet({TestEnum::TEST_3, TestEnum::TEST_4, TestEnum::TEST_5}),
            enums3);
}

TEST_F(EnumSetTest, Intersection) {
  const TestEnumSet enums1 = {TestEnum::TEST_4, TestEnum::TEST_5};
  const TestEnumSet enums2 = {TestEnum::TEST_3, TestEnum::TEST_4};
  const TestEnumSet enums3 = Intersection(enums1, enums2);

  EXPECT_EQ(TestEnumSet({TestEnum::TEST_4}), enums3);
}

TEST_F(EnumSetTest, Difference) {
  const TestEnumSet enums1 = {TestEnum::TEST_4, TestEnum::TEST_5};
  const TestEnumSet enums2 = {TestEnum::TEST_3, TestEnum::TEST_4};
  const TestEnumSet enums3 = Difference(enums1, enums2);

  EXPECT_EQ(TestEnumSet({TestEnum::TEST_5}), enums3);
}

TEST_F(EnumSetTest, ToFromEnumBitmask) {
  const TestEnumSet empty;
  EXPECT_EQ(empty.ToEnumBitmask(), 0ULL);
  EXPECT_EQ(TestEnumSet::FromEnumBitmask(0), empty);

  const TestEnumSet enums1 = {TestEnum::TEST_2};
  const uint64_t val1 = 1ULL << static_cast<uint64_t>(TestEnum::TEST_2);
  EXPECT_EQ(enums1.ToEnumBitmask(), val1);
  EXPECT_EQ(TestEnumSet::FromEnumBitmask(val1), enums1);

  const TestEnumSet enums2 = {TestEnum::TEST_3, TestEnum::TEST_4};
  const uint64_t val2 = 1ULL << static_cast<uint64_t>(TestEnum::TEST_3) |
                        1ULL << static_cast<uint64_t>(TestEnum::TEST_4);
  EXPECT_EQ(enums2.ToEnumBitmask(), val2);
  EXPECT_EQ(TestEnumSet::FromEnumBitmask(val2), enums2);
}

TEST_F(EnumSetTest, ToFromEnumBitmaskExtreme) {
  const TestEnumExtremeSet empty;
  EXPECT_EQ(empty.ToEnumBitmask(), 0ULL);
  EXPECT_EQ(TestEnumExtremeSet::FromEnumBitmask(0ULL), empty);

  const TestEnumExtremeSet enums1 = {TestEnumExtreme::TEST_63};
  const uint64_t val1 = 1ULL << static_cast<uint64_t>(TestEnumExtreme::TEST_63);
  EXPECT_EQ(enums1.ToEnumBitmask(), val1);
  EXPECT_EQ(TestEnumExtremeSet::FromEnumBitmask(val1), enums1);
}

TEST_F(EnumSetTest, FromEnumBitmaskIgnoresExtraBits) {
  const TestEnumSet kSets[] = {
      {},
      {TestEnum::TEST_MIN},
      {TestEnum::TEST_MAX},
      {TestEnum::TEST_MIN, TestEnum::TEST_MAX},
      {TestEnum::TEST_MIN, TestEnum::TEST_MAX},
      {TestEnum::TEST_2, TestEnum::TEST_4},
  };
  size_t i = 0;
  for (const TestEnumSet& set : kSets) {
    SCOPED_TRACE(i++);
    const uint64_t val = set.ToEnumBitmask();

    // Produce a bitstring for a single enum value. When `e` is in range
    // relative to TestEnumSet, this function behaves identically to
    // `single_val_bitstring`. When `e` is not in range, this function attempts
    // to compute a value, while `single_val_bitstring` intentionally crashes.
    auto single_val_bitstring = [](TestEnum e) -> uint64_t {
      uint64_t shift_amount = static_cast<uint64_t>(e);
      // Shifting left more than the number of bits in the lhs would be UB.
      CHECK_LT(shift_amount, sizeof(uint64_t) * 8);
      return 1ULL << shift_amount;
    };

    const uint64_t kJunkVals[] = {
        // Add junk bits above TEST_MAX.
        val | single_val_bitstring(TestEnum::TEST_6_OUT_OF_BOUNDS),
        val | single_val_bitstring(TestEnum::TEST_7_OUT_OF_BOUNDS),
        val | single_val_bitstring(TestEnum::TEST_6_OUT_OF_BOUNDS) |
            single_val_bitstring(TestEnum::TEST_7_OUT_OF_BOUNDS),
        // Add junk bits below TEST_MIN.
        val | single_val_bitstring(TestEnum::TEST_BELOW_MIN),
    };
    for (uint64_t junk_val : kJunkVals) {
      SCOPED_TRACE(junk_val);
      ASSERT_NE(val, junk_val);

      const TestEnumSet set_from_junk = TestEnumSet::FromEnumBitmask(junk_val);
      EXPECT_EQ(set_from_junk, set);
      EXPECT_EQ(set_from_junk.ToEnumBitmask(), set.ToEnumBitmask());

      // Iterating both sets should produce the same sequence.
      auto it1 = set.begin();
      auto it2 = set_from_junk.begin();
      while (it1 != set.end() && it2 != set_from_junk.end()) {
        EXPECT_EQ(*it1, *it2);
        ++it1;
        ++it2;
      }
      EXPECT_TRUE(it1 == set.end());
      EXPECT_TRUE(it2 == set_from_junk.end());
    }
  }
}

TEST_F(EnumSetTest, OneEnumValue) {
  enum class TestEnumOne {
    kTest1 = 1,
    kTestMin = kTest1,
    kTestMax = kTest1,
  };
  using TestEnumOneSet =
      EnumSet<TestEnumOne, TestEnumOne::kTestMin, TestEnumOne::kTestMax>;
  EXPECT_EQ(TestEnumOne::kTestMin, TestEnumOneSet::kMinValue);
  EXPECT_EQ(TestEnumOne::kTestMax, TestEnumOneSet::kMaxValue);
  EXPECT_EQ(1u, TestEnumOneSet::kValueCount);
}

TEST_F(EnumSetTest, SparseEnum) {
  enum class TestEnumSparse {
    TEST_1 = 1,
    TEST_MIN = 1,
    TEST_50 = 50,
    TEST_100 = 100,
    TEST_MAX = TEST_100,
  };
  using TestEnumSparseSet = EnumSet<TestEnumSparse, TestEnumSparse::TEST_MIN,
                                    TestEnumSparse::TEST_MAX>;
  TestEnumSparseSet sparse;
  sparse.Put(TestEnumSparse::TEST_MIN);
  sparse.Put(TestEnumSparse::TEST_MAX);
  EXPECT_EQ(sparse.Size(), 2u);

  EXPECT_EQ(TestEnumSparseSet::All().Size(), 100u);
}

TEST_F(EnumSetTest, SparseEnumSmall) {
  enum class TestEnumSparse {
    TEST_1 = 1,
    TEST_MIN = 1,
    TEST_50 = 50,
    TEST_60 = 60,
    TEST_MAX = TEST_60,
  };
  using TestEnumSparseSet = EnumSet<TestEnumSparse, TestEnumSparse::TEST_MIN,
                                    TestEnumSparse::TEST_MAX>;
  TestEnumSparseSet sparse;
  sparse.Put(TestEnumSparse::TEST_MIN);
  sparse.Put(TestEnumSparse::TEST_MAX);
  EXPECT_EQ(sparse.Size(), 2u);

  // This may seem a little surprising! There are only 3 distinct values in
  // TestEnumSparse, so why does TestEnumSparseSet think it has 60 of them? This
  // is an artifact of EnumSet's design, as it has no way of knowing which
  // values between the min and max are actually named in the enum's definition.
  EXPECT_EQ(TestEnumSparseSet::All().Size(), 60u);
}

TEST_F(EnumSetDeathTest, CrashesOnOutOfRange) {
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::TEST_BELOW_MIN}));
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::TEST_6_OUT_OF_BOUNDS}));
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::TEST_7_OUT_OF_BOUNDS}));
}

TEST_F(EnumSetDeathTest, EnumWithNegatives) {
  enum class TestEnumNeg {
    TEST_BELOW_MIN = -3,
    TEST_A = -2,
    TEST_MIN = TEST_A,
    TEST_B = -1,
    TEST_C = 0,
    TEST_D = 1,
    TEST_E = 2,
    TEST_MAX = TEST_E,
    TEST_F = 3,
  };
  // This EnumSet starts negative and ends positive.
  using TestEnumWithNegSet =
      EnumSet<TestEnumNeg, TestEnumNeg::TEST_MIN, TestEnumNeg::TEST_MAX>;

  // Should crash because TEST_BELOW_MIN is not in range.
  EXPECT_CHECK_DEATH(TestEnumWithNegSet({TestEnumNeg::TEST_BELOW_MIN}));
  // TEST_D is in range, but note that TEST_MIN is negative. This should work.
  EXPECT_TRUE(
      TestEnumWithNegSet({TestEnumNeg::TEST_D}).Has(TestEnumNeg::TEST_D));
  // Even though TEST_A is negative, it is in range, so this should work.
  EXPECT_TRUE(
      TestEnumWithNegSet({TestEnumNeg::TEST_A}).Has(TestEnumNeg::TEST_A));
}

TEST_F(EnumSetDeathTest, EnumWithOnlyNegatives) {
  enum class TestEnumNeg {
    TEST_BELOW_MIN = -10,
    TEST_A = -9,
    TEST_MIN = TEST_A,
    TEST_B = -8,
    TEST_C = -7,
    TEST_D = -6,
    TEST_MAX = TEST_D,
    TEST_F = -5,
  };
  // This EnumSet starts negative and ends negative.
  using TestEnumWithNegSet =
      EnumSet<TestEnumNeg, TestEnumNeg::TEST_MIN, TestEnumNeg::TEST_MAX>;

  // Should crash because TEST_BELOW_MIN is not in range.
  EXPECT_CHECK_DEATH(TestEnumWithNegSet({TestEnumNeg::TEST_BELOW_MIN}));
  // TEST_A, TEST_D are in range, but note that TEST_MIN and values are
  // negative. This should work.
  EXPECT_TRUE(
      TestEnumWithNegSet({TestEnumNeg::TEST_A}).Has(TestEnumNeg::TEST_A));
  EXPECT_TRUE(
      TestEnumWithNegSet({TestEnumNeg::TEST_D}).Has(TestEnumNeg::TEST_D));
}

TEST_F(EnumSetDeathTest, VariadicConstructorCrashesOnOutOfRange) {
  // Constructor should crash given out-of-range values.
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::TEST_BELOW_MIN}).Empty());
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::TEST_BELOW_MIN_NEGATIVE}).Empty());
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::TEST_6_OUT_OF_BOUNDS}).Empty());
}

TEST_F(EnumSetDeathTest, FromRangeCrashesOnBadInputs) {
  // FromRange crashes when the bounds are in range, but out of order.
  EXPECT_CHECK_DEATH(
      TestEnumSet().FromRange(TestEnum::TEST_3, TestEnum::TEST_1));

  // FromRange crashes when the start value is out of range.
  EXPECT_CHECK_DEATH(
      TestEnumSet().FromRange(TestEnum::TEST_BELOW_MIN, TestEnum::TEST_1));
  EXPECT_CHECK_DEATH(TestEnumSet().FromRange(TestEnum::TEST_BELOW_MIN_NEGATIVE,
                                             TestEnum::TEST_1));
  EXPECT_CHECK_DEATH(TestEnumSet().FromRange(TestEnum::TEST_6_OUT_OF_BOUNDS,
                                             TestEnum::TEST_1));

  // FromRange crashes when the end value is out of range.
  EXPECT_CHECK_DEATH(
      TestEnumSet().FromRange(TestEnum::TEST_3, TestEnum::TEST_BELOW_MIN));
  EXPECT_CHECK_DEATH(TestEnumSet().FromRange(
      TestEnum::TEST_3, TestEnum::TEST_BELOW_MIN_NEGATIVE));
  EXPECT_CHECK_DEATH(TestEnumSet().FromRange(TestEnum::TEST_3,
                                             TestEnum::TEST_6_OUT_OF_BOUNDS));

  // Crashes when start and end are both out of range.
  EXPECT_CHECK_DEATH(TestEnumSet().FromRange(TestEnum::TEST_7_OUT_OF_BOUNDS,
                                             TestEnum::TEST_6_OUT_OF_BOUNDS));
  EXPECT_CHECK_DEATH(TestEnumSet().FromRange(TestEnum::TEST_6_OUT_OF_BOUNDS,
                                             TestEnum::TEST_7_OUT_OF_BOUNDS));
}

TEST_F(EnumSetDeathTest, PutCrashesOnOutOfRange) {
  EXPECT_CHECK_DEATH(TestEnumSet().Put(TestEnum::TEST_BELOW_MIN));
  EXPECT_CHECK_DEATH(TestEnumSet().Put(TestEnum::TEST_BELOW_MIN_NEGATIVE));
  EXPECT_CHECK_DEATH(TestEnumSet().Put(TestEnum::TEST_6_OUT_OF_BOUNDS));
  EXPECT_CHECK_DEATH(TestEnumSet().Put(TestEnum::TEST_7_OUT_OF_BOUNDS));
}

TEST_F(EnumSetDeathTest, PutRangeCrashesOnBadInputs) {
  // Crashes when one input is out of range.
  EXPECT_CHECK_DEATH(TestEnumSet().PutRange(TestEnum::TEST_BELOW_MIN_NEGATIVE,
                                            TestEnum::TEST_BELOW_MIN));
  EXPECT_CHECK_DEATH(
      TestEnumSet().PutRange(TestEnum::TEST_3, TestEnum::TEST_7_OUT_OF_BOUNDS));

  // Crashes when both inputs are out of range.
  EXPECT_CHECK_DEATH(TestEnumSet().PutRange(TestEnum::TEST_6_OUT_OF_BOUNDS,
                                            TestEnum::TEST_7_OUT_OF_BOUNDS));

  // Crashes when inputs are out of order.
  EXPECT_CHECK_DEATH(
      TestEnumSet().PutRange(TestEnum::TEST_2, TestEnum::TEST_1));
}

TEST_F(EnumSetTest, ToStringEmpty) {
  const TestEnumSet enums;
  EXPECT_THAT(enums.ToString(), testing::Eq("00000"));
}

TEST_F(EnumSetTest, ToString) {
  const TestEnumSet enums = {TestEnum::TEST_4};
  EXPECT_THAT(enums.ToString(), testing::Eq("01000"));
}

TEST_F(EnumSetTest, ToVectorEmpty) {
  const TestEnumSet enums;
  EXPECT_TRUE(ToVector(enums).empty());
}

TEST_F(EnumSetTest, ToVector) {
  const TestEnumSet enums = {TestEnum::TEST_2, TestEnum::TEST_4};
  EXPECT_THAT(ToVector(enums),
              testing::ElementsAre(TestEnum::TEST_2, TestEnum::TEST_4));
}

}  // namespace
}  // namespace base
