// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/enum_set.h"

#include <stddef.h>

#include <optional>

#include "base/containers/to_vector.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-death-test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

enum class TestEnum {
  kTestBelowMinNegative = -1,
  kTestBelowMin = 0,
  kTest1 = 1,
  kTestMin = kTest1,
  kTest2,
  kTest3,
  kTest4,
  kTest5,
  kTestMax = kTest5,
  kTest6OutOfBounds,
  kTest7OutOfBounds
};
using TestEnumSet = EnumSet<TestEnum, TestEnum::kTestMin, TestEnum::kTestMax>;

enum class TestEnumExtreme {
  kTest0 = 0,
  kTestMin = kTest0,
  kTest63 = 63,
  kTestMax = kTest63,
  kTest64OutOfBounds,
};
using TestEnumExtremeSet = EnumSet<TestEnumExtreme,
                                   TestEnumExtreme::kTestMin,
                                   TestEnumExtreme::kTestMax>;

class EnumSetTest : public ::testing::Test {};
class EnumSetDeathTest : public ::testing::Test {};

TEST_F(EnumSetTest, ClassConstants) {
  EXPECT_EQ(TestEnum::kTestMin, TestEnumSet::kMinValue);
  EXPECT_EQ(TestEnum::kTestMax, TestEnumSet::kMaxValue);
  EXPECT_EQ(5u, TestEnumSet::kValueCount);
}

// Use static_assert to check that functions we expect to be compile time
// evaluatable are really that way.
TEST_F(EnumSetTest, ConstexprsAreValid) {
  static_assert(TestEnumSet::All().Has(TestEnum::kTest2),
                "Expected All() to be integral constant expression");
  static_assert(TestEnumSet::FromRange(TestEnum::kTest2, TestEnum::kTest4)
                    .Has(TestEnum::kTest2),
                "Expected FromRange() to be integral constant expression");
  static_assert(TestEnumSet{TestEnum::kTest2}.Has(TestEnum::kTest2),
                "Expected TestEnumSet() to be integral constant expression");
  static_assert(
      TestEnumSet::FromEnumBitmask(1 << static_cast<uint64_t>(TestEnum::kTest2))
          .Has(TestEnum::kTest2),
      "Expected TestEnumSet() to be integral constant expression");
}

TEST_F(EnumSetTest, DefaultConstructor) {
  const TestEnumSet enums;
  EXPECT_TRUE(enums.empty());
  EXPECT_EQ(0u, enums.size());
  EXPECT_FALSE(enums.Has(TestEnum::kTest1));
  EXPECT_FALSE(enums.Has(TestEnum::kTest2));
  EXPECT_FALSE(enums.Has(TestEnum::kTest3));
  EXPECT_FALSE(enums.Has(TestEnum::kTest4));
  EXPECT_FALSE(enums.Has(TestEnum::kTest5));
}

TEST_F(EnumSetTest, OneArgConstructor) {
  const TestEnumSet enums = {TestEnum::kTest4};
  EXPECT_FALSE(enums.empty());
  EXPECT_EQ(1u, enums.size());
  EXPECT_FALSE(enums.Has(TestEnum::kTest1));
  EXPECT_FALSE(enums.Has(TestEnum::kTest2));
  EXPECT_FALSE(enums.Has(TestEnum::kTest3));
  EXPECT_TRUE(enums.Has(TestEnum::kTest4));
  EXPECT_FALSE(enums.Has(TestEnum::kTest5));
}

TEST_F(EnumSetTest, OneArgConstructorSize) {
  TestEnumExtremeSet enums = {TestEnumExtreme::kTest0};
  EXPECT_TRUE(enums.Has(TestEnumExtreme::kTest0));
}

TEST_F(EnumSetTest, TwoArgConstructor) {
  const TestEnumSet enums = {TestEnum::kTest4, TestEnum::kTest2};
  EXPECT_FALSE(enums.empty());
  EXPECT_EQ(2u, enums.size());
  EXPECT_FALSE(enums.Has(TestEnum::kTest1));
  EXPECT_TRUE(enums.Has(TestEnum::kTest2));
  EXPECT_FALSE(enums.Has(TestEnum::kTest3));
  EXPECT_TRUE(enums.Has(TestEnum::kTest4));
  EXPECT_FALSE(enums.Has(TestEnum::kTest5));
}

TEST_F(EnumSetTest, ThreeArgConstructor) {
  const TestEnumSet enums = {TestEnum::kTest4, TestEnum::kTest2,
                             TestEnum::kTest1};
  EXPECT_FALSE(enums.empty());
  EXPECT_EQ(3u, enums.size());
  EXPECT_TRUE(enums.Has(TestEnum::kTest1));
  EXPECT_TRUE(enums.Has(TestEnum::kTest2));
  EXPECT_FALSE(enums.Has(TestEnum::kTest3));
  EXPECT_TRUE(enums.Has(TestEnum::kTest4));
  EXPECT_FALSE(enums.Has(TestEnum::kTest5));
}

TEST_F(EnumSetTest, DuplicatesInConstructor) {
  EXPECT_EQ(
      TestEnumSet({TestEnum::kTest4, TestEnum::kTest2, TestEnum::kTest1,
                   TestEnum::kTest4, TestEnum::kTest2, TestEnum::kTest4}),
      TestEnumSet({TestEnum::kTest1, TestEnum::kTest2, TestEnum::kTest4}));
}

TEST_F(EnumSetTest, All) {
  const TestEnumSet enums(TestEnumSet::All());
  EXPECT_FALSE(enums.empty());
  EXPECT_EQ(5u, enums.size());
  EXPECT_TRUE(enums.Has(TestEnum::kTest1));
  EXPECT_TRUE(enums.Has(TestEnum::kTest2));
  EXPECT_TRUE(enums.Has(TestEnum::kTest3));
  EXPECT_TRUE(enums.Has(TestEnum::kTest4));
  EXPECT_TRUE(enums.Has(TestEnum::kTest5));
}

TEST_F(EnumSetTest, AllExtreme) {
  const TestEnumExtremeSet enums(TestEnumExtremeSet::All());
  EXPECT_FALSE(enums.empty());
  EXPECT_EQ(64u, enums.size());
  EXPECT_TRUE(enums.Has(TestEnumExtreme::kTest0));
  EXPECT_TRUE(enums.Has(TestEnumExtreme::kTest63));
  EXPECT_FALSE(enums.Has(TestEnumExtreme::kTest64OutOfBounds));
}

TEST_F(EnumSetTest, FromRange) {
  EXPECT_EQ(TestEnumSet({TestEnum::kTest2, TestEnum::kTest3, TestEnum::kTest4}),
            TestEnumSet::FromRange(TestEnum::kTest2, TestEnum::kTest4));
  EXPECT_EQ(TestEnumSet::All(),
            TestEnumSet::FromRange(TestEnum::kTest1, TestEnum::kTest5));
  EXPECT_EQ(TestEnumSet({TestEnum::kTest2}),
            TestEnumSet::FromRange(TestEnum::kTest2, TestEnum::kTest2));

  using RestrictedRangeSet =
      EnumSet<TestEnum, TestEnum::kTest2, TestEnum::kTestMax>;
  EXPECT_EQ(RestrictedRangeSet(
                {TestEnum::kTest2, TestEnum::kTest3, TestEnum::kTest4}),
            RestrictedRangeSet::FromRange(TestEnum::kTest2, TestEnum::kTest4));
  EXPECT_EQ(RestrictedRangeSet::All(),
            RestrictedRangeSet::FromRange(TestEnum::kTest2, TestEnum::kTest5));
}

TEST_F(EnumSetTest, Put) {
  TestEnumSet enums = {TestEnum::kTest4};
  enums.Put(TestEnum::kTest3);
  EXPECT_EQ(TestEnumSet({TestEnum::kTest3, TestEnum::kTest4}), enums);
  enums.Put(TestEnum::kTest5);
  EXPECT_EQ(TestEnumSet({TestEnum::kTest3, TestEnum::kTest4, TestEnum::kTest5}),
            enums);
}

TEST_F(EnumSetTest, PutAll) {
  TestEnumSet enums = {TestEnum::kTest4, TestEnum::kTest5};
  enums.PutAll({TestEnum::kTest3, TestEnum::kTest4});
  EXPECT_EQ(TestEnumSet({TestEnum::kTest3, TestEnum::kTest4, TestEnum::kTest5}),
            enums);
}

TEST_F(EnumSetTest, PutRange) {
  TestEnumSet enums;
  enums.PutRange(TestEnum::kTest2, TestEnum::kTest4);
  EXPECT_EQ(TestEnumSet({TestEnum::kTest2, TestEnum::kTest3, TestEnum::kTest4}),
            enums);
}

TEST_F(EnumSetTest, RetainAll) {
  TestEnumSet enums = {TestEnum::kTest4, TestEnum::kTest5};
  enums.RetainAll(TestEnumSet({TestEnum::kTest3, TestEnum::kTest4}));
  EXPECT_EQ(TestEnumSet({TestEnum::kTest4}), enums);
}

TEST_F(EnumSetTest, Remove) {
  TestEnumSet enums = {TestEnum::kTest4, TestEnum::kTest5};
  enums.Remove(TestEnum::kTest1);
  enums.Remove(TestEnum::kTest3);
  EXPECT_EQ(TestEnumSet({TestEnum::kTest4, TestEnum::kTest5}), enums);
  enums.Remove(TestEnum::kTest4);
  EXPECT_EQ(TestEnumSet({TestEnum::kTest5}), enums);
  enums.Remove(TestEnum::kTest5);
  enums.Remove(TestEnum::kTest6OutOfBounds);
  EXPECT_TRUE(enums.empty());
}

TEST_F(EnumSetTest, RemoveAll) {
  TestEnumSet enums = {TestEnum::kTest4, TestEnum::kTest5};
  enums.RemoveAll(TestEnumSet({TestEnum::kTest3, TestEnum::kTest4}));
  EXPECT_EQ(TestEnumSet({TestEnum::kTest5}), enums);
}

TEST_F(EnumSetTest, Clear) {
  TestEnumSet enums = {TestEnum::kTest4, TestEnum::kTest5};
  enums.Clear();
  EXPECT_TRUE(enums.empty());
}

TEST_F(EnumSetTest, Set) {
  TestEnumSet enums;
  EXPECT_TRUE(enums.empty());

  enums.PutOrRemove(TestEnum::kTest3, false);
  EXPECT_TRUE(enums.empty());

  enums.PutOrRemove(TestEnum::kTest4, true);
  EXPECT_EQ(enums, TestEnumSet({TestEnum::kTest4}));

  enums.PutOrRemove(TestEnum::kTest5, true);
  EXPECT_EQ(enums, TestEnumSet({TestEnum::kTest4, TestEnum::kTest5}));
  enums.PutOrRemove(TestEnum::kTest5, true);
  EXPECT_EQ(enums, TestEnumSet({TestEnum::kTest4, TestEnum::kTest5}));

  enums.PutOrRemove(TestEnum::kTest4, false);
  EXPECT_EQ(enums, TestEnumSet({TestEnum::kTest5}));
}

TEST_F(EnumSetTest, Has) {
  const TestEnumSet enums = {TestEnum::kTest4, TestEnum::kTest5};
  EXPECT_FALSE(enums.Has(TestEnum::kTest1));
  EXPECT_FALSE(enums.Has(TestEnum::kTest2));
  EXPECT_FALSE(enums.Has(TestEnum::kTest3));
  EXPECT_TRUE(enums.Has(TestEnum::kTest4));
  EXPECT_TRUE(enums.Has(TestEnum::kTest5));
  EXPECT_FALSE(enums.Has(TestEnum::kTest6OutOfBounds));
}

TEST_F(EnumSetTest, HasAll) {
  const TestEnumSet enums1 = {TestEnum::kTest4, TestEnum::kTest5};
  const TestEnumSet enums2 = {TestEnum::kTest3, TestEnum::kTest4};
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
  const TestEnumSet enums1 = {TestEnum::kTest4, TestEnum::kTest5};
  const TestEnumSet enums2 = {TestEnum::kTest3, TestEnum::kTest4};
  const TestEnumSet enums3 = {TestEnum::kTest1, TestEnum::kTest2};
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
  const TestEnumSet enums1 = {TestEnum::kTest4, TestEnum::kTest5};
  TestEnumSet enums2;
  for (TestEnum e : enums1) {
    enums2.Put(e);
  }
  EXPECT_EQ(enums2, enums1);
}

TEST_F(EnumSetTest, RangeBasedForLoop) {
  const TestEnumSet enums1 = {TestEnum::kTest2, TestEnum::kTest5};
  TestEnumSet enums2;
  for (TestEnum e : enums1) {
    enums2.Put(e);
  }
  EXPECT_EQ(enums2, enums1);
}

TEST_F(EnumSetTest, IteratorComparisonOperators) {
  const TestEnumSet enums = {TestEnum::kTest2, TestEnum::kTest4};
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
  const TestEnumSet enums = {TestEnum::kTest2, TestEnum::kTest4};
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
  const TestEnumSet enums1 = {TestEnum::kTest4, TestEnum::kTest5};
  const TestEnumSet enums2 = {TestEnum::kTest3, TestEnum::kTest4};
  const TestEnumSet enums3 = Union(enums1, enums2);

  EXPECT_EQ(TestEnumSet({TestEnum::kTest3, TestEnum::kTest4, TestEnum::kTest5}),
            enums3);
}

TEST_F(EnumSetTest, Intersection) {
  const TestEnumSet enums1 = {TestEnum::kTest4, TestEnum::kTest5};
  const TestEnumSet enums2 = {TestEnum::kTest3, TestEnum::kTest4};
  const TestEnumSet enums3 = Intersection(enums1, enums2);

  EXPECT_EQ(TestEnumSet({TestEnum::kTest4}), enums3);
}

TEST_F(EnumSetTest, Difference) {
  const TestEnumSet enums1 = {TestEnum::kTest4, TestEnum::kTest5};
  const TestEnumSet enums2 = {TestEnum::kTest3, TestEnum::kTest4};
  const TestEnumSet enums3 = Difference(enums1, enums2);

  EXPECT_EQ(TestEnumSet({TestEnum::kTest5}), enums3);
}

TEST_F(EnumSetTest, ToFromEnumBitmask) {
  const TestEnumSet empty;
  EXPECT_EQ(empty.ToEnumBitmask(), 0ULL);
  EXPECT_EQ(TestEnumSet::FromEnumBitmask(0), empty);

  const TestEnumSet enums1 = {TestEnum::kTest2};
  const uint64_t val1 = 1ULL << static_cast<uint64_t>(TestEnum::kTest2);
  EXPECT_EQ(enums1.ToEnumBitmask(), val1);
  EXPECT_EQ(TestEnumSet::FromEnumBitmask(val1), enums1);

  const TestEnumSet enums2 = {TestEnum::kTest3, TestEnum::kTest4};
  const uint64_t val2 = 1ULL << static_cast<uint64_t>(TestEnum::kTest3) |
                        1ULL << static_cast<uint64_t>(TestEnum::kTest4);
  EXPECT_EQ(enums2.ToEnumBitmask(), val2);
  EXPECT_EQ(TestEnumSet::FromEnumBitmask(val2), enums2);
}

TEST_F(EnumSetTest, ToFromEnumBitmaskExtreme) {
  const TestEnumExtremeSet empty;
  EXPECT_EQ(empty.ToEnumBitmask(), 0ULL);
  EXPECT_EQ(TestEnumExtremeSet::FromEnumBitmask(0ULL), empty);

  const TestEnumExtremeSet enums1 = {TestEnumExtreme::kTest63};
  const uint64_t val1 = 1ULL << static_cast<uint64_t>(TestEnumExtreme::kTest63);
  EXPECT_EQ(enums1.ToEnumBitmask(), val1);
  EXPECT_EQ(TestEnumExtremeSet::FromEnumBitmask(val1), enums1);
}

TEST_F(EnumSetTest, FromEnumBitmaskIgnoresExtraBits) {
  const TestEnumSet kSets[] = {
      {},
      {TestEnum::kTestMin},
      {TestEnum::kTestMax},
      {TestEnum::kTestMin, TestEnum::kTestMax},
      {TestEnum::kTestMin, TestEnum::kTestMax},
      {TestEnum::kTest2, TestEnum::kTest4},
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
        // Add junk bits above kTestMax.
        val | single_val_bitstring(TestEnum::kTest6OutOfBounds),
        val | single_val_bitstring(TestEnum::kTest7OutOfBounds),
        val | single_val_bitstring(TestEnum::kTest6OutOfBounds) |
            single_val_bitstring(TestEnum::kTest7OutOfBounds),
        // Add junk bits below kTestMin.
        val | single_val_bitstring(TestEnum::kTestBelowMin),
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
    kTest1 = 1,
    kTestMin = 1,
    kTest50 = 50,
    kTest100 = 100,
    kTestMax = kTest100,
  };
  using TestEnumSparseSet = EnumSet<TestEnumSparse, TestEnumSparse::kTestMin,
                                    TestEnumSparse::kTestMax>;
  TestEnumSparseSet sparse;
  sparse.Put(TestEnumSparse::kTestMin);
  sparse.Put(TestEnumSparse::kTestMax);
  EXPECT_EQ(sparse.size(), 2u);

  EXPECT_EQ(TestEnumSparseSet::All().size(), 100u);
}

TEST_F(EnumSetTest, GetNth64bitWordBitmaskFromEnum) {
  enum class TestEnumEdgeCase {
    kTest1 = 1,
    kTestMin = kTest1,
    kTest63 = 63,
    kTest64 = 64,
    kTest100 = 100,
    kTestMax = kTest100,
  };
  using TestEnumEdgeCaseSet =
      EnumSet<TestEnumEdgeCase, TestEnumEdgeCase::kTestMin,
              TestEnumEdgeCase::kTestMax>;
  TestEnumEdgeCaseSet sparse;
  sparse.Put(TestEnumEdgeCase::kTest1);
  sparse.Put(TestEnumEdgeCase::kTest63);
  sparse.Put(TestEnumEdgeCase::kTest64);
  sparse.Put(TestEnumEdgeCase::kTest100);
  std::optional<uint64_t> bit_mask_0 = sparse.GetNth64bitWordBitmask(0);
  ASSERT_TRUE(bit_mask_0.has_value());
  ASSERT_EQ(bit_mask_0.value(),
            1ull << static_cast<uint32_t>(TestEnumEdgeCase::kTest1) |
                1ull << static_cast<uint32_t>(TestEnumEdgeCase::kTest63));
  std::optional<uint64_t> bit_mask_1 = sparse.GetNth64bitWordBitmask(1);
  ASSERT_TRUE(bit_mask_1.has_value());
  ASSERT_EQ(
      bit_mask_1.value(),
      1ull << (static_cast<uint32_t>(TestEnumEdgeCase::kTest64) - 64u) |
          1ull << (static_cast<uint32_t>(TestEnumEdgeCase::kTest100) - 64u));
  std::optional<uint64_t> bit_mask_2 = sparse.GetNth64bitWordBitmask(2);
  ASSERT_FALSE(bit_mask_2.has_value());
}

TEST_F(EnumSetTest, SparseEnumSmall) {
  enum class TestEnumSparse {
    kTest1 = 1,
    kTestMin = 1,
    kTest50 = 50,
    kTest60 = 60,
    kTestMax = kTest60,
  };
  using TestEnumSparseSet = EnumSet<TestEnumSparse, TestEnumSparse::kTestMin,
                                    TestEnumSparse::kTestMax>;
  TestEnumSparseSet sparse;
  sparse.Put(TestEnumSparse::kTestMin);
  sparse.Put(TestEnumSparse::kTestMax);
  EXPECT_EQ(sparse.size(), 2u);

  // This may seem a little surprising! There are only 3 distinct values in
  // TestEnumSparse, so why does TestEnumSparseSet think it has 60 of them? This
  // is an artifact of EnumSet's design, as it has no way of knowing which
  // values between the min and max are actually named in the enum's definition.
  EXPECT_EQ(TestEnumSparseSet::All().size(), 60u);
}

TEST_F(EnumSetDeathTest, CrashesOnOutOfRange) {
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::kTestBelowMin}));
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::kTest6OutOfBounds}));
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::kTest7OutOfBounds}));
}

TEST_F(EnumSetDeathTest, EnumWithNegatives) {
  enum class TestEnumNeg {
    kTestBelowMin = -3,
    kTestA = -2,
    kTestMin = kTestA,
    kTestB = -1,
    kTestC = 0,
    kTestD = 1,
    kTestE = 2,
    kTestMax = kTestE,
    kTestF = 3,
  };
  // This EnumSet starts negative and ends positive.
  using TestEnumWithNegSet =
      EnumSet<TestEnumNeg, TestEnumNeg::kTestMin, TestEnumNeg::kTestMax>;

  // Should crash because kTestBelowMin is not in range.
  EXPECT_CHECK_DEATH(TestEnumWithNegSet({TestEnumNeg::kTestBelowMin}));
  // kTestD is in range, but note that kTestMin is negative. This should work.
  EXPECT_TRUE(
      TestEnumWithNegSet({TestEnumNeg::kTestD}).Has(TestEnumNeg::kTestD));
  // Even though kTestA is negative, it is in range, so this should work.
  EXPECT_TRUE(
      TestEnumWithNegSet({TestEnumNeg::kTestA}).Has(TestEnumNeg::kTestA));
}

TEST_F(EnumSetDeathTest, EnumWithOnlyNegatives) {
  enum class TestEnumNeg {
    kTestBelowMin = -10,
    kTestA = -9,
    kTestMin = kTestA,
    kTestB = -8,
    kTestC = -7,
    kTestD = -6,
    kTestMax = kTestD,
    kTestF = -5,
  };
  // This EnumSet starts negative and ends negative.
  using TestEnumWithNegSet =
      EnumSet<TestEnumNeg, TestEnumNeg::kTestMin, TestEnumNeg::kTestMax>;

  // Should crash because kTestBelowMin is not in range.
  EXPECT_CHECK_DEATH(TestEnumWithNegSet({TestEnumNeg::kTestBelowMin}));
  // kTestA, kTestD are in range, but note that kTestMin and values are
  // negative. This should work.
  EXPECT_TRUE(
      TestEnumWithNegSet({TestEnumNeg::kTestA}).Has(TestEnumNeg::kTestA));
  EXPECT_TRUE(
      TestEnumWithNegSet({TestEnumNeg::kTestD}).Has(TestEnumNeg::kTestD));
}

TEST_F(EnumSetDeathTest, VariadicConstructorCrashesOnOutOfRange) {
  // Constructor should crash given out-of-range values.
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::kTestBelowMin}).empty());
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::kTestBelowMinNegative}).empty());
  EXPECT_CHECK_DEATH(TestEnumSet({TestEnum::kTest6OutOfBounds}).empty());
}

TEST_F(EnumSetDeathTest, FromRangeCrashesOnBadInputs) {
  // FromRange crashes when the bounds are in range, but out of order.
  EXPECT_CHECK_DEATH(
      TestEnumSet().FromRange(TestEnum::kTest3, TestEnum::kTest1));

  // FromRange crashes when the start value is out of range.
  EXPECT_CHECK_DEATH(
      TestEnumSet().FromRange(TestEnum::kTestBelowMin, TestEnum::kTest1));
  EXPECT_CHECK_DEATH(TestEnumSet().FromRange(TestEnum::kTestBelowMinNegative,
                                             TestEnum::kTest1));
  EXPECT_CHECK_DEATH(
      TestEnumSet().FromRange(TestEnum::kTest6OutOfBounds, TestEnum::kTest1));

  // FromRange crashes when the end value is out of range.
  EXPECT_CHECK_DEATH(
      TestEnumSet().FromRange(TestEnum::kTest3, TestEnum::kTestBelowMin));
  EXPECT_CHECK_DEATH(TestEnumSet().FromRange(TestEnum::kTest3,
                                             TestEnum::kTestBelowMinNegative));
  EXPECT_CHECK_DEATH(
      TestEnumSet().FromRange(TestEnum::kTest3, TestEnum::kTest6OutOfBounds));

  // Crashes when start and end are both out of range.
  EXPECT_CHECK_DEATH(TestEnumSet().FromRange(TestEnum::kTest6OutOfBounds,
                                             TestEnum::kTest7OutOfBounds));
  EXPECT_CHECK_DEATH(TestEnumSet().FromRange(TestEnum::kTest6OutOfBounds,
                                             TestEnum::kTest7OutOfBounds));
}

TEST_F(EnumSetDeathTest, PutCrashesOnOutOfRange) {
  EXPECT_CHECK_DEATH(TestEnumSet().Put(TestEnum::kTestBelowMin));
  EXPECT_CHECK_DEATH(TestEnumSet().Put(TestEnum::kTestBelowMinNegative));
  EXPECT_CHECK_DEATH(TestEnumSet().Put(TestEnum::kTest6OutOfBounds));
  EXPECT_CHECK_DEATH(TestEnumSet().Put(TestEnum::kTest7OutOfBounds));
}

TEST_F(EnumSetDeathTest, PutRangeCrashesOnBadInputs) {
  // Crashes when one input is out of range.
  EXPECT_CHECK_DEATH(TestEnumSet().PutRange(TestEnum::kTestBelowMinNegative,
                                            TestEnum::kTestBelowMin));
  EXPECT_CHECK_DEATH(
      TestEnumSet().PutRange(TestEnum::kTest3, TestEnum::kTest7OutOfBounds));

  // Crashes when both inputs are out of range.
  EXPECT_CHECK_DEATH(TestEnumSet().PutRange(TestEnum::kTest6OutOfBounds,
                                            TestEnum::kTest7OutOfBounds));

  // Crashes when inputs are out of order.
  EXPECT_CHECK_DEATH(
      TestEnumSet().PutRange(TestEnum::kTest2, TestEnum::kTest1));
}

TEST_F(EnumSetTest, ToStringEmpty) {
  const TestEnumSet enums;
  EXPECT_THAT(enums.ToString(), testing::Eq("00000"));
}

TEST_F(EnumSetTest, ToString) {
  const TestEnumSet enums = {TestEnum::kTest4};
  EXPECT_THAT(enums.ToString(), testing::Eq("01000"));
}

TEST_F(EnumSetTest, ToVectorEmpty) {
  const TestEnumSet enums;
  EXPECT_TRUE(ToVector(enums).empty());
}

TEST_F(EnumSetTest, ToVector) {
  const TestEnumSet enums = {TestEnum::kTest2, TestEnum::kTest4};
  EXPECT_THAT(ToVector(enums),
              testing::ElementsAre(TestEnum::kTest2, TestEnum::kTest4));
}

}  // namespace
}  // namespace base
