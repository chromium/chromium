// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ranges/ranges.h"

#include <array>
#include <initializer_list>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Test struct with free function overloads for begin and end. Tests whether the
// free functions are found.
struct S {
  std::vector<int> v;
};

auto begin(const S& s) {
  return s.v.begin();
}

auto end(const S& s) {
  return s.v.end();
}

// Test struct with both member and free function overloads for begin and end.
// Tests whether the member function is preferred.
struct T {
  constexpr int begin() const { return 1; }
  constexpr int end() const { return 1; }
};

constexpr int begin(const T& t) {
  return 2;
}

constexpr int end(const T& t) {
  return 2;
}

// constexpr utility to generate a std::array. Ensures that a mutable array can
// be used in a constexpr context.
template <size_t N>
constexpr std::array<int, N> GenerateArray() {
  std::array<int, N> arr{};
  int i = 0;
  for (auto* it = ranges::begin(arr); it != ranges::end(arr); ++it) {
    *it = i++;
  }

  return arr;
}

}  // namespace

TEST(RangesTest, BeginPrefersMember) {
  constexpr T t;
  static_assert(ranges::begin(t) == 1, "");
}

TEST(RangesTest, BeginConstexprContainers) {
  int arr[1]{};
  static_assert(arr == ranges::begin(arr), "");

  static constexpr std::initializer_list<int> il = {1, 2, 3};
  static_assert(il.begin() == ranges::begin(il), "");

  static constexpr std::array<int, 3> array = {1, 2, 3};
  static_assert(&array[0] == ranges::begin(array), "");
}

TEST(RangesTest, BeginRegularContainers) {
  std::vector<int> vec;
  S s;

  EXPECT_EQ(vec.begin(), ranges::begin(vec));
  EXPECT_EQ(s.v.begin(), ranges::begin(s));
}

TEST(RangesTest, EndPrefersMember) {
  constexpr T t;
  static_assert(ranges::end(t) == 1, "");
}

TEST(RangesTest, EndConstexprContainers) {
  int arr[1]{};
  static_assert(arr + 1 == ranges::end(arr), "");

  static constexpr std::initializer_list<int> il = {1, 2, 3};
  static_assert(il.end() == ranges::end(il), "");

  static constexpr std::array<int, 3> array = {1, 2, 3};
  static_assert(&array[0] + 3 == ranges::end(array), "");
}

TEST(RangesTest, EndRegularContainers) {
  std::vector<int> vec;
  S s;

  EXPECT_EQ(vec.end(), ranges::end(vec));
  EXPECT_EQ(s.v.end(), ranges::end(s));
}

TEST(RangesTest, BeginEndStdArray) {
  static constexpr std::array<int, 0> null_array = GenerateArray<0>();
  static_assert(ranges::begin(null_array) == ranges::end(null_array), "");

  static constexpr std::array<int, 3> array = GenerateArray<3>();
  static_assert(array[0] == 0, "");
  static_assert(array[1] == 1, "");
  static_assert(array[2] == 2, "");
}

}  // namespace base
