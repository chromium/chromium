// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/fixed_flat_map.h"

#include <algorithm>
#include <array>
#include <compare>
#include <concepts>
#include <functional>
#include <string_view>
#include <utility>

#include "base/containers/fixed_flat_set.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Pair;

namespace base {

namespace {

struct Unsortable {
  int value;
};

bool operator==(const Unsortable& lhs, const Unsortable& rhs) {
  return lhs.value == rhs.value;
}

bool operator<(const Unsortable& lhs, const Unsortable& rhs) = delete;
bool operator<=(const Unsortable& lhs, const Unsortable& rhs) = delete;
bool operator>(const Unsortable& lhs, const Unsortable& rhs) = delete;
bool operator>=(const Unsortable& lhs, const Unsortable& rhs) = delete;

}  // namespace

TEST(FixedFlatMapTest, MakeFixedFlatMap_SortedInput) {
  constexpr auto kSquares =
      MakeFixedFlatMap<int, int>({{1, 1}, {2, 4}, {3, 9}, {4, 16}});
  static_assert(std::ranges::is_sorted(kSquares), "Error: Map is not sorted.");
  static_assert(std::ranges::adjacent_find(kSquares) == kSquares.end(),
                "Error: Map contains repeated elements.");
  EXPECT_THAT(kSquares,
              ElementsAre(Pair(1, 1), Pair(2, 4), Pair(3, 9), Pair(4, 16)));
}

TEST(FixedFlatMapTest, MakeFixedFlatMap_UnsortedInput) {
  constexpr auto kMap = MakeFixedFlatMap<std::string_view, int>(
      {{"foo", 1}, {"bar", 2}, {"baz", 3}});
  static_assert(std::ranges::is_sorted(kMap), "Error: Map is not sorted.");
  static_assert(std::ranges::adjacent_find(kMap) == kMap.end(),
                "Error: Map contains repeated elements.");
  EXPECT_THAT(kMap,
              ElementsAre(Pair("bar", 2), Pair("baz", 3), Pair("foo", 1)));
}

// Tests that even though the keys are immutable, the values of a non-const map
// can still be changed.
TEST(FixedFlatMapTest, MutableValues) {
  auto map = MakeFixedFlatMap<std::string_view, int>({{"bar", 1}, {"foo", 2}});
  EXPECT_THAT(map, ElementsAre(Pair("bar", 1), Pair("foo", 2)));
  map.at("bar") = 2;
  EXPECT_THAT(map, ElementsAre(Pair("bar", 2), Pair("foo", 2)));
}

// Tests that even though the values are unsortable the built in sort still
// correctly orders the keys.
TEST(FixedFlatMapTest, UnsortableValues) {
  using PairType = std::pair<int, Unsortable>;

  constexpr auto kSquares = MakeFixedFlatMap<int, Unsortable>({
      {4, {16}},
      {3, {9}},
      {2, {4}},
      {1, {1}},
  });
  EXPECT_THAT(kSquares, ElementsAre(PairType(1, {1}), PairType(2, {4}),
                                    PairType(3, {9}), PairType(4, {16})));
}

TEST(FixedFlatMapTest, MakeFixedFlatMap_StdArray) {
  constexpr auto kEntries = std::to_array<std::pair<int, Unsortable>>({
      {3, {9}},
      {1, {1}},
      {2, {4}},
  });
  constexpr auto kMap = MakeFixedFlatMap<int, Unsortable>(kEntries);
  EXPECT_THAT(kMap, ElementsAre(Pair(1, Unsortable{1}), Pair(2, Unsortable{4}),
                                Pair(3, Unsortable{9})));

  // Implicit pair conversion without a projection.
  constexpr auto kRawEntries = std::to_array<std::pair<const char*, int>>({
      {"foo", 1},
      {"bar", 2},
      {"baz", 3},
  });
  constexpr auto kStrings =
      MakeFixedFlatMap<std::string_view, int>(kRawEntries);
  EXPECT_THAT(kStrings,
              ElementsAre(Pair("bar", 2), Pair("baz", 3), Pair("foo", 1)));

  // Non-default-constructible key and mapped types.
  struct NoDefault {
    constexpr explicit NoDefault(int v) : value(v) {}
    auto operator<=>(const NoDefault&) const = default;
    int value;
  };
  static_assert(!std::default_initializable<NoDefault>);
  constexpr auto kNoDefault = std::to_array<std::pair<NoDefault, NoDefault>>({
      {NoDefault(2), NoDefault(20)},
      {NoDefault(1), NoDefault(10)},
  });
  constexpr auto kNoDefaultMap =
      MakeFixedFlatMap<NoDefault, NoDefault>(kNoDefault);
  EXPECT_THAT(kNoDefaultMap, ElementsAre(Pair(NoDefault(1), NoDefault(10)),
                                         Pair(NoDefault(2), NoDefault(20))));

  // Empty std::array.
  constexpr auto kEmpty =
      MakeFixedFlatMap<int, int>(std::array<std::pair<int, int>, 0>{});
  static_assert(kEmpty.empty());
}

TEST(FixedFlatMapTest, MakeFixedFlatMap_StdArrayWithProjection) {
  struct Row {
    std::string_view name;
    int value;
  };
  constexpr auto kRows = std::to_array<Row>({
      {"foo", 1},
      {"bar", 2},
      {"baz", 3},
  });
  constexpr auto kMap = MakeFixedFlatMap<std::string_view, int>(
      kRows, /*comp=*/{},
      [](const Row& row) { return std::pair(row.name, row.value); });
  EXPECT_THAT(kMap,
              ElementsAre(Pair("bar", 2), Pair("baz", 3), Pair("foo", 1)));
}

TEST(FixedFlatMapTest, MakeFixedFlatMap_FromFixedFlatMap) {
  constexpr auto kMap = MakeFixedFlatMap<std::string_view, int>(
      {{"foo", 1}, {"bar", 2}, {"baz", 3}});

  // Invert with comparator passed as a value argument.
  constexpr auto kInverted = MakeFixedFlatMap<int, std::string_view>(
      kMap, std::greater<>(),
      [](const auto& entry) { return std::pair(entry.second, entry.first); });
  EXPECT_THAT(kInverted,
              ElementsAre(Pair(3, "baz"), Pair(2, "bar"), Pair(1, "foo")));

  // Invert with comparator specified as an explicit template argument.
  constexpr auto kInvertedExplicitComp =
      MakeFixedFlatMap<int, std::string_view, std::greater<>>(
          kMap, /*comp=*/{}, [](const auto& entry) {
            return std::pair(entry.second, entry.first);
          });
  EXPECT_EQ(kInvertedExplicitComp, kInverted);

  // Re-sort with the default identity projection (no lambda needed to drop
  // const from keys).
  constexpr auto kDescending =
      MakeFixedFlatMap<std::string_view, int, std::greater<>>(kMap);
  EXPECT_THAT(kDescending,
              ElementsAre(Pair("foo", 1), Pair("baz", 3), Pair("bar", 2)));
}

TEST(FixedFlatMapTest, MakeFixedFlatMap_FromFixedFlatSet) {
  constexpr auto kKeys = MakeFixedFlatSet<int>({1, 2, 3});
  constexpr auto kSquares = MakeFixedFlatMap<int, int>(
      kKeys, /*comp=*/{}, [](int key) { return std::pair(key, key * key); });
  EXPECT_THAT(kSquares, ElementsAre(Pair(1, 1), Pair(2, 4), Pair(3, 9)));
}

TEST(FixedFlatMapTest, MakeFixedFlatMap_CArrayBackwardsCompat) {
  constexpr auto kExplicit = [] {
    std::pair<int, int> entries[] = {{2, 4}, {1, 1}};
    return MakeFixedFlatMap<int, int>(std::move(entries));
  }();
  constexpr auto kDeduced = [] {
    std::pair<int, int> entries[] = {{2, 4}, {1, 1}};
    return MakeFixedFlatMap(std::move(entries));
  }();
  EXPECT_THAT(kExplicit, ElementsAre(Pair(1, 1), Pair(2, 4)));
  EXPECT_EQ(kDeduced, kExplicit);
}

}  // namespace base
