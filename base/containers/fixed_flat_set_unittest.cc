// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_set.h"

#include <algorithm>
#include <string_view>

#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(FixedFlatSetTest, MakeFixedFlatSet_SortedInput) {
  constexpr auto kSet = MakeFixedFlatSet<int>({1, 2, 3, 4});
  static_assert(std::ranges::is_sorted(kSet), "Error: Set is not sorted.");
  static_assert(std::ranges::adjacent_find(kSet) == kSet.end(),
                "Error: Set contains repeated elements.");
  EXPECT_THAT(kSet, ::testing::ElementsAre(1, 2, 3, 4));
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

}  // namespace base
