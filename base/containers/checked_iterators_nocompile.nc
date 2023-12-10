// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/checked_iterators.h"

namespace base {

constexpr int kArray1[] = {1, 2, 3, 4, 5};
constexpr int kArray2[] = {1, 2, 3, 4, 5};

constexpr auto GetBeginIter() {
  return CheckedContiguousIterator<const int>(kArray1, kArray1 + 5);
}

constexpr auto GetEndIter() {
  return CheckedContiguousIterator<const int>(kArray1, kArray1 + 5,
                                              kArray1 + 5);
}

void ConstructorOrdering() {
  // Start can't be larger than end.
  constexpr CheckedContiguousIterator<const int> iter1(kArray1 + 1, kArray1);  // expected-error {{constexpr variable 'iter1' must be initialized by a constant expression}}

  // Current can't be larger than start.
  constexpr CheckedContiguousIterator<const int> iter2(kArray1 + 1, kArray1,  // expected-error {{constexpr variable 'iter2' must be initialized by a constant expression}}
                                                       kArray1 + 5);

  // Current can't be larger than end.
  constexpr CheckedContiguousIterator<const int> iter3(kArray1, kArray1 + 2,  // expected-error {{constexpr variable 'iter3' must be initialized by a constant expression}}
                                                       kArray1 + 1);
}

void CompareItersFromDifferentContainers() {
  // Can't compare iterators into different containers.
  constexpr auto iter1 = GetBeginIter();
  constexpr CheckedContiguousIterator<const int> iter2(kArray2, kArray2 + 5);
  constexpr bool equal = iter1 == iter2;          // expected-error {{constexpr variable 'equal' must be initialized by a constant expression}}
  constexpr bool not_equal = iter1 != iter2;      // expected-error {{constexpr variable 'not_equal' must be initialized by a constant expression}}
  constexpr bool less_than = iter1 < iter2;       // expected-error {{constexpr variable 'less_than' must be initialized by a constant expression}}
  constexpr bool less_equal = iter1 <= iter2;     // expected-error {{constexpr variable 'less_equal' must be initialized by a constant expression}}
  constexpr bool greater_than = iter1 > iter2;    // expected-error {{constexpr variable 'greater_than' must be initialized by a constant expression}}
  constexpr bool greater_equal = iter1 >= iter2;  // expected-error {{constexpr variable 'greater_equal' must be initialized by a constant expression}}
  constexpr auto difference = iter1 - iter2;      // expected-error {{constexpr variable 'difference' must be initialized by a constant expression}}
}

void DecrementBegin() {
  constexpr auto past_begin1 = --GetBeginIter();        // expected-error {{constexpr variable 'past_begin1' must be initialized by a constant expression}}
  constexpr auto past_begin2 = GetBeginIter()--;        // expected-error {{constexpr variable 'past_begin2' must be initialized by a constant expression}}
  constexpr auto past_begin3 = (GetBeginIter() += -1);  // expected-error {{constexpr variable 'past_begin3' must be initialized by a constant expression}}
  constexpr auto past_begin4 = GetBeginIter() + -1;     // expected-error {{constexpr variable 'past_begin4' must be initialized by a constant expression}}
  constexpr auto past_begin5 = (GetBeginIter() -= 1);   // expected-error {{constexpr variable 'past_begin5' must be initialized by a constant expression}}
  constexpr auto past_begin6 = GetBeginIter() - 1;      // expected-error {{constexpr variable 'past_begin6' must be initialized by a constant expression}}
}

void IncrementBeginPastEnd() {
  constexpr auto past_end1 = (GetBeginIter() += 6);   // expected-error {{constexpr variable 'past_end1' must be initialized by a constant expression}}
  constexpr auto past_end2 = GetBeginIter() + 6;      // expected-error {{constexpr variable 'past_end2' must be initialized by a constant expression}}
  constexpr auto past_end3 = (GetBeginIter() -= -6);  // expected-error {{constexpr variable 'past_end3' must be initialized by a constant expression}}
  constexpr auto past_end4 = GetBeginIter() - -6;     // expected-error {{constexpr variable 'past_end4' must be initialized by a constant expression}}
}

void IncrementEnd() {
  constexpr auto past_end1 = ++GetEndIter();        // expected-error {{constexpr variable 'past_end1' must be initialized by a constant expression}}
  constexpr auto past_end2 = GetEndIter()++;        // expected-error {{constexpr variable 'past_end2' must be initialized by a constant expression}}
  constexpr auto past_end3 = (GetEndIter() += 1);   // expected-error {{constexpr variable 'past_end3' must be initialized by a constant expression}}
  constexpr auto past_end4 = GetEndIter() + 1;      // expected-error {{constexpr variable 'past_end4' must be initialized by a constant expression}}
  constexpr auto past_end5 = (GetEndIter() -= -1);  // expected-error {{constexpr variable 'past_end5' must be initialized by a constant expression}}
  constexpr auto past_end6 = GetEndIter() - -1;     // expected-error {{constexpr variable 'past_end6' must be initialized by a constant expression}}
}

void DecrementEndPastBegin() {
  constexpr auto past_begin1 = (GetEndIter() += -6);  // expected-error {{constexpr variable 'past_begin1' must be initialized by a constant expression}}
  constexpr auto past_begin2 = GetEndIter() + -6;     // expected-error {{constexpr variable 'past_begin2' must be initialized by a constant expression}}
  constexpr auto past_begin3 = (GetEndIter() -= 6);   // expected-error {{constexpr variable 'past_begin3' must be initialized by a constant expression}}
  constexpr auto past_begin4 = GetEndIter() - 6;      // expected-error {{constexpr variable 'past_begin4' must be initialized by a constant expression}}
}

void DerefBegin() {
  // Can't use a negative index in operator[].
  constexpr auto& ref1 = GetBeginIter()[-1];  // expected-error {{constexpr variable 'ref1' must be initialized by a constant expression}}

  // Can't use a operator[] to deref the end.
  constexpr auto& ref2 = GetBeginIter()[5];  // expected-error {{constexpr variable 'ref2' must be initialized by a constant expression}}
}

void DerefEnd() {
  // Can't dereference the end iterator.
  constexpr auto& ref1 = *GetEndIter();             // expected-error {{constexpr variable 'ref1' must be initialized by a constant expression}}
  constexpr auto* ptr = GetEndIter().operator->();  // expected-error {{constexpr variable 'ptr' must be initialized by a constant expression}}
  constexpr auto& ref2 = GetEndIter()[0];           // expected-error {{constexpr variable 'ref2' must be initialized by a constant expression}}
}

}  // namespace base
