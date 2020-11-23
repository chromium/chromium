// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_set.h"

#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(FixedFlatSetTest, MakeFixedFlatSet_SortedInput) {
  constexpr auto kSet = MakeFixedFlatSet<int>({1, 2, 3, 4});
  static_assert(ranges::is_sorted(kSet), "Error: Set is not sorted.");
  static_assert(ranges::adjacent_find(kSet) == kSet.end(),
                "Error: Set contains repeated elements.");
  EXPECT_THAT(kSet, ::testing::ElementsAre(1, 2, 3, 4));
}

TEST(FixedFlatSetTest, MakeFixedFlatSet_UnsortedInput) {
  constexpr auto kSet = MakeFixedFlatSet<StringPiece>({"foo", "bar", "baz"});
  static_assert(ranges::is_sorted(kSet), "Error: Set is not sorted.");
  static_assert(ranges::adjacent_find(kSet) == kSet.end(),
                "Error: Set contains repeated elements.");
  EXPECT_THAT(kSet, ::testing::ElementsAre("bar", "baz", "foo"));
}

// Verifies that passing repeated keys to MakeFixedFlatSet results in a CHECK
// failure.
TEST(FixedFlatSetTest, RepeatedKeys) {
  EXPECT_CHECK_DEATH(MakeFixedFlatSet<int>({1, 2, 3, 1}));
}

}  // namespace base
