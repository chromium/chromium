// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <random>
#include <utility>

#include "partition_alloc/partition_alloc_base/ranges/algorithm.h"
#include "partition_alloc/partition_alloc_base/ranges/functional.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::Le;
using ::testing::Lt;
using ::testing::Pair;

namespace partition_alloc::internal::base {

namespace {

// A macro to work around the fact that lambdas are not constexpr in C++14.
// This will define an unnamed struct with a constexpr call operator, similarly
// to how lambdas behave in C++17+.
// Note that this does not support capture groups, so all lambdas defined like
// this must be stateless.
// Example Usage: `CONSTEXPR_LAMBDA((int i, int j) { return i + j; }) lambda;`
// TODO(crbug.com/40533712): Remove once we have constexpr lambdas for real.
#define CONSTEXPR_LAMBDA(fun)     \
  constexpr struct {              \
    constexpr bool operator() fun \
  }

struct Int {
  constexpr Int() = default;
  constexpr Int(int value) : value(value) {}

  int value = 0;
};

constexpr bool operator==(Int lhs, Int rhs) {
  return lhs.value == rhs.value;
}

// Move-only int that clears `value` when moving out.
struct MoveOnlyInt {
  MoveOnlyInt(int value) : value(value) {}
  MoveOnlyInt(MoveOnlyInt&& other) : value(std::exchange(other.value, 0)) {}

  MoveOnlyInt& operator=(MoveOnlyInt&& other) {
    value = std::exchange(other.value, 0);
    return *this;
  }

  int value = 0;
};

constexpr bool is_even(int i) {
  return i % 2 == 0;
}

template <typename Iter>
auto make_vector(Iter begin, Iter end) {
  using T = typename std::iterator_traits<Iter>::value_type;
  return std::vector<T>(begin, end);
}

}  // namespace

TEST(RangesTest, AdjacentFind) {
  constexpr int array[] = {1, 2, 3, 3};
  static_assert(array + 2 == ranges::adjacent_find(array, ranges::end(array)),
                "");
  static_assert(
      array == ranges::adjacent_find(array, ranges::end(array), ranges::less{}),
      "");

  constexpr Int ints[] = {{6}, {6}, {5}, {4}};
  static_assert(
      ints == ranges::adjacent_find(ints, ranges::equal_to{}, &Int::value), "");
  static_assert(ranges::end(ints) ==
                    ranges::adjacent_find(ints, ranges::less{}, &Int::value),
                "");
}

TEST(RangesTest, UnaryTransform) {
  int input[] = {1, 2, 3, 4, 5};
  auto plus_1 = [](int i) { return i + 1; };
  auto times_2 = [](int i) { return i * 2; };

  EXPECT_EQ(input + 4,
            ranges::transform(input + 1, input + 4, input + 1, plus_1));
  EXPECT_THAT(input, ElementsAre(1, 3, 4, 5, 5));

  int output[] = {0, 0, 0, 0, 0};
  EXPECT_EQ(output + 3,
            ranges::transform(input + 1, input + 4, output, times_2));
  EXPECT_THAT(output, ElementsAre(6, 8, 10, 0, 0));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_EQ(values + 4,
            ranges::transform(values, values, times_2, &Int::value));
  EXPECT_THAT(values, ElementsAre(Int{0}, Int{4}, Int{8}, Int{10}));
}

TEST(RangesTest, BinaryTransform) {
  int input[] = {1, 2, 3, 4, 5};
  int output[] = {0, 0, 0, 0, 0};

  EXPECT_EQ(output + 2, ranges::transform(input, input + 2, input + 3,
                                          input + 5, output, std::plus<>{}));
  EXPECT_THAT(output, ElementsAre(5, 7, 0, 0, 0));

  EXPECT_EQ(output + 5,
            ranges::transform(input, input, output, std::multiplies<>{}));
  EXPECT_THAT(output, ElementsAre(1, 4, 9, 16, 25));

  Int values[] = {{0}, {2}, {4}, {5}};
  EXPECT_EQ(values + 4,
            ranges::transform(values, values, values, std::minus<>{},
                              &Int::value, &Int::value));
  EXPECT_THAT(values, ElementsAre(Int{0}, Int{0}, Int{0}, Int{0}));
}

TEST(RangesTest, RemoveIf) {
  int input[] = {0, 1, 2, 3, 4};

  EXPECT_EQ(input + 2, ranges::remove_if(input, input + 4, is_even));
  EXPECT_EQ(input[0], 1);
  EXPECT_EQ(input[1], 3);
  EXPECT_EQ(input[4], 4);

  Int ints[] = {2, 2, 1, 1, 2, 2};
  EXPECT_EQ(ints + 2, ranges::remove_if(ints, is_even, &Int::value));
  EXPECT_EQ(ints[0], 1);
  EXPECT_EQ(ints[1], 1);
}

TEST(RangesTest, LowerBound) {
  int array[] = {0, 0, 1, 1, 2, 2};

  EXPECT_EQ(array, ranges::lower_bound(array, array + 6, -1));
  EXPECT_EQ(array, ranges::lower_bound(array, array + 6, 0));
  EXPECT_EQ(array + 2, ranges::lower_bound(array, array + 6, 1));
  EXPECT_EQ(array + 4, ranges::lower_bound(array, array + 6, 2));
  EXPECT_EQ(array + 6, ranges::lower_bound(array, array + 6, 3));

  Int ints[] = {0, 0, 1, 1, 2, 2};

  EXPECT_EQ(ints, ranges::lower_bound(ints, -1, {}, &Int::value));
  EXPECT_EQ(ints, ranges::lower_bound(ints, 0, {}, &Int::value));
  EXPECT_EQ(ints + 2, ranges::lower_bound(ints, 1, {}, &Int::value));
  EXPECT_EQ(ints + 4, ranges::lower_bound(ints, 2, {}, &Int::value));
  EXPECT_EQ(ints + 6, ranges::lower_bound(ints, 3, {}, &Int::value));

  const auto proj = [](const Int& i) { return 2 - i.value; };
  EXPECT_EQ(ints, ranges::lower_bound(ints, 3, ranges::greater{}, proj));
  EXPECT_EQ(ints, ranges::lower_bound(ints, 2, ranges::greater{}, proj));
  EXPECT_EQ(ints + 2, ranges::lower_bound(ints, 1, ranges::greater{}, proj));
  EXPECT_EQ(ints + 4, ranges::lower_bound(ints, 0, ranges::greater{}, proj));
  EXPECT_EQ(ints + 6, ranges::lower_bound(ints, -1, ranges::greater{}, proj));
}

TEST(RangesTest, UpperBound) {
  int array[] = {0, 0, 1, 1, 2, 2};

  EXPECT_EQ(array, ranges::upper_bound(array, array + 6, -1));
  EXPECT_EQ(array + 2, ranges::upper_bound(array, array + 6, 0));
  EXPECT_EQ(array + 4, ranges::upper_bound(array, array + 6, 1));
  EXPECT_EQ(array + 6, ranges::upper_bound(array, array + 6, 2));
  EXPECT_EQ(array + 6, ranges::upper_bound(array, array + 6, 3));

  Int ints[] = {0, 0, 1, 1, 2, 2};

  EXPECT_EQ(ints, ranges::upper_bound(ints, -1, {}, &Int::value));
  EXPECT_EQ(ints + 2, ranges::upper_bound(ints, 0, {}, &Int::value));
  EXPECT_EQ(ints + 4, ranges::upper_bound(ints, 1, {}, &Int::value));
  EXPECT_EQ(ints + 6, ranges::upper_bound(ints, 2, {}, &Int::value));
  EXPECT_EQ(ints + 6, ranges::upper_bound(ints, 3, {}, &Int::value));

  const auto proj = [](const Int& i) { return 2 - i.value; };
  EXPECT_EQ(ints, ranges::upper_bound(ints, 3, ranges::greater{}, proj));
  EXPECT_EQ(ints + 2, ranges::upper_bound(ints, 2, ranges::greater{}, proj));
  EXPECT_EQ(ints + 4, ranges::upper_bound(ints, 1, ranges::greater{}, proj));
  EXPECT_EQ(ints + 6, ranges::upper_bound(ints, 0, ranges::greater{}, proj));
  EXPECT_EQ(ints + 6, ranges::upper_bound(ints, -1, ranges::greater{}, proj));
}

}  // namespace partition_alloc::internal::base
