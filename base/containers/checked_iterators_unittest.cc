// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/checked_iterators.h"

#include <algorithm>
#include <iterator>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Checks that constexpr CheckedContiguousConstIterators can be compared at
// compile time.
TEST(CheckedContiguousIterator, StaticComparisonOperators) {
  static constexpr int arr[] = {0};

  constexpr CheckedContiguousConstIterator<int> begin(arr, arr, arr + 1);
  constexpr CheckedContiguousConstIterator<int> end(arr, arr + 1, arr + 1);

  static_assert(begin == begin, "");
  static_assert(end == end, "");

  static_assert(begin != end, "");
  static_assert(end != begin, "");

  static_assert(begin < end, "");

  static_assert(begin <= begin, "");
  static_assert(begin <= end, "");
  static_assert(end <= end, "");

  static_assert(end > begin, "");

  static_assert(end >= end, "");
  static_assert(end >= begin, "");
  static_assert(begin >= begin, "");
}

// Checks that comparison between iterators and const iterators works in both
// directions.
TEST(CheckedContiguousIterator, ConvertingComparisonOperators) {
  static int arr[] = {0};

  CheckedContiguousIterator<int> begin(arr, arr, arr + 1);
  CheckedContiguousConstIterator<int> cbegin(arr, arr, arr + 1);

  CheckedContiguousIterator<int> end(arr, arr + 1, arr + 1);
  CheckedContiguousConstIterator<int> cend(arr, arr + 1, arr + 1);

  EXPECT_EQ(begin, cbegin);
  EXPECT_EQ(cbegin, begin);
  EXPECT_EQ(end, cend);
  EXPECT_EQ(cend, end);

  EXPECT_NE(begin, cend);
  EXPECT_NE(cbegin, end);
  EXPECT_NE(end, cbegin);
  EXPECT_NE(cend, begin);

  EXPECT_LT(begin, cend);
  EXPECT_LT(cbegin, end);

  EXPECT_LE(begin, cbegin);
  EXPECT_LE(cbegin, begin);
  EXPECT_LE(begin, cend);
  EXPECT_LE(cbegin, end);
  EXPECT_LE(end, cend);
  EXPECT_LE(cend, end);

  EXPECT_GT(end, cbegin);
  EXPECT_GT(cend, begin);

  EXPECT_GE(end, cend);
  EXPECT_GE(cend, end);
  EXPECT_GE(end, cbegin);
  EXPECT_GE(cend, begin);
  EXPECT_GE(begin, cbegin);
  EXPECT_GE(cbegin, begin);
}

#if defined(_LIBCPP_VERSION)
namespace {

// Helper template that wraps an iterator and disables its dereference and
// increment operations.
template <typename Iterator>
struct DisableDerefAndIncr : Iterator {
  using Iterator::Iterator;

  void operator*() = delete;
  void operator++() = delete;
  void operator++(int) = delete;
};

template <typename Iterator>
auto __unwrap_iter(DisableDerefAndIncr<Iterator> iter) {
  return __unwrap_iter(static_cast<Iterator>(iter));
}

}  // namespace

// Tests that using std::copy with CheckedContiguousIterator<int> results in an
// optimized code-path that does not invoke the iterator's dereference and
// increment operations. This would fail to compile if std::copy was not
// optimized.
TEST(CheckedContiguousIterator, OptimizedCopy) {
  using Iter = DisableDerefAndIncr<CheckedContiguousIterator<int>>;
  static_assert(std::is_same<int*, decltype(__unwrap_iter(Iter()))>::value,
                "Error: Iter should unwrap to int*");

  int arr_in[5] = {1, 2, 3, 4, 5};
  int arr_out[5];

  Iter begin(std::begin(arr_in), std::end(arr_in));
  Iter end(std::begin(arr_in), std::end(arr_in), std::end(arr_in));
  std::copy(begin, end, arr_out);

  EXPECT_TRUE(std::equal(std::begin(arr_in), std::end(arr_in),
                         std::begin(arr_out), std::end(arr_out)));
}

TEST(CheckedContiguousIterator, UnwrapIter) {
  static_assert(
      std::is_same<int*, decltype(__unwrap_iter(
                             CheckedContiguousIterator<int>()))>::value,
      "Error: CCI<int> should unwrap to int*");

  static_assert(
      std::is_same<CheckedContiguousIterator<std::string>,
                   decltype(__unwrap_iter(
                       CheckedContiguousIterator<std::string>()))>::value,
      "Error: CCI<std::string> should unwrap to CCI<std::string>");
}

// While the result of std::copying into a range via a CCI can't be
// compared to other iterators, it should be possible to re-use it in another
// std::copy expresson.
TEST(CheckedContiguousIterator, ReuseCopyIter) {
  using Iter = CheckedContiguousIterator<int>;

  int arr_in[5] = {1, 2, 3, 4, 5};
  int arr_out[5];

  Iter begin(std::begin(arr_in), std::end(arr_in));
  Iter end(std::begin(arr_in), std::end(arr_in), std::end(arr_in));
  Iter out_begin(std::begin(arr_out), std::end(arr_out));

  auto out_middle = std::copy_n(begin, 3, out_begin);
  std::copy(begin + 3, end, out_middle);

  EXPECT_TRUE(std::equal(std::begin(arr_in), std::end(arr_in),
                         std::begin(arr_out), std::end(arr_out)));
}

#endif

}  // namespace base
