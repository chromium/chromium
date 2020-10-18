// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/checked_range.h"
#include <algorithm>
#include <array>
#include <initializer_list>
#include <type_traits>

#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(CheckedContiguousRange, Constructor_Default) {
  constexpr CheckedContiguousRange<std::vector<int>> range;
  static_assert(range.data() == nullptr, "");
  static_assert(range.size() == 0, "");
  static_assert(range.empty(), "");
  static_assert(range.begin() == range.end(), "");
}

TEST(CheckedContiguousRange, Constructor_Vector) {
  std::vector<int> vector = {1, 2, 3, 4, 5};
  CheckedContiguousRange<std::vector<int>> range(vector);
  EXPECT_EQ(vector.data(), range.data());
  EXPECT_EQ(vector.size(), range.size());
}

TEST(CheckedContiguousRange, Constructor_String) {
  std::string str = "Hello World";
  CheckedContiguousRange<std::string> range(str);
  EXPECT_EQ(str.data(), range.data());
  EXPECT_EQ(str.size(), range.size());
}

TEST(CheckedContiguousRange, Constructor_Array) {
  static constexpr int array[] = {1, 2, 3, 4, 5};
  constexpr CheckedContiguousRange<const int[5]> range(array);
  static_assert(data(array) == range.data(), "");
  static_assert(size(array) == range.size(), "");
}

TEST(CheckedContiguousRange, Constructor_StdArray) {
  static constexpr std::array<int, 5> array = {1, 2, 3, 4, 5};
  constexpr CheckedContiguousRange<const std::array<int, 5>> range(array);
  static_assert(data(array) == range.data(), "");
  static_assert(base::size(array) == range.size(), "");
}

TEST(CheckedContiguousRange, Constructor_StringPiece) {
  static constexpr base::StringPiece str = "Hello World";
  constexpr CheckedContiguousRange<const base::StringPiece> range(str);
  static_assert(str.data() == range.data(), "");
  static_assert(str.size() == range.size(), "");
}

TEST(CheckedContiguousRange, Constructor_InitializerList) {
  static constexpr std::initializer_list<int> il = {1, 2, 3, 4, 5};
  constexpr CheckedContiguousRange<const std::initializer_list<int>> range(il);
  static_assert(base::data(il) == range.data(), "");
  static_assert(base::size(il) == range.size(), "");
}

TEST(CheckedContiguousRange, Constructor_Copy) {
  std::vector<int> vector = {1, 2, 3, 4, 5};
  CheckedContiguousRange<std::vector<int>> range(vector);
  CheckedContiguousRange<std::vector<int>> copy(range);
  EXPECT_EQ(vector.data(), copy.data());
  EXPECT_EQ(vector.size(), copy.size());
}

TEST(CheckedContiguousRange, Constructor_Move) {
  std::vector<int> vector = {1, 2, 3, 4, 5};
  CheckedContiguousRange<std::vector<int>> range(vector);
  CheckedContiguousRange<std::vector<int>> move(std::move(range));
  EXPECT_EQ(vector.data(), move.data());
  EXPECT_EQ(vector.size(), move.size());
}

TEST(CheckedContiguousRange, Copy_Assign) {
  std::vector<int> vector = {1, 2, 3, 4, 5};
  CheckedContiguousRange<std::vector<int>> range(vector);
  CheckedContiguousRange<std::vector<int>> copy;
  copy = range;
  EXPECT_EQ(vector.data(), copy.data());
  EXPECT_EQ(vector.size(), copy.size());
}

TEST(CheckedContiguousRange, Move_Assign) {
  std::vector<int> vector = {1, 2, 3, 4, 5};
  CheckedContiguousRange<std::vector<int>> range(vector);
  CheckedContiguousRange<std::vector<int>> move;
  move = std::move(range);
  EXPECT_EQ(vector.data(), move.data());
  EXPECT_EQ(vector.size(), move.size());
}

TEST(CheckedContiguousRange, Iterators) {
  std::vector<int> vector;
  CheckedContiguousRange<const std::vector<int>> range(vector);

  // Check that all ranges by the iterators compare equal, even when elements
  // are added.
  for (size_t i = 0; i < 5; ++i) {
    vector.push_back(i);
    EXPECT_TRUE(
        std::equal(vector.begin(), vector.end(), range.begin(), range.end()));
    EXPECT_TRUE(std::equal(vector.cbegin(), vector.cend(), range.cbegin(),
                           range.cend()));
    EXPECT_TRUE(std::equal(vector.rbegin(), vector.rend(), range.rbegin(),
                           range.rend()));
    EXPECT_TRUE(std::equal(vector.crbegin(), vector.crend(), range.crbegin(),
                           range.crend()));
  }
}

TEST(CheckedContiguousRange, Front) {
  static constexpr std::array<int, 5> array = {1, 2, 3, 4, 5};
  constexpr CheckedContiguousRange<const std::array<int, 5>> range(array);
  static_assert(array.front() == range.front(), "");
}

TEST(CheckedContiguousRange, Back) {
  static constexpr std::array<int, 5> array = {1, 2, 3, 4, 5};
  constexpr CheckedContiguousRange<const std::array<int, 5>> range(array);
  static_assert(array.back() == range.back(), "");
}

TEST(CheckedContiguousRange, OperatorAt_Constexpr) {
  static constexpr std::array<int, 5> array = {1, 2, 3, 4, 5};
  constexpr CheckedContiguousRange<const std::array<int, 5>> range(array);
  static_assert(array[0] == range[0], "");
  static_assert(array[1] == range[1], "");
  static_assert(array[2] == range[2], "");
  static_assert(array[3] == range[3], "");
  static_assert(array[4] == range[4], "");
}

TEST(CheckedContiguousRange, Mutable_OperatorAt) {
  std::vector<int> vector = {1, 2, 3, 4, 5};
  CheckedContiguousRange<std::vector<int>> range(vector);

  EXPECT_EQ(vector[0], range[0]);
  range[0] = 2;
  EXPECT_EQ(vector[0], 2);
}

TEST(CheckedContiguousRange, Mutable_Data) {
  std::vector<int> vector = {3, 1, 4, 2, 5};
  CheckedContiguousRange<std::vector<int>> range(vector);

  EXPECT_FALSE(ranges::is_sorted(vector));
  std::sort(range.data(), range.data() + range.size());
  EXPECT_TRUE(ranges::is_sorted(vector));
}

TEST(CheckedContiguousRange, DataSizeEmpty_Constexpr) {
  static constexpr std::array<int, 0> array = {};
  constexpr CheckedContiguousRange<const std::array<int, 0>> range(array);
  static_assert(data(array) == range.data(), "");
  static_assert(data(array) == range.cdata(), "");
  static_assert(base::size(array) == range.size(), "");
  static_assert(range.empty(), "");
}

TEST(CheckedContiguousRange, MakeCheckedContiguousRange) {
  using Vec = std::vector<int>;

  static_assert(std::is_same<CheckedContiguousRange<Vec>,
                             decltype(MakeCheckedContiguousRange(
                                 std::declval<Vec&>()))>::value,
                "");

  static_assert(std::is_same<CheckedContiguousRange<const Vec>,
                             decltype(MakeCheckedContiguousRange(
                                 std::declval<const Vec&>()))>::value,
                "");

  static_assert(std::is_same<CheckedContiguousRange<Vec>,
                             decltype(MakeCheckedContiguousRange(
                                 std::declval<Vec&&>()))>::value,
                "");

  static_assert(std::is_same<CheckedContiguousRange<const Vec>,
                             decltype(MakeCheckedContiguousRange(
                                 std::declval<const Vec&&>()))>::value,
                "");
}

TEST(CheckedContiguousRange, MakeCheckedContiguousConstRange) {
  using Vec = std::vector<int>;

  static_assert(std::is_same<CheckedContiguousRange<const Vec>,
                             decltype(MakeCheckedContiguousConstRange(
                                 std::declval<Vec&>()))>::value,
                "");

  static_assert(std::is_same<CheckedContiguousRange<const Vec>,
                             decltype(MakeCheckedContiguousConstRange(
                                 std::declval<const Vec&>()))>::value,
                "");

  static_assert(std::is_same<CheckedContiguousRange<const Vec>,
                             decltype(MakeCheckedContiguousConstRange(
                                 std::declval<Vec&&>()))>::value,
                "");

  static_assert(std::is_same<CheckedContiguousRange<const Vec>,
                             decltype(MakeCheckedContiguousConstRange(
                                 std::declval<const Vec&&>()))>::value,
                "");
}

TEST(CheckedContiguousRange, Conversions) {
  using T = std::vector<int>;
  // Check that adding const is allowed, but removing const is not.
  static_assert(std::is_convertible<CheckedContiguousRange<T>,
                                    CheckedContiguousRange<const T>>::value,
                "");
  static_assert(!std::is_constructible<CheckedContiguousRange<T>,
                                       CheckedContiguousRange<const T>>::value,
                "");

  struct Base {};
  struct Derived : Base {};
  // Check that trying to assign arrays from a derived class fails.
  static_assert(
      !std::is_constructible<CheckedContiguousRange<Base[5]>,
                             CheckedContiguousRange<Derived[5]>>::value,
      "");
}

TEST(CheckedContiguousRange, OutOfBoundsDeath) {
  std::vector<int> empty_vector;
  CheckedContiguousRange<std::vector<int>> empty_range(empty_vector);
  ASSERT_DEATH_IF_SUPPORTED(empty_range[0], "");
  ASSERT_DEATH_IF_SUPPORTED(empty_range.front(), "");
  ASSERT_DEATH_IF_SUPPORTED(empty_range.back(), "");

  static constexpr int array[] = {0, 1, 2};
  constexpr CheckedContiguousRange<const int[3]> range(array);
  ASSERT_DEATH_IF_SUPPORTED(range[3], "");
}

}  // namespace base
