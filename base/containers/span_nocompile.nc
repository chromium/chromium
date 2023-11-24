// Copyright 2017 The Chromium Authors
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

// A default constructed span must have an extent of 0 or dynamic_extent.
void DefaultSpanWithNonZeroStaticExtentDisallowed() {
  span<int, 1u> span;  // expected-error {{no matching constructor for initialization of 'span<int, 1U>'}}
}

// A span with static extent constructed from an array must match the size of
// the array.
void SpanFromArrayWithNonMatchingStaticExtentDisallowed() {
  int array[] = {1, 2, 3};
  span<int, 1u> span(array);  // expected-error {{no matching constructor for initialization of 'span<int, 1U>'}}
}

// A span with static extent constructed from another span must match the
// extent.
void SpanFromOtherSpanWithMismatchingExtentDisallowed() {
  std::array<int, 3> array = {1, 2, 3};
  span<int, 3u> span3(array);
  span<int, 4u> span4(span3);  // expected-error {{no matching constructor for initialization of 'span<int, 4U>'}}
}

// Converting a dynamic span to a static span should not be allowed.
void DynamicSpanToStaticSpanDisallowed() {
  span<int> dynamic_span;
  span<int, 3u> static_span = dynamic_span;  // expected-error-re {{no viable conversion from 'span<[...], (default) dynamic_extent aka {{.*}}>' to 'span<[...], 3>'}}
}

// Internally, this is represented as a pointer to pointers to Derived. An
// implicit conversion to a pointer to pointers to Base must not be allowed.
// If it were allowed, then something like this would be possible:
//   Cat** cats = GetCats();
//   Animals** animals = cats;
//   animals[0] = new Dog();  // Uh oh!
void DerivedToBaseConversionDisallowed() {
  span<Derived*> derived_span;
  span<Base*> base_span(derived_span);  // expected-error {{no matching constructor for initialization of 'span<Base *>'}}
}

// Similarly, converting a span<int*> to span<const int*> requires internally
// converting T** to const T**. This is also disallowed, as it would allow code
// to violate the contract of const.
void PtrToConstPtrConversionDisallowed() {
  span<int*> non_const_span;
  span<const int*> const_span(non_const_span);  // expected-error {{no matching constructor for initialization of 'span<const int *>'}}
}

// A const container should not be convertible to a mutable span.
void ConstContainerToMutableConversionDisallowed() {
  const std::vector<int> v = {1, 2, 3};
  span<int> span(v);  // expected-error {{no matching constructor for initialization of 'span<int>'}}
}

// A dynamic const container should not be implicitly convertible to a static span.
void ImplicitConversionFromDynamicConstContainerToStaticSpanDisallowed() {
  const std::vector<int> v = {1, 2, 3};
  span<const int, 3u> span = v;  // expected-error {{no viable conversion from 'const std::vector<int>' to 'span<const int, 3U>'}}
}

// A dynamic mutable container should not be implicitly convertible to a static span.
void ImplicitConversionFromDynamicMutableContainerToStaticSpanDisallowed() {
  std::vector<int> v = {1, 2, 3};
  span<int, 3u> span = v;  // expected-error {{no viable conversion from 'std::vector<int>' to 'span<int, 3U>'}}
}

// A std::set() should not satisfy the requirements for conversion to a span.
void StdSetConversionDisallowed() {
  std::set<int> set;
  span<int> span1(set.begin(), 0u);                // expected-error {{no matching constructor for initialization of 'span<int>'}}
  span<int> span2(set.begin(), set.end());         // expected-error {{no matching constructor for initialization of 'span<int>'}}
  span<int> span3(set);                            // expected-error {{no matching constructor for initialization of 'span<int>'}}
  auto span4 = make_span(set.begin(), 0u);         // expected-error@*:* {{no matching constructor for initialization of 'span<T>' (aka 'span<const int>')}}
  auto span5 = make_span(set.begin(), set.end());  // expected-error@*:* {{no matching constructor for initialization of 'span<T>' (aka 'span<const int>')}}
  auto span6 = make_span(set);                     // expected-error@*:* {{no matching function for call to 'data'}}
}

// Static views of spans with static extent must not exceed the size.
void OutOfRangeSubviewsOnStaticSpan() {
  std::array<int, 3> array = {1, 2, 3};
  span<int, 3u> span(array);
  auto first = span.first<4>();          // expected-error@*:* {{no matching member function for call to 'first'}}
  auto last = span.last<4>();            // expected-error@*:* {{no matching member function for call to 'last'}}
  auto subspan1 = span.subspan<4>();     // expected-error@*:* {{no matching member function for call to 'subspan'}}
  auto subspan2 = span.subspan<0, 4>();  // expected-error@*:* {{no matching member function for call to 'subspan'}}
}

// Discarding the return value of empty() is not allowed.
void DiscardReturnOfEmptyDisallowed() {
  span<int> s;
  s.empty();  // expected-error {{ignoring return value of function}}
}

// Getting elements of an empty span with static extent is not allowed.
void RefsOnEmptyStaticSpanDisallowed() {
  span<int, 0u> s;
  s.front();  // expected-error@*:* {{invalid reference to function 'front': constraints not satisfied}}
  s.back();   // expected-error@*:* {{invalid reference to function 'back': constraints not satisfied}}
}

// Calling swap on spans with different extents is not allowed.
void SwapWithDifferentExtentsDisallowed() {
  std::array<int, 3> array = {1, 2, 3};
  span<int, 3u> static_span(array);
  span<int> dynamic_span(array);
  std::swap(static_span, dynamic_span);  // expected-error {{no matching function for call to 'swap'}}
}

// as_writable_bytes should not be possible for a const container.
void AsWritableBytesWithConstContainerDisallowed() {
  const std::vector<int> v = {1, 2, 3};
  span<uint8_t> bytes = as_writable_bytes(make_span(v));  // expected-error {{no matching function for call to 'as_writable_bytes'}}
}

void ConstVectorDeducesAsConstSpan() {
  const std::vector<int> v;
  span<int> s = make_span(v);  // expected-error-re@*:* {{no viable conversion from 'span<{{.*}}, [...]>' to 'span<int, [...]>'}}
}

// make_span<N>() should CHECK whether N matches the actual size.
void MakeSpanChecksSize() {
  constexpr StringPiece str = "Foo";
  constexpr auto made_span1 = make_span<2>(str.begin(), 3u);         // expected-error {{constexpr variable 'made_span1' must be initialized by a constant expression}}
  constexpr auto made_span2 = make_span<2>(str.begin(), str.end());  // expected-error {{constexpr variable 'made_span2' must be initialized by a constant expression}}
  constexpr auto made_span3 = make_span<2>(str);                     // expected-error {{constexpr variable 'made_span3' must be initialized by a constant expression}}
}

// EXTENT should not result in |dynamic_extent|, it should be a compile-time
// error.
void ExtentNoDynamicExtent() {
  std::vector<uint8_t> vector;
  constexpr size_t extent = EXTENT(vector);  // expected-error@*:* {{EXTENT should only be used for containers with a static extent}}
}

void Dangling() {
  span<const int, 3u> s1{std::array<int, 3>()};     // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  span<const int> s2{std::vector<int>({1, 2, 3})};  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
}

void NotSizeTSize() {
  std::vector<int> vector = {1, 2, 3};
  // Using distinct enum types causes distinct span template instantiations, so
  // we get assertion failures below where we expect.
  enum Length1 { kSize1 = -1 };
  enum Length2 { kSize2 = -1 };
  auto s1 = make_span(vector.data(), kSize1);  // expected-error@*:* {{The source type is out of range for the destination type}}
  span s2(vector.data(), kSize2);              // expected-error@*:* {{The source type is out of range for the destination type}}
}

void FromVolatileArrayDisallowed() {
  static volatile int array[] = {1, 2, 3};
  span<int> s(array);  // expected-error {{no matching constructor for initialization of 'span<int>'}}
}

}  // namespace base
