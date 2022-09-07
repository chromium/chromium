// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/checked_iterators.h"

namespace base {

#if defined(NCTEST_CHECKED_ITERATORS_CONSTRUCTOR_START_END)  // [r"constexpr variable 'iter' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // start can't be larger than end
  constexpr CheckedContiguousIterator<const int> iter(kArray + 1, kArray);
}

#elif defined(NCTEST_CHECKED_ITERATORS_CONSTRUCTOR_START_CURRENT)  // [r"constexpr variable 'iter' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // current can't be larger than start
  constexpr CheckedContiguousIterator<const int> iter(kArray + 1, kArray, kArray + 5);
}

#elif defined(NCTEST_CHECKED_ITERATORS_CONSTRUCTOR_CURRENT_END)  // [r"constexpr variable 'iter' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // current can't be larger than end
  constexpr CheckedContiguousIterator<const int> iter(kArray, kArray + 2, kArray + 1);
}

#elif defined(NCTEST_CHECKED_ITERATORS_EQ_DIFFERENT_ITER)  // [r"constexpr variable 'equal' must be initialized by a constant expression"]

constexpr int kArray1[] = {1, 2, 3, 4, 5};
constexpr int kArray2[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't compare iterators into different containers
  constexpr CheckedContiguousIterator<const int> iter1(kArray1, kArray1 + 5);
  constexpr CheckedContiguousIterator<const int> iter2(kArray2, kArray2 + 5);
  constexpr bool equal = iter1 == iter2;
}

#elif defined(NCTEST_CHECKED_ITERATORS_NE_DIFFERENT_ITER)  // [r"constexpr variable 'not_equal' must be initialized by a constant expression"]

constexpr int kArray1[] = {1, 2, 3, 4, 5};
constexpr int kArray2[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't compare iterators into different containers
  constexpr CheckedContiguousIterator<const int> iter1(kArray1, kArray1 + 5);
  constexpr CheckedContiguousIterator<const int> iter2(kArray2, kArray2 + 5);
  constexpr bool not_equal = iter1 != iter2;
}

#elif defined(NCTEST_CHECKED_ITERATORS_LT_DIFFERENT_ITER)  // [r"constexpr variable 'less_than' must be initialized by a constant expression"]

constexpr int kArray1[] = {1, 2, 3, 4, 5};
constexpr int kArray2[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't compare iterators into different containers
  constexpr CheckedContiguousIterator<const int> iter1(kArray1, kArray1 + 5);
  constexpr CheckedContiguousIterator<const int> iter2(kArray2, kArray2 + 5);
  constexpr bool less_than = iter1 < iter2;
}

#elif defined(NCTEST_CHECKED_ITERATORS_LE_DIFFERENT_ITER)  // [r"constexpr variable 'less_equal' must be initialized by a constant expression"]

constexpr int kArray1[] = {1, 2, 3, 4, 5};
constexpr int kArray2[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't compare iterators into different containers
  constexpr CheckedContiguousIterator<const int> iter1(kArray1, kArray1 + 5);
  constexpr CheckedContiguousIterator<const int> iter2(kArray2, kArray2 + 5);
  constexpr bool less_equal = iter1 <= iter2;
}

#elif defined(NCTEST_CHECKED_ITERATORS_GT_DIFFERENT_ITER)  // [r"constexpr variable 'greater_than' must be initialized by a constant expression"]

constexpr int kArray1[] = {1, 2, 3, 4, 5};
constexpr int kArray2[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't compare iterators into different containers
  constexpr CheckedContiguousIterator<const int> iter1(kArray1, kArray1 + 5);
  constexpr CheckedContiguousIterator<const int> iter2(kArray2, kArray2 + 5);
  constexpr bool greater_than = iter1 > iter2;
}

#elif defined(NCTEST_CHECKED_ITERATORS_GE_DIFFERENT_ITER)  // [r"constexpr variable 'greater_equal' must be initialized by a constant expression"]

constexpr int kArray1[] = {1, 2, 3, 4, 5};
constexpr int kArray2[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't compare iterators into different containers
  constexpr CheckedContiguousIterator<const int> iter1(kArray1, kArray1 + 5);
  constexpr CheckedContiguousIterator<const int> iter2(kArray2, kArray2 + 5);
  constexpr bool greater_equal = iter1 >= iter2;
}

#elif defined(NCTEST_CHECKED_ITERATORS_PRE_INCR_END)  // [r"constexpr variable 'pre_incr' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

constexpr int PreIncr() {
  // Can't pre-increment the end iterator.
  CheckedContiguousIterator<const int> end_iter(kArray, kArray + 5, kArray + 5);
  ++end_iter;
  return 0;
}

void WontCompile() {
  constexpr int pre_incr = PreIncr();
}

#elif defined(NCTEST_CHECKED_ITERATORS_POST_INCR_END)  // [r"constexpr variable 'post_incr' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

constexpr int PostIncr() {
  // Can't post-increment the end iterator.
  CheckedContiguousIterator<const int> end_iter(kArray, kArray + 5, kArray + 5);
  end_iter++;
  return 0;
}

void WontCompile() {
  constexpr int post_incr = PostIncr();
}

#elif defined(NCTEST_CHECKED_ITERATORS_PRE_DECR_BEGIN)  // [r"constexpr variable 'pre_decr' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

constexpr int PreDecr() {
  // Can't pre-decrement the begin iterator.
  CheckedContiguousIterator<const int> begin_iter(kArray, kArray + 5);
  --begin_iter;
  return 0;
}

void WontCompile() {
  constexpr int pre_decr = PreDecr();
}

#elif defined(NCTEST_CHECKED_ITERATORS_POST_DECR_BEGIN)  // [r"constexpr variable 'post_decr' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

constexpr int PostDecr() {
  // Can't post-decrement the begin iterator.
  CheckedContiguousIterator<const int> begin_iter(kArray, kArray + 5);
  begin_iter--;
  return 0;
}

void WontCompile() {
  constexpr int post_decr = PostDecr();
}

#elif defined(NCTEST_CHECKED_ITERATORS_INCR_PAST_END)  // [r"constexpr variable 'incr_past_end' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

constexpr int IncrPastEnd() {
  // Can't increment iterator past the end.
  CheckedContiguousIterator<const int> iter(kArray, kArray + 5);
  iter += 6;
  return 0;
}

void WontCompile() {
  constexpr int incr_past_end = IncrPastEnd();
}

#elif defined(NCTEST_CHECKED_ITERATORS_DECR_PAST_BEGIN)  // [r"constexpr variable 'decr_past_begin' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

constexpr int DecrPastBegin() {
  // Can't decrement iterator past the begin.
  CheckedContiguousIterator<const int> iter(kArray, kArray + 5);
  iter += -1;
  return 0;
}

void WontCompile() {
  constexpr int decr_past_begin = DecrPastBegin();
}

#elif defined(NCTEST_CHECKED_ITERATORS_INCR_PAST_END_2)  // [r"constexpr variable 'iter_past_end' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't increment iterator past the end.
  constexpr CheckedContiguousIterator<const int> iter(kArray, kArray + 5);
  constexpr auto iter_past_end = iter + 6;
}

#elif defined(NCTEST_CHECKED_ITERATORS_DECR_PAST_BEGIN_2)  // [r"constexpr variable 'iter_past_begin' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't decrement iterator past the begin.
  constexpr CheckedContiguousIterator<const int> iter(kArray, kArray + 5);
  constexpr auto iter_past_begin = iter + (-1);
}

#elif defined(NCTEST_CHECKED_ITERATORS_DECR_PAST_BEGIN_3)  // [r"constexpr variable 'decr_past_begin' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

constexpr int DecrPastBegin() {
  // Can't decrement iterator past the begin.
  CheckedContiguousIterator<const int> iter(kArray, kArray + 5);
  iter -= 1;
  return 0;
}

void WontCompile() {
  constexpr int decr_past_begin = DecrPastBegin();
}

#elif defined(NCTEST_CHECKED_ITERATORS_INCR_PAST_END_3)  // [r"constexpr variable 'incr_past_end' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

constexpr int IncrPastEnd() {
  // Can't increment iterator past the end.
  CheckedContiguousIterator<const int> iter(kArray, kArray + 5);
  iter -= (-6);
  return 0;
}

void WontCompile() {
  constexpr int incr_past_end = IncrPastEnd();
}

#elif defined(NCTEST_CHECKED_ITERATORS_DECR_PAST_BEGIN_4)  // [r"constexpr variable 'iter_past_begin' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't decrement iterator past the begin.
  constexpr CheckedContiguousIterator<const int> iter(kArray, kArray + 5);
  constexpr auto iter_past_begin = iter - 1;
}

#elif defined(NCTEST_CHECKED_ITERATORS_INCR_PAST_END_4)  // [r"constexpr variable 'iter_past_end' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't increment iterator past the end.
  constexpr CheckedContiguousIterator<const int> iter(kArray, kArray + 5);
  constexpr auto iter_past_end = iter - (-6);
}

#elif defined(NCTEST_CHECKED_ITERATORS_DIFFERENCE_DIFFERENT_ITER)  // [r"constexpr variable 'difference' must be initialized by a constant expression"]

constexpr int kArray1[] = {1, 2, 3, 4, 5};
constexpr int kArray2[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't compare iterators into different containers
  constexpr CheckedContiguousIterator<const int> iter1(kArray1, kArray1 + 5);
  constexpr CheckedContiguousIterator<const int> iter2(kArray2, kArray2 + 5);
  constexpr auto difference = iter1 - iter2;
}

#elif defined(NCTEST_CHECKED_ITERATORS_STAR_END)  // [r"constexpr variable 'ref' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't dereference the end iterator by star.
  constexpr CheckedContiguousIterator<const int> end_iter(kArray, kArray + 5, kArray + 5);
  constexpr auto& ref = *end_iter;
}

#elif defined(NCTEST_CHECKED_ITERATORS_ARROW_END)  // [r"constexpr variable 'ptr' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't dereference the end iterator by arrow.
  constexpr CheckedContiguousIterator<const int> end_iter(kArray, kArray + 5, kArray + 5);
  constexpr auto* ptr = end_iter.operator->();
}

#elif defined(NCTEST_CHECKED_ITERATORS_NEGATIVE_OPERATOR_AT)  // [r"constexpr variable 'ref' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't use a negative index in operator[].
  constexpr CheckedContiguousIterator<const int> iter(kArray, kArray + 5);
  constexpr auto& ref = iter[-1];
}

#elif defined(NCTEST_CHECKED_ITERATORS_OPERATOR_AT_END)  // [r"constexpr variable 'ref' must be initialized by a constant expression"]

constexpr int kArray[] = {1, 2, 3, 4, 5};

void WontCompile() {
  // Can't use a operator[] to deref the end.
  constexpr CheckedContiguousIterator<const int> iter(kArray, kArray + 5);
  constexpr auto& ref = iter[5];
}

#endif

}  // namespace base
