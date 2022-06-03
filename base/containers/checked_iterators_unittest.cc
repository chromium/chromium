// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/checked_iterators.h"

#include <algorithm>
#include <iterator>

#include "base/check_op.h"
#include "build/build_config.h"
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

}  // namespace base

// ChromeOS does not use the in-tree libc++, but rather a shared library that
// lags a bit behind.
// TODO(crbug.com/1166360): Enable this test on ChromeOS once the shared libc++
// is sufficiently modern.
#if defined(_LIBCPP_VERSION) && !defined(OS_NACL) && !defined(OS_CHROMEOS)
namespace {

// Helper template that wraps an iterator and disables its dereference and
// increment operations.
// Note: We don't simply delete these operations, because code using these
// operations still needs to compile, even though the codepath will never be
// taken at runtime. This will crash at runtime in case code does try to use
// these operations.
template <typename Iterator>
struct DisableDerefAndIncr : Iterator {
  using Iterator::Iterator;
  constexpr DisableDerefAndIncr(const Iterator& iter) : Iterator(iter) {}

  constexpr typename Iterator::reference operator*() {
    CHECK(false);
    return Iterator::operator*();
  }

  constexpr Iterator& operator++() {
    CHECK(false);
    return Iterator::operator++();
  }

  constexpr Iterator operator++(int i) {
    CHECK(false);
    return Iterator::operator++(i);
  }
};

}  // namespace

// Inherit `__is_cpp17_contiguous_iterator` and `pointer_traits` specializations
// from the base class.
namespace std {
template <typename Iter>
struct __is_cpp17_contiguous_iterator<DisableDerefAndIncr<Iter>>
    : __is_cpp17_contiguous_iterator<Iter> {};

template <typename Iter>
struct pointer_traits<DisableDerefAndIncr<Iter>> : pointer_traits<Iter> {};
}  // namespace std

namespace base {

// Tests that using std::copy with CheckedContiguousIterator<int> results in an
// optimized code-path that does not invoke the iterator's dereference and
// increment operations. This would fail at runtime if std::copy was not
// optimized.
TEST(CheckedContiguousIterator, OptimizedCopy) {
  using Iter = DisableDerefAndIncr<CheckedContiguousIterator<int>>;

  int arr_in[5] = {1, 2, 3, 4, 5};
  int arr_out[5];

  Iter in_begin(std::begin(arr_in), std::end(arr_in));
  Iter in_end(std::begin(arr_in), std::end(arr_in), std::end(arr_in));
  Iter out_begin(std::begin(arr_out), std::end(arr_out));
  Iter out_end = std::copy(in_begin, in_end, out_begin);
  EXPECT_EQ(out_end, out_begin + (in_end - in_begin));

  EXPECT_TRUE(std::equal(std::begin(arr_in), std::end(arr_in),
                         std::begin(arr_out), std::end(arr_out)));
}

}  // namespace base

#endif
