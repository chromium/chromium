// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/checked_iterators.h"

#include <algorithm>
#include <iterator>

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/ranges/algorithm.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(CheckedContiguousIterator, SatisfiesContiguousIteratorConcept) {
  static_assert(std::contiguous_iterator<CheckedContiguousIterator<int>>);
}

template <class T, size_t N>
constexpr CheckedContiguousConstIterator<T> MakeConstIter(T (&arr)[N],
                                                          size_t cur) {
  // We allow cur == N as that makes a pointer at one-past-the-end which is
  // considered part of the same allocation.
  CHECK_LE(cur, N);
  return
      // SAFETY: `arr` has 1 element, `arr + 1` is considered a pointer into the
      // same allocation, as it's one past the end.
      UNSAFE_BUFFERS(
          CheckedContiguousConstIterator<T>(arr, arr + cur, arr + N));
}

template <class T, size_t N>
constexpr CheckedContiguousIterator<T> MakeIter(T (&arr)[N], size_t cur) {
  // We allow cur == N as that makes a pointer at one-past-the-end which is
  // considered part of the same allocation.
  CHECK_LE(cur, N);
  return
      // SAFETY: `arr` has 1 element, `arr + 1` is considered a pointer into the
      // same allocation, as it's one past the end.
      UNSAFE_BUFFERS(CheckedContiguousIterator<T>(arr, arr + cur, arr + N));
}

// Checks that constexpr CheckedContiguousConstIterators can be compared at
// compile time.
TEST(CheckedContiguousIterator, StaticComparisonOperators) {
  static constexpr int arr[] = {0};

  constexpr CheckedContiguousConstIterator<int> begin = MakeConstIter(arr, 0u);
  constexpr CheckedContiguousConstIterator<int> end = MakeConstIter(arr, 1u);

  static_assert(begin == begin);
  static_assert(end == end);

  static_assert(begin != end);
  static_assert(end != begin);

  static_assert(begin < end);

  static_assert(begin <= begin);
  static_assert(begin <= end);
  static_assert(end <= end);

  static_assert(end > begin);

  static_assert(end >= end);
  static_assert(end >= begin);
  static_assert(begin >= begin);
}

// Checks that comparison between iterators and const iterators works in both
// directions.
TEST(CheckedContiguousIterator, ConvertingComparisonOperators) {
  static int arr[] = {0};

  CheckedContiguousIterator<int> begin = MakeIter(arr, 0u);
  CheckedContiguousConstIterator<int> cbegin = MakeConstIter(arr, 0u);

  CheckedContiguousIterator<int> end = MakeIter(arr, 1u);
  CheckedContiguousConstIterator<int> cend = MakeConstIter(arr, 1u);

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

TEST(CheckedContiguousIteratorDeathTest, OutOfBounds) {
  static int arr[] = {0, 1, 2};

  CheckedContiguousIterator<int> it = MakeIter(arr, 1u);

  EXPECT_CHECK_DEATH(base::debug::Alias(&it[-2]));
  EXPECT_EQ(it[-1], 0);
  EXPECT_EQ(it[0], 1);
  EXPECT_EQ(it[1], 2);
  EXPECT_CHECK_DEATH(base::debug::Alias(&it[3]));

  it += 2;  // At [3], in bounds (at end).
  it -= 3;  // At [0], in bounds.
  it += 1;  // Back to [1], in bounds.

  EXPECT_CHECK_DEATH({
    it -= 2;
    base::debug::Alias(&it);
  });
  EXPECT_CHECK_DEATH({
    it += 3;
    base::debug::Alias(&it);
  });
  EXPECT_CHECK_DEATH({
    auto o = it - 2;
    base::debug::Alias(&o);
  });
  EXPECT_CHECK_DEATH({
    auto o = it + 3;
    base::debug::Alias(&o);
  });

  it++;  // At [2], in bounds.
  ++it;  // At [3], in bounds (at end).
  EXPECT_CHECK_DEATH({
    ++it;
    base::debug::Alias(&it);
  });
  EXPECT_CHECK_DEATH({
    it++;
    base::debug::Alias(&it);
  });

  it -= 3;  // At [0], in bounds.
  EXPECT_CHECK_DEATH({
    --it;
    base::debug::Alias(&it);
  });
  EXPECT_CHECK_DEATH({
    it--;
    base::debug::Alias(&it);
  });
}

}  // namespace base

namespace {

// Helper template that wraps an iterator and disables its dereference and
// increment operations.
template <typename Iterator>
struct DisableDerefAndIncr : Iterator {
  using Iterator::Iterator;

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr DisableDerefAndIncr(const Iterator& iter) : Iterator(iter) {}

  void operator*() = delete;
  void operator++() = delete;
  void operator++(int) = delete;
};

}  // namespace

// Inherit `pointer_traits` specialization from the base class.
template <typename Iter>
struct std::pointer_traits<DisableDerefAndIncr<Iter>>
    : ::std::pointer_traits<Iter> {};

namespace base {

// Tests that using std::copy with CheckedContiguousIterator<int> results in an
// optimized code-path that does not invoke the iterator's dereference and
// increment operations, as expected in libc++. This fails to compile if
// std::copy is not optimized.
// NOTE: This test relies on implementation details of the STL and thus might
// break in the future during a libc++ roll. If this does happen, please reach
// out to memory-safety-dev@chromium.org to reevaluate whether this test will
// still be needed.
#if defined(_LIBCPP_VERSION)
TEST(CheckedContiguousIterator, OptimizedCopy) {
  using Iter = DisableDerefAndIncr<CheckedContiguousIterator<int>>;

  int arr_in[5] = {1, 2, 3, 4, 5};
  int arr_out[5];

  Iter in_begin = MakeIter(arr_in, 0u);
  Iter in_end = MakeIter(arr_in, 5u);
  Iter out_begin = MakeIter(arr_out, 0u);
  Iter out_end = std::copy(in_begin, in_end, out_begin);
  EXPECT_EQ(out_end, out_begin + (in_end - in_begin));

  EXPECT_TRUE(ranges::equal(arr_in, arr_out));
}
#endif  // defined(_LIBCPP_VERSION)

}  // namespace base
