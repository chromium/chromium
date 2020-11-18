// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_map.h"

#include <string>

#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Pair;

namespace base {

TEST(FixedFlatMapTest, MakeFixedFlatMap_SortedInput) {
  constexpr auto kSquares =
      MakeFixedFlatMap<int, int>({{1, 1}, {2, 4}, {3, 9}, {4, 16}});
  static_assert(ranges::is_sorted(kSquares), "Error: Map is not sorted.");
  static_assert(ranges::adjacent_find(kSquares) == kSquares.end(),
                "Error: Map contains repeated elements.");
  EXPECT_THAT(kSquares,
              ElementsAre(Pair(1, 1), Pair(2, 4), Pair(3, 9), Pair(4, 16)));
}

TEST(FixedFlatMapTest, MakeFixedFlatMap_UnsortedInput) {
  constexpr auto kMap =
      MakeFixedFlatMap<StringPiece, int>({{"foo", 1}, {"bar", 2}, {"baz", 3}});
  static_assert(ranges::is_sorted(kMap), "Error: Map is not sorted.");
  static_assert(ranges::adjacent_find(kMap) == kMap.end(),
                "Error: Map contains repeated elements.");
  EXPECT_THAT(kMap,
              ElementsAre(Pair("bar", 2), Pair("baz", 3), Pair("foo", 1)));
}

// Tests that even though the keys are immutable, the values of a non-const map
// can still be changed.
TEST(FixedFlatMapTest, MutableValues) {
  auto map = MakeFixedFlatMap<std::string, int>({{"bar", 1}, {"foo", 2}});
  EXPECT_THAT(map, ElementsAre(Pair("bar", 1), Pair("foo", 2)));
  map.at("bar") = 2;
  EXPECT_THAT(map, ElementsAre(Pair("bar", 2), Pair("foo", 2)));
}

}  // namespace base
