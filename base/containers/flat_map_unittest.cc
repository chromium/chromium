// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/flat_map.h"

#include <string>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/test/move_only_int.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// A flat_map is basically a interface to flat_tree. So several basic
// operations are tested to make sure things are set up properly, but the bulk
// of the tests are in flat_tree_unittests.cc.

using ::testing::ElementsAre;

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

TEST(FlatMap, IncompleteType) {
  struct A {
    using Map = flat_map<A, A>;
    int data;
    Map set_with_incomplete_type;
    Map::iterator it;
    Map::const_iterator cit;

    // We do not declare operator< because clang complains that it's unused.
  };

  A a;
}

TEST(FlatMap, RangeConstructor) {
  flat_map<int, int>::value_type input_vals[] = {
      {1, 1}, {1, 2}, {1, 3}, {2, 1}, {2, 2}, {2, 3}, {3, 1}, {3, 2}, {3, 3}};

  flat_map<int, int> first(std::begin(input_vals), std::end(input_vals));
  EXPECT_THAT(first, ElementsAre(std::make_pair(1, 1), std::make_pair(2, 1),
                                 std::make_pair(3, 1)));
}

TEST(FlatMap, MoveConstructor) {
  using pair = std::pair<MoveOnlyInt, MoveOnlyInt>;

  flat_map<MoveOnlyInt, MoveOnlyInt> original;
  original.insert(pair(MoveOnlyInt(1), MoveOnlyInt(1)));
  original.insert(pair(MoveOnlyInt(2), MoveOnlyInt(2)));
  original.insert(pair(MoveOnlyInt(3), MoveOnlyInt(3)));
  original.insert(pair(MoveOnlyInt(4), MoveOnlyInt(4)));

  flat_map<MoveOnlyInt, MoveOnlyInt> moved(std::move(original));

  EXPECT_EQ(1U, moved.count(MoveOnlyInt(1)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(2)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(3)));
  EXPECT_EQ(1U, moved.count(MoveOnlyInt(4)));
}

TEST(FlatMap, VectorConstructor) {
  using IntPair = std::pair<int, int>;
  using IntMap = flat_map<int, int>;
  std::vector<IntPair> vect{{1, 1}, {1, 2}, {2, 1}};
  IntMap map(std::move(vect));
  EXPECT_THAT(map, ElementsAre(IntPair(1, 1), IntPair(2, 1)));
}

TEST(FlatMap, InitializerListConstructor) {
  flat_map<int, int> cont(
      {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {1, 2}, {10, 10}, {8, 8}});
  EXPECT_THAT(cont, ElementsAre(std::make_pair(1, 1), std::make_pair(2, 2),
                                std::make_pair(3, 3), std::make_pair(4, 4),
                                std::make_pair(5, 5), std::make_pair(8, 8),
                                std::make_pair(10, 10)));
}

TEST(FlatMap, SortedRangeConstructor) {
  using PairType = std::pair<int, Unsortable>;
  using MapType = flat_map<int, Unsortable>;
  MapType::value_type input_vals[] = {{1, {1}}, {2, {1}}, {3, {1}}};
  MapType map(sorted_unique, std::begin(input_vals), std::end(input_vals));
  EXPECT_THAT(
      map, ElementsAre(PairType(1, {1}), PairType(2, {1}), PairType(3, {1})));
}

TEST(FlatMap, SortedCopyFromVectorConstructor) {
  using PairType = std::pair<int, Unsortable>;
  using MapType = flat_map<int, Unsortable>;
  std::vector<PairType> vect{{1, {1}}, {2, {1}}};
  MapType map(sorted_unique, vect);
  EXPECT_THAT(map, ElementsAre(PairType(1, {1}), PairType(2, {1})));
}

TEST(FlatMap, SortedMoveFromVectorConstructor) {
  using PairType = std::pair<int, Unsortable>;
  using MapType = flat_map<int, Unsortable>;
  std::vector<PairType> vect{{1, {1}}, {2, {1}}};
  MapType map(sorted_unique, std::move(vect));
  EXPECT_THAT(map, ElementsAre(PairType(1, {1}), PairType(2, {1})));
}

TEST(FlatMap, SortedInitializerListConstructor) {
  using PairType = std::pair<int, Unsortable>;
  flat_map<int, Unsortable> map(
      sorted_unique,
      {{1, {1}}, {2, {2}}, {3, {3}}, {4, {4}}, {5, {5}}, {8, {8}}, {10, {10}}});
  EXPECT_THAT(map,
              ElementsAre(PairType(1, {1}), PairType(2, {2}), PairType(3, {3}),
                          PairType(4, {4}), PairType(5, {5}), PairType(8, {8}),
                          PairType(10, {10})));
}

TEST(FlatMap, InitializerListAssignment) {
  flat_map<int, int> cont;
  cont = {{1, 1}, {2, 2}};
  EXPECT_THAT(cont, ElementsAre(std::make_pair(1, 1), std::make_pair(2, 2)));
}

TEST(FlatMap, InsertFindSize) {
  base::flat_map<int, int> s;
  s.insert(std::make_pair(1, 1));
  s.insert(std::make_pair(1, 1));
  s.insert(std::make_pair(2, 2));

  EXPECT_EQ(2u, s.size());
  EXPECT_EQ(std::make_pair(1, 1), *s.find(1));
  EXPECT_EQ(std::make_pair(2, 2), *s.find(2));
  EXPECT_EQ(s.end(), s.find(7));
}

TEST(FlatMap, CopySwap) {
  base::flat_map<int, int> original;
  original.insert({1, 1});
  original.insert({2, 2});
  EXPECT_THAT(original,
              ElementsAre(std::make_pair(1, 1), std::make_pair(2, 2)));

  base::flat_map<int, int> copy(original);
  EXPECT_THAT(copy, ElementsAre(std::make_pair(1, 1), std::make_pair(2, 2)));

  copy.erase(copy.begin());
  copy.insert({10, 10});
  EXPECT_THAT(copy, ElementsAre(std::make_pair(2, 2), std::make_pair(10, 10)));

  original.swap(copy);
  EXPECT_THAT(original,
              ElementsAre(std::make_pair(2, 2), std::make_pair(10, 10)));
  EXPECT_THAT(copy, ElementsAre(std::make_pair(1, 1), std::make_pair(2, 2)));
}

// operator[](const Key&)
TEST(FlatMap, SubscriptConstKey) {
  base::flat_map<std::string, int> m;

  // Default construct elements that don't exist yet.
  int& s = m["a"];
  EXPECT_EQ(0, s);
  EXPECT_EQ(1u, m.size());

  // The returned mapped reference should refer into the map.
  s = 22;
  EXPECT_EQ(22, m["a"]);

  // Overwrite existing elements.
  m["a"] = 44;
  EXPECT_EQ(44, m["a"]);
}

// operator[](Key&&)
TEST(FlatMap, SubscriptMoveOnlyKey) {
  base::flat_map<MoveOnlyInt, int> m;

  // Default construct elements that don't exist yet.
  int& s = m[MoveOnlyInt(1)];
  EXPECT_EQ(0, s);
  EXPECT_EQ(1u, m.size());

  // The returned mapped reference should refer into the map.
  s = 22;
  EXPECT_EQ(22, m[MoveOnlyInt(1)]);

  // Overwrite existing elements.
  m[MoveOnlyInt(1)] = 44;
  EXPECT_EQ(44, m[MoveOnlyInt(1)]);
}

// Mapped& at(const Key&)
// const Mapped& at(const Key&) const
TEST(FlatMap, AtFunction) {
  base::flat_map<int, std::string> m = {{1, "a"}, {2, "b"}};

  // Basic Usage.
  EXPECT_EQ("a", m.at(1));
  EXPECT_EQ("b", m.at(2));

  // Const reference works.
  const std::string& const_ref = base::as_const(m).at(1);
  EXPECT_EQ("a", const_ref);

  // Reference works, can operate on the string.
  m.at(1)[0] = 'x';
  EXPECT_EQ("x", m.at(1));

  // Out-of-bounds will CHECK.
  EXPECT_DEATH_IF_SUPPORTED(m.at(-1), "");
  EXPECT_DEATH_IF_SUPPORTED({ m.at(-1)[0] = 'z'; }, "");

  // Heterogeneous look-up works.
  base::flat_map<std::string, int> m2 = {{"a", 1}, {"b", 2}};
  EXPECT_EQ(1, m2.at(base::StringPiece("a")));
  EXPECT_EQ(2, base::as_const(m2).at(base::StringPiece("b")));
}

// insert_or_assign(K&&, M&&)
TEST(FlatMap, InsertOrAssignMoveOnlyKey) {
  base::flat_map<MoveOnlyInt, MoveOnlyInt> m;

  // Initial insertion should return an iterator to the element and set the
  // second pair member to |true|. The inserted key and value should be moved
  // from.
  MoveOnlyInt key(1);
  MoveOnlyInt val(22);
  auto result = m.insert_or_assign(std::move(key), std::move(val));
  EXPECT_EQ(1, result.first->first.data());
  EXPECT_EQ(22, result.first->second.data());
  EXPECT_TRUE(result.second);
  EXPECT_EQ(1u, m.size());
  EXPECT_EQ(0, key.data());  // moved from
  EXPECT_EQ(0, val.data());  // moved from

  // Second call with same key should result in an assignment, overwriting the
  // old value. Assignment should be indicated by setting the second pair member
  // to |false|. Only the inserted value should be moved from, the key should be
  // left intact.
  key = MoveOnlyInt(1);
  val = MoveOnlyInt(44);
  result = m.insert_or_assign(std::move(key), std::move(val));
  EXPECT_EQ(1, result.first->first.data());
  EXPECT_EQ(44, result.first->second.data());
  EXPECT_FALSE(result.second);
  EXPECT_EQ(1u, m.size());
  EXPECT_EQ(1, key.data());  // not moved from
  EXPECT_EQ(0, val.data());  // moved from

  // Check that random insertion results in sorted range.
  base::flat_map<MoveOnlyInt, int> map;
  for (int i : {3, 1, 5, 6, 8, 7, 0, 9, 4, 2}) {
    map.insert_or_assign(MoveOnlyInt(i), i);
    EXPECT_TRUE(ranges::is_sorted(map));
  }
}

// insert_or_assign(const_iterator hint, K&&, M&&)
TEST(FlatMap, InsertOrAssignMoveOnlyKeyWithHint) {
  base::flat_map<MoveOnlyInt, MoveOnlyInt> m;

  // Initial insertion should return an iterator to the element. The inserted
  // key and value should be moved from.
  MoveOnlyInt key(1);
  MoveOnlyInt val(22);
  auto result = m.insert_or_assign(m.end(), std::move(key), std::move(val));
  EXPECT_EQ(1, result->first.data());
  EXPECT_EQ(22, result->second.data());
  EXPECT_EQ(1u, m.size());
  EXPECT_EQ(0, key.data());  // moved from
  EXPECT_EQ(0, val.data());  // moved from

  // Second call with same key should result in an assignment, overwriting the
  // old value. Only the inserted value should be moved from, the key should be
  // left intact.
  key = MoveOnlyInt(1);
  val = MoveOnlyInt(44);
  result = m.insert_or_assign(m.end(), std::move(key), std::move(val));
  EXPECT_EQ(1, result->first.data());
  EXPECT_EQ(44, result->second.data());
  EXPECT_EQ(1u, m.size());
  EXPECT_EQ(1, key.data());  // not moved from
  EXPECT_EQ(0, val.data());  // moved from

  // Check that random insertion results in sorted range.
  base::flat_map<MoveOnlyInt, int> map;
  for (int i : {3, 1, 5, 6, 8, 7, 0, 9, 4, 2}) {
    map.insert_or_assign(map.end(), MoveOnlyInt(i), i);
    EXPECT_TRUE(ranges::is_sorted(map));
  }
}

// try_emplace(K&&, Args&&...)
TEST(FlatMap, TryEmplaceMoveOnlyKey) {
  base::flat_map<MoveOnlyInt, std::pair<MoveOnlyInt, MoveOnlyInt>> m;

  // Trying to emplace into an empty map should succeed. Insertion should return
  // an iterator to the element and set the second pair member to |true|. The
  // inserted key and value should be moved from.
  MoveOnlyInt key(1);
  MoveOnlyInt val1(22);
  MoveOnlyInt val2(44);
  // Test piecewise construction of mapped_type.
  auto result = m.try_emplace(std::move(key), std::move(val1), std::move(val2));
  EXPECT_EQ(1, result.first->first.data());
  EXPECT_EQ(22, result.first->second.first.data());
  EXPECT_EQ(44, result.first->second.second.data());
  EXPECT_TRUE(result.second);
  EXPECT_EQ(1u, m.size());
  EXPECT_EQ(0, key.data());   // moved from
  EXPECT_EQ(0, val1.data());  // moved from
  EXPECT_EQ(0, val2.data());  // moved from

  // Second call with same key should result in a no-op, returning an iterator
  // to the existing element and returning false as the second pair member.
  // Key and values that were attempted to be inserted should be left intact.
  key = MoveOnlyInt(1);
  auto paired_val = std::make_pair(MoveOnlyInt(33), MoveOnlyInt(55));
  // Test construction of mapped_type from pair.
  result = m.try_emplace(std::move(key), std::move(paired_val));
  EXPECT_EQ(1, result.first->first.data());
  EXPECT_EQ(22, result.first->second.first.data());
  EXPECT_EQ(44, result.first->second.second.data());
  EXPECT_FALSE(result.second);
  EXPECT_EQ(1u, m.size());
  EXPECT_EQ(1, key.data());                 // not moved from
  EXPECT_EQ(33, paired_val.first.data());   // not moved from
  EXPECT_EQ(55, paired_val.second.data());  // not moved from

  // Check that random insertion results in sorted range.
  base::flat_map<MoveOnlyInt, int> map;
  for (int i : {3, 1, 5, 6, 8, 7, 0, 9, 4, 2}) {
    map.try_emplace(MoveOnlyInt(i), i);
    EXPECT_TRUE(ranges::is_sorted(map));
  }
}

// try_emplace(const_iterator hint, K&&, Args&&...)
TEST(FlatMap, TryEmplaceMoveOnlyKeyWithHint) {
  base::flat_map<MoveOnlyInt, std::pair<MoveOnlyInt, MoveOnlyInt>> m;

  // Trying to emplace into an empty map should succeed. Insertion should return
  // an iterator to the element. The inserted key and value should be moved
  // from.
  MoveOnlyInt key(1);
  MoveOnlyInt val1(22);
  MoveOnlyInt val2(44);
  // Test piecewise construction of mapped_type.
  auto result =
      m.try_emplace(m.end(), std::move(key), std::move(val1), std::move(val2));
  EXPECT_EQ(1, result->first.data());
  EXPECT_EQ(22, result->second.first.data());
  EXPECT_EQ(44, result->second.second.data());
  EXPECT_EQ(1u, m.size());
  EXPECT_EQ(0, key.data());   // moved from
  EXPECT_EQ(0, val1.data());  // moved from
  EXPECT_EQ(0, val2.data());  // moved from

  // Second call with same key should result in a no-op, returning an iterator
  // to the existing element. Key and values that were attempted to be inserted
  // should be left intact.
  key = MoveOnlyInt(1);
  val1 = MoveOnlyInt(33);
  val2 = MoveOnlyInt(55);
  auto paired_val = std::make_pair(MoveOnlyInt(33), MoveOnlyInt(55));
  // Test construction of mapped_type from pair.
  result = m.try_emplace(m.end(), std::move(key), std::move(paired_val));
  EXPECT_EQ(1, result->first.data());
  EXPECT_EQ(22, result->second.first.data());
  EXPECT_EQ(44, result->second.second.data());
  EXPECT_EQ(1u, m.size());
  EXPECT_EQ(1, key.data());                 // not moved from
  EXPECT_EQ(33, paired_val.first.data());   // not moved from
  EXPECT_EQ(55, paired_val.second.data());  // not moved from

  // Check that random insertion results in sorted range.
  base::flat_map<MoveOnlyInt, int> map;
  for (int i : {3, 1, 5, 6, 8, 7, 0, 9, 4, 2}) {
    map.try_emplace(map.end(), MoveOnlyInt(i), i);
    EXPECT_TRUE(ranges::is_sorted(map));
  }
}

TEST(FlatMap, UsingTransparentCompare) {
  using ExplicitInt = base::MoveOnlyInt;
  base::flat_map<ExplicitInt, int> m;
  const auto& m1 = m;
  int x = 0;

  // Check if we can use lookup functions without converting to key_type.
  // Correctness is checked in flat_tree tests.
  m.count(x);
  m1.count(x);
  m.find(x);
  m1.find(x);
  m.equal_range(x);
  m1.equal_range(x);
  m.lower_bound(x);
  m1.lower_bound(x);
  m.upper_bound(x);
  m1.upper_bound(x);
  m.erase(x);

  // Check if we broke overload resolution.
  m.emplace(ExplicitInt(0), 0);
  m.emplace(ExplicitInt(1), 0);
  m.erase(m.begin());
  m.erase(m.cbegin());
}

}  // namespace base
