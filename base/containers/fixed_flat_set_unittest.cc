// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_set.h"

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <functional>
#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace base {

TEST(FixedFlatSetTest, MakeFixedFlatSet_SortedInput) {
  constexpr auto kSet = MakeFixedFlatSet<int>({1, 2, 3, 4});
  static_assert(std::ranges::is_sorted(kSet), "Error: Set is not sorted.");
  static_assert(std::ranges::adjacent_find(kSet) == kSet.end(),
                "Error: Set contains repeated elements.");
  EXPECT_THAT(kSet, ::testing::ElementsAre(1, 2, 3, 4));
}

// The container deduction guides for `flat_set` must not break CTAD for
// `fixed_flat_set`, whose `std::array` container has a non-type parameter.
TEST(FixedFlatSetTest, CopyDeduction) {
  constexpr auto kSet = MakeFixedFlatSet<int>({1, 2, 3});
  fixed_flat_set copy(kSet);
  static_assert(std::same_as<decltype(copy), fixed_flat_set<int, 3>>);
  EXPECT_EQ(copy, kSet);
}

TEST(FixedFlatSetTest, MakeFixedFlatSet_UnsortedInput) {
  constexpr auto kSet =
      MakeFixedFlatSet<std::string_view>({"foo", "bar", "baz"});
  static_assert(std::ranges::is_sorted(kSet), "Error: Set not sorted.");
  static_assert(std::ranges::adjacent_find(kSet) == kSet.end(),
                "Error: Set contains repeated elements.");
  EXPECT_THAT(kSet, ::testing::ElementsAre("bar", "baz", "foo"));
}

TEST(FixedFlatSetTest, MakeFixedFlatSet_Contains) {
  constexpr auto kSet =
      MakeFixedFlatSet<std::string_view>({"foo", "bar", "baz"});
  static_assert(kSet.contains("foo"), "Error: Set must contain \"foo\".");
  static_assert(!kSet.contains("fooz"),
                "Error: Set must not contain \"fooz\".");
}

TEST(FixedFlatSetTest, MakeFixedFlatSet_Find) {
  constexpr auto kSet =
      MakeFixedFlatSet<std::string_view>({"foo", "bar", "baz"});
  static_assert(kSet.find("foo") != kSet.end(),
                "Error: Set must contain \"foo\".");
  static_assert(kSet.find("fooz") == kSet.end(),
                "Error: Set must not contain \"fooz\".");
}

TEST(FixedFlatSetTest, MakeFixedFlatSet_LowerBound) {
  constexpr auto kSet = MakeFixedFlatSet<int>({1, 3, 5, 7});

  static_assert(kSet.lower_bound(0) == kSet.begin() + 0);
  static_assert(kSet.lower_bound(1) == kSet.begin() + 0);
  static_assert(kSet.lower_bound(2) == kSet.begin() + 1);
  static_assert(kSet.lower_bound(3) == kSet.begin() + 1);
  static_assert(kSet.lower_bound(4) == kSet.begin() + 2);
  static_assert(kSet.lower_bound(5) == kSet.begin() + 2);
  static_assert(kSet.lower_bound(6) == kSet.begin() + 3);
  static_assert(kSet.lower_bound(7) == kSet.begin() + 3);
  static_assert(kSet.lower_bound(8) == kSet.begin() + 4);
}

TEST(FixedFlatSetTest, MakeFixedFlatSet_UpperBound) {
  constexpr auto kSet = MakeFixedFlatSet<int>({1, 3, 5, 7});
  static_assert(kSet.upper_bound(0) == kSet.begin() + 0);
  static_assert(kSet.upper_bound(1) == kSet.begin() + 1);
  static_assert(kSet.upper_bound(2) == kSet.begin() + 1);
  static_assert(kSet.upper_bound(3) == kSet.begin() + 2);
  static_assert(kSet.upper_bound(4) == kSet.begin() + 2);
  static_assert(kSet.upper_bound(5) == kSet.begin() + 3);
  static_assert(kSet.upper_bound(6) == kSet.begin() + 3);
  static_assert(kSet.upper_bound(7) == kSet.begin() + 4);
  static_assert(kSet.upper_bound(8) == kSet.begin() + 4);
}

TEST(FixedFlatSetTest, MakeFixedFlatSet_EqualRange) {
  constexpr auto kSet = MakeFixedFlatSet<int>({1, 3, 5, 7});

  static_assert(kSet.equal_range(3).first == kSet.begin() + 1);
  static_assert(kSet.equal_range(3).second == kSet.begin() + 2);
  static_assert(kSet.equal_range(4).first == kSet.begin() + 2);
  static_assert(kSet.equal_range(4).second == kSet.begin() + 2);
}

TEST(FixedFlatSetTest, MakeFixedFlatSet_StdArray) {
  constexpr std::array<int, 3> kUnsorted = {3, 1, 2};
  constexpr auto kSet = MakeFixedFlatSet<int>(kUnsorted);
  EXPECT_THAT(kSet, ElementsAre(1, 2, 3));

  // Pre-sorted rvalue with explicit comparator template arg.
  constexpr auto kDesc =
      MakeFixedFlatSet<int, std::greater<>>(std::array{3, 2, 1});
  EXPECT_THAT(kDesc, ElementsAre(3, 2, 1));

  // Projection with pointer-to-data-member and comparator value arg.
  struct Row {
    int id;
    std::string_view name;
  };
  constexpr auto kRows = std::to_array<Row>({
      {2, "two"},
      {1, "one"},
      {3, "three"},
  });
  constexpr auto kIds =
      MakeFixedFlatSet<int>(kRows, std::greater<>(), &Row::id);
  EXPECT_THAT(kIds, ElementsAre(3, 2, 1));

  // Implicit conversion without a projection.
  constexpr std::array<const char*, 3> kRawStrings = {"foo", "bar", "baz"};
  constexpr auto kStrings = MakeFixedFlatSet<std::string_view>(kRawStrings);
  EXPECT_THAT(kStrings, ElementsAre("bar", "baz", "foo"));

  // Non-default-constructible key type.
  struct NoDefault {
    constexpr explicit NoDefault(int v) : value(v) {}
    auto operator<=>(const NoDefault&) const = default;
    int value;
  };
  static_assert(!std::default_initializable<NoDefault>);
  constexpr std::array<NoDefault, 3> kNoDefault = {
      NoDefault(3),
      NoDefault(1),
      NoDefault(2),
  };
  constexpr auto kNoDefaultSet = MakeFixedFlatSet<NoDefault>(kNoDefault);
  EXPECT_THAT(kNoDefaultSet,
              ElementsAre(NoDefault(1), NoDefault(2), NoDefault(3)));

  // Empty std::array.
  constexpr auto kEmpty = MakeFixedFlatSet<int>(std::array<int, 0>{});
  static_assert(kEmpty.empty());
}

TEST(FixedFlatSetTest, MakeFixedFlatSet_FromFixedFlatMap) {
  constexpr auto kMap = MakeFixedFlatMap<std::string_view, int>(
      {{"foo", 1}, {"bar", 2}, {"baz", 3}});

  // Extracting keys preserves sorted order.
  constexpr auto kKeys = MakeFixedFlatSet<std::string_view>(
      kMap, /*comp=*/{}, [](const auto& entry) { return entry.first; });
  EXPECT_THAT(kKeys, ElementsAre("bar", "baz", "foo"));

  // Extracting values re-sorts them.
  constexpr auto kValues = MakeFixedFlatSet<int>(
      kMap, /*comp=*/{}, [](const auto& entry) { return entry.second; });
  EXPECT_THAT(kValues, ElementsAre(1, 2, 3));
}

TEST(FixedFlatSetTest, MakeFixedFlatSet_FromFixedFlatSet) {
  constexpr auto kSet = MakeFixedFlatSet<int>({1, 2, 3});
  constexpr auto kNegated =
      MakeFixedFlatSet<int>(kSet, /*comp=*/{}, [](int x) { return -x; });
  EXPECT_THAT(kNegated, ElementsAre(-3, -2, -1));
}

}  // namespace base
