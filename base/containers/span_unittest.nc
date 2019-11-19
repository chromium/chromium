// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/span.h"

#include <array>
#include <set>
#include <vector>

#include "base/strings/string_piece.h"

namespace base {

class Base {
};

class Derived : Base {
};

#if defined(NCTEST_DEFAULT_SPAN_WITH_NON_ZERO_STATIC_EXTENT_DISALLOWED)  // [r"fatal error: static_assert failed due to requirement '1UL == dynamic_extent || 1UL == 0' \"Invalid Extent\""]

// A default constructed span must have an extent of 0 or dynamic_extent.
void WontCompile() {
  span<int, 1> span;
}

#elif defined(NCTEST_SPAN_FROM_ARRAY_WITH_NON_MATCHING_STATIC_EXTENT_DISALLOWED) // [r"fatal error: no matching constructor for initialization of 'span<int, 1>'"]

// A span with static extent constructed from an array must match the size of
// the array.
void WontCompile() {
  int array[] = {1, 2, 3};
  span<int, 1> span(array);
}

#elif defined(NCTEST_SPAN_FROM_STD_ARRAY_WITH_NON_MATCHING_STATIC_EXTENT_DISALLOWED) // [r"fatal error: no matching constructor for initialization of 'span<int, 2>'"]

// A span with static extent constructed from std::array must match the size of
// the array.
void WontCompile() {
  std::array<int, 3> array = {1, 2, 3};
  span<int, 2> span(array);
}

#elif defined(NCTEST_SPAN_FROM_CONST_STD_ARRAY_WITH_NON_MATCHING_STATIC_EXTENT_DISALLOWED) // [r"fatal error: no matching constructor for initialization of 'span<const int, 2>'"]

// A span with static extent constructed from std::array must match the size of
// the array.
void WontCompile() {
  const std::array<int, 3> array = {1, 2, 3};
  span<const int, 2> span(array);
}

#elif defined(NCTEST_SPAN_FROM_OTHER_SPAN_WITH_MISMATCHING_EXTENT_DISALLOWED) // [r"fatal error: no matching constructor for initialization of 'span<int, 4>'"]

// A span with static extent constructed from another span must match the
// extent.
void WontCompile() {
  std::array<int, 3> array = {1, 2, 3};
  span<int, 3> span3(array);
  span<int, 4> span4(span3);
}

#elif defined(NCTEST_DYNAMIC_SPAN_TO_STATIC_SPAN_DISALLOWED)  // [r"fatal error: no matching constructor for initialization of 'span<int, 3>'"]

// Converting a dynamic span to a static span should not be allowed.
void WontCompile() {
  span<int> dynamic_span;
  span<int, 3> static_span(dynamic_span);
}

#elif defined(NCTEST_DERIVED_TO_BASE_CONVERSION_DISALLOWED)  // [r"fatal error: no matching constructor for initialization of 'span<base::Base \*>'"]

// Internally, this is represented as a pointer to pointers to Derived. An
// implicit conversion to a pointer to pointers to Base must not be allowed.
// If it were allowed, then something like this would be possible.
//   Cat** cats = GetCats();
//   Animals** animals = cats;
//   animals[0] = new Dog();  // Uhoh!
void WontCompile() {
  span<Derived*> derived_span;
  span<Base*> base_span(derived_span);
}

#elif defined(NCTEST_PTR_TO_CONSTPTR_CONVERSION_DISALLOWED)  // [r"fatal error: no matching constructor for initialization of 'span<const int \*>'"]

// Similarly, converting a span<int*> to span<const int*> requires internally
// converting T** to const T**. This is also disallowed, as it would allow code
// to violate the contract of const.
void WontCompile() {
  span<int*> non_const_span;
  span<const int*> const_span(non_const_span);
}

#elif defined(NCTEST_CONST_CONTAINER_TO_MUTABLE_CONVERSION_DISALLOWED)  // [r"fatal error: no matching constructor for initialization of 'span<int>'"]

// A const container should not be convertible to a mutable span.
void WontCompile() {
  const std::vector<int> v = {1, 2, 3};
  span<int> span(v);
}

#elif defined(NCTEST_IMPLICIT_CONVERSION_FROM_DYNAMIC_CONST_CONTAINER_TO_STATIC_SPAN_DISALLOWED) // [r"fatal error: no viable conversion from 'const std::vector<int>' to 'span<const int, 3>'"]

// A dynamic const container should not be implicitly convertible to a static span.
void WontCompile() {
  const std::vector<int> v = {1, 2, 3};
  span<const int, 3> span = v;
}

#elif defined(NCTEST_IMPLICIT_CONVERSION_FROM_DYNAMIC_MUTABLE_CONTAINER_TO_STATIC_SPAN_DISALLOWED) // [r"fatal error: no viable conversion from 'std::vector<int>' to 'span<int, 3>'"]

// A dynamic mutable container should not be implicitly convertible to a static span.
void WontCompile() {
  std::vector<int> v = {1, 2, 3};
  span<int, 3> span = v;
}

#elif defined(NCTEST_STD_SET_CONVERSION_DISALLOWED)  // [r"fatal error: no matching constructor for initialization of 'span<int>'"]

// A std::set() should not satisfy the requirements for conversion to a span.
void WontCompile() {
  std::set<int> set;
  span<int> span(set);
}

#elif defined(NCTEST_STATIC_FRONT_WITH_EXCEEDING_COUNT_DISALLOWED)  // [r" fatal error: static_assert failed due to requirement '3UL == dynamic_extent || 4UL <= 3UL' \"Count must not exceed Extent\""]

// Static first called on a span with static extent must not exceed the size.
void WontCompile() {
  std::array<int, 3> array = {1, 2, 3};
  span<int, 3> span(array);
  auto first = span.first<4>();
}

#elif defined(NCTEST_STATIC_LAST_WITH_EXCEEDING_COUNT_DISALLOWED)  // [r"fatal error: static_assert failed due to requirement '3UL == dynamic_extent || 4UL <= 3UL' \"Count must not exceed Extent\""]

// Static last called on a span with static extent must not exceed the size.
void WontCompile() {
  std::array<int, 3> array = {1, 2, 3};
  span<int, 3> span(array);
  auto last = span.last<4>();
}

#elif defined(NCTEST_STATIC_SUBSPAN_WITH_EXCEEDING_OFFSET_DISALLOWED)  // [r"fatal error: static_assert failed due to requirement '3UL == dynamic_extent || 4UL <= 3UL' \"Count must not exceed Extent\""]

// Static subspan called on a span with static extent must not exceed the size.
void WontCompile() {
  std::array<int, 3> array = {1, 2, 3};
  span<int, 3> span(array);
  auto subspan = span.subspan<4>();
}

#elif defined(NCTEST_STATIC_SUBSPAN_WITH_EXCEEDING_COUNT_DISALLOWED)  // [r"fatal error: static_assert failed due to requirement '3UL == dynamic_extent || 4UL == dynamic_extent || 4UL <= 3UL - 0UL' \"Count must not exceed Extent - Offset\""]

// Static subspan called on a span with static extent must not exceed the size.
void WontCompile() {
  std::array<int, 3> array = {1, 2, 3};
  span<int, 3> span(array);
  auto subspan = span.subspan<0, 4>();
}

#elif defined(NCTEST_DISCARD_RETURN_OF_EMPTY_DISALLOWED) // [r"ignoring return value of function"]

// Discarding the return value of empty() is not allowed.
void WontCompile() {
  span<int> s;
  s.empty();
}

#elif defined(NCTEST_FRONT_ON_EMPTY_STATIC_SPAN_DISALLOWED) // [r"Extent must not be 0"]

// Front called on an empty span with static extent is not allowed.
void WontCompile() {
  span<int, 0> s;
  s.front();
}

#elif defined(NCTEST_BACK_ON_EMPTY_STATIC_SPAN_DISALLOWED) // [r"Extent must not be 0"]

// Back called on an empty span with static extent is not allowed.
void WontCompile() {
  span<int, 0> s;
  s.back();
}

#elif defined(NCTEST_SWAP_WITH_DIFFERENT_EXTENTS_DISALLOWED)  // [r"no matching function for call to 'swap'"]

// Calling swap on spans with different extents is not allowed.
void WontCompile() {
  std::array<int, 3> array = {1, 2, 3};
  span<int, 3> static_span(array);
  span<int> dynamic_span(array);
  std::swap(static_span, dynamic_span);
}

#elif defined(NCTEST_AS_WRITABLE_BYTES_WITH_CONST_CONTAINER_DISALLOWED)  // [r"fatal error: no matching function for call to 'as_writable_bytes'"]

// as_writable_bytes should not be possible for a const container.
void WontCompile() {
  const std::vector<int> v = {1, 2, 3};
  span<uint8_t> bytes = as_writable_bytes(make_span(v));
}

#elif defined(NCTEST_MAKE_SPAN_FROM_SET_CONVERSION_DISALLOWED)  // [r"fatal error: no matching function for call to 'data'"]

// A std::set() should not satisfy the requirements for conversion to a span.
void WontCompile() {
  std::set<int> set;
  auto span = make_span(set);
}

#elif defined(NCTEST_TUPLE_SIZE_WITH_DYNAMIC_SPAN_DISALLOWED)  // [r"implicit instantiation of undefined template"]

// Invoking std::tuple_size on a dynamically sized span is not allowed.
size_t WontCompile() {
  return std::tuple_size<span<int>>::value;
}

#elif defined(NCTEST_TUPLE_ELEMENT_WITH_DYNAMIC_SPAN_DISALLOWED)  // [r"std::tuple_element<> not supported for base::span<T, dynamic_extent>"]

// Invoking std::tuple_element on a dynamically elementd span is not allowed.
void WontCompile() {
  std::tuple_element<0, span<int>>::type value;
}

#elif defined(NCTEST_TUPLE_ELEMENT_WITH_INDEX_OUT_OF_BOUNDS_DISALLOWED)  // [r"Index out of bounds in std::tuple_element<> \(base::span\)"]

// Invoking std::tuple_element with an out of bound index is not allowed.
void WontCompile() {
  std::tuple_element<0, span<int, 0>>::type value;
}

#elif defined(NCTEST_GET_WITH_DYNAMIC_SPAN_DISALLOWED)  // [r"std::get<> not supported for base::span<T, dynamic_extent>"]

// Invoking std::get on a dynamically elementd span is not allowed.
int WontCompile() {
  span<int> s;
  return std::get<0>(s);
}

#elif defined(NCTEST_GET_WITH_INDEX_OUT_OF_BOUNDS_DISALLOWED)  // [r"Index out of bounds in std::get<> \(base::span\)"]

// Invoking std::get with an out of bound index is not allowed.
int WontCompile() {
  span<int, 0> s;
  return std::get<0>(s);
}

#elif defined(NCTEST_CONST_VECTOR_DEDUCES_AS_CONST_SPAN)  // [r"fatal error: no viable conversion from 'span<const int, \[...\]>' to 'span<int, \[...\]>'"]

int WontCompile() {
  const std::vector<int> v;
  span<int> s = make_span(v);
}

#elif defined(NCTEST_STATIC_MAKE_SPAN_CHECKS_SIZE)  // [r"constexpr variable 'made_span' must be initialized by a constant expression"]

// The static make_span<N>(cont) should CHECK whether N matches size(cont). This
// should result in compilation failures when evaluated at compile time.
int WontCompile() {
  constexpr StringPiece str = "Foo";
  // Intentional extent mismatch causing CHECK failure.
  constexpr auto made_span = make_span<2>(str);
}

#endif

}  // namespace base
