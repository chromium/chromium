// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a "No Compile Test" suite.
// http://dev.chromium.org/developers/testing/no-compile-tests

#include "base/containers/span.h"

#include <array>
#include <set>
#include <string>
#include <string_view>
#include <vector>

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
  span<int> span1(v);             // expected-error {{no matching constructor for initialization of 'span<int>'}}
  span<int, 2u> span2({1, 2});    // expected-error {{no matching constructor for initialization of 'span<int, 2U>'}}
  span<int> span3(make_span(v));  // expected-error {{no matching constructor for initialization of 'span<int>'}}
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

// Fixed-extent span construction from an initializer list is explicit.
void InitializerListConstructionIsExplicit() {
  span<const int, 3u> s = {{1, 2, 3}};  // expected-error {{chosen constructor is explicit in copy-initialization}}
}

// A std::set() should not satisfy the requirements for conversion to a span.
void StdSetConversionDisallowed() {
  std::set<int> set;
  span<int> span1(set.begin(), 0u);                // expected-error {{no matching constructor for initialization of 'span<int>'}}
  span<int> span2(set.begin(), set.end());         // expected-error {{no matching constructor for initialization of 'span<int>'}}
  span<int> span3(set);                            // expected-error {{no matching constructor for initialization of 'span<int>'}}
  auto span4 = make_span(set.begin(), 0u);         // expected-error@*:* {{no matching function for call to 'make_span'}}
  auto span5 = make_span(set.begin(), set.end());  // expected-error@*:* {{no matching function for call to 'make_span'}}
  auto span6 = make_span(set);                     // expected-error@*:* {{no matching function for call to 'make_span'}}
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

// A span can only be constructed from a range rvalue when the element type is
// read-only or the range is a borrowed range.
void SpanFromNonConstRvalueRange() {
  std::array<bool, 3> arr = {true, false, true};
  [[maybe_unused]] auto a = span(std::move(arr));  // expected-error {{no matching conversion}}

  std::string str = "ok";
  [[maybe_unused]] auto b = span(std::move(str));  // expected-error {{no matching conversion}}

  std::u16string str16 = u"ok";
  [[maybe_unused]] auto c = span(std::move(str16));  // expected-error {{no matching conversion}}

  std::vector<int> vec = {1, 2, 3, 4, 5};
  [[maybe_unused]] auto d = span(std::move(vec));  // expected-error {{no matching conversion}}
}

// make_span can only be called on a range rvalue when the element type is
// read-only or the range is a borrowed range.
void MakeSpanFromNonConstRvalueRange() {
  std::array<bool, 3> arr = {true, false, true};
  [[maybe_unused]] auto a = make_span(std::move(arr));  // expected-error {{no matching function for call to 'make_span'}}

  std::string str = "ok";
  [[maybe_unused]] auto b = make_span(std::move(str));  // expected-error {{no matching function for call to 'make_span'}}

  std::u16string str16 = u"ok";
  [[maybe_unused]] auto c = make_span(std::move(str16));  // expected-error {{no matching function for call to 'make_span'}}

  std::vector<int> vec = {1, 2, 3, 4, 5};
  [[maybe_unused]] auto d = make_span(std::move(vec));  // expected-error {{no matching function for call to 'make_span'}}
}

void Dangling() {
  // `std::array` destroyed at the end of the full expression.
  [[maybe_unused]] auto a = span<const int>(std::to_array({1, 2, 3}));     // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto b = span<const int, 3>(std::to_array({1, 2, 3}));  // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}

  // Range destroyed at the end of the full expression.
  [[maybe_unused]] auto c = span<const int>(std::vector<int>({1, 2, 3}));     // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto d = span<const int, 3>(std::vector<int>({1, 2, 3}));  // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}

  // Here the `std::string` is an lvalue, but the `std::vector`s that copy its
  // data aren't.
  std::string str = "123";
  [[maybe_unused]] auto e =
      span<const char>(std::vector<char>(str.begin(), str.end()));  // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto f =
      span<const char, 3>(std::vector<char>(str.begin(), str.end()));  // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}

  // `std::string_view`'s safety depends on the life of the referred-to buffer.
  // Here the underlying data is destroyed before the end of the full
  // expression.
  [[maybe_unused]] auto g =
      span<const char>(std::string_view(std::string("123")));  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto h =
      span<const char, 3>(std::string_view(std::string("123")));  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
  // TODO(https://github.com/llvm/llvm-project/issues/111768) Detect dangling
  // usage sufficient to enable this testcase.
#if 0
  [[maybe_unused]] auto i = as_byte_span(std::string_view(std::string("123")));  // expected-error {{object backing the pointer will be destroyed at the end of the full-expression}}
#endif

  // Spans must not outlast a referred-to C-style array. It's tricky to create
  // an object of C-style array type (not an initializer list) that is destroyed
  // before the end of the full expression, so instead test the case where the
  // referred-to array goes out of scope before the referring span.
  [] {
    int arr[3] = {1, 2, 3};
    return span<int>(arr);  // expected-error-re {{address of stack memory associated with local variable {{.*}}returned}}
  }();
  [] {
    int arr[3] = {1, 2, 3};
    return span<int, 3>(arr);  // expected-error-re {{address of stack memory associated with local variable {{.*}}returned}}
  }();

  // TODO(https://github.com/llvm/llvm-project/issues/99685) Detect dangling
  // usage sufficient to enable this testcase.
#if 0
  []() -> std::optional<span<int>> {
    int arr[3] = {1, 2, 3};
    return span<int>(arr);  // expected-error-re {{address of stack memory associated with local variable {{.*}}returned}}
  }();
#endif

  // span's `std::array` constructor takes lvalue refs, so to test the non-const
  // `element_type` case, use the same technique as above.
  [] {
    std::array arr{1, 2, 3};
    return span<int>(arr);  // expected-error-re + {{address of stack memory associated with local variable {{.*}}returned}}
  }();
  [] {
    std::array arr{1, 2, 3};
    return span<int, 3>(arr);  // expected-error-re + {{address of stack memory associated with local variable {{.*}}returned}}
  }();
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

void BadConstConversionsWithStdSpan() {
  int kData[] = {10, 11, 12};
  {
    base::span<const int, 3u> fixed_base_span(kData);
    std::span<int, 3u> s(fixed_base_span);  // expected-error {{no matching constructor}}
  }
  {
    std::span<const int, 3u> fixed_std_span(kData);
    base::span<int, 3u> s(fixed_std_span);  // expected-error {{no matching constructor}}
  }
}

void FromVolatileArrayDisallowed() {
  static volatile int array[] = {1, 2, 3};
  span<int> s(array);  // expected-error {{no matching constructor for initialization of 'span<int>'}}
}

void FixedSizeCopyTooSmall() {
  const int src[] = {1, 2, 3};
  int dst[2];
  base::span(dst).copy_from(base::span(src));  // expected-error@*:* {{no matching member function}}

  base::span(dst).copy_from(src);  // expected-error@*:* {{no matching member function}}

  base::span(dst).copy_prefix_from(src);  // expected-error@*:* {{no matching member function}}
}

void FixedSizeCopyFromNonSpan() {
  int dst[2];
  // The copy_from() template overload is not selected.
  base::span(dst).copy_from(5);  // expected-error@*:* {{no matching member function for call to 'copy_from'}}
}

void FixedSizeSplitAtOutOfBounds() {
  const int arr[] = {1, 2, 3};
  base::span(arr).split_at<4u>();  // expected-error@*:* {{no matching member function for call to 'split_at'}}
}

void FromRefLifetimeBoundErrorForIntLiteral() {
  // Testing that `LIFETIME_BOUND` works as intended.
  [[maybe_unused]] auto wont_work = span_from_ref<const int>(123);        // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto wont_work2 = byte_span_from_ref<const int>(123);  // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}
}

void FromRefLifetimeBoundErrorForTemporaryStringObject() {
  // Testing that `LIFETIME_BOUND` works as intended.
  [[maybe_unused]] auto wont_work =
      span_from_ref<const std::string>("temporary string");  // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto wont_work2 =
      as_byte_span(std::string("temporary string"));  // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}
}

void InitializerListLifetime() {
  // `std::initializer_list` destroyed at the end of the full expression.
  [[maybe_unused]] auto wont_work = span<const int>({1, 2});      // expected-error-re {{array backing local initializer list {{.*}}will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto wont_work2 = span<const int, 3>({1, 2});  // expected-error-re {{array backing local initializer list {{.*}}will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto wont_work3 = as_byte_span({1, 2});        // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}
}

void FromCStringThatIsntStaticLifetime() {
  [[maybe_unused]] auto wont_work = span_from_cstring({'a', 'b', '\0'});        // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}
  [[maybe_unused]] auto wont_work2 = byte_span_from_cstring({'a', 'b', '\0'});  // expected-error-re {{temporary whose address is used as value of local variable {{.*}}will be destroyed at the end of the full-expression}}
}

void CompareFixedSizeMismatch() {
  const int arr[] = {1, 2, 3};
  const int arr2[] = {1, 2, 3, 4};
  (void)(span(arr) == arr2);  // expected-error@*:* {{invalid operands to binary expression}}
  (void)(span(arr) == span(arr2));  // expected-error@*:* {{invalid operands to binary expression}}
}

void CompareNotComparable() {
  struct NoEq { int i; };
  static_assert(!std::equality_comparable<NoEq>);

  const NoEq arr[] = {{1}, {2}, {3}};
  (void)(span(arr) == arr);  // expected-error@*:* {{invalid operands to binary expression}}
  (void)(span(arr) == span(arr));  // expected-error@*:* {{invalid operands to binary expression}}

  struct SelfEq {
    constexpr bool operator==(SelfEq s) const { return i == s.i; }
    int i;
  };
  static_assert(std::equality_comparable<SelfEq>);
  static_assert(!std::equality_comparable_with<SelfEq, int>);

  const SelfEq self_arr[] = {{1}, {2}, {3}};
  const int int_arr[] = {1, 2, 3};

  (void)(span(self_arr) == int_arr);  // expected-error@*:* {{invalid operands to binary expression}}
  (void)(span(self_arr) == span(int_arr));  // expected-error@*:* {{invalid operands to binary expression}}

  // Span's operator== works on `const T` and thus won't be able to use the
  // non-const operator here. We get this from equality_comparable which also
  // requires it.
  struct NonConstEq {
    constexpr bool operator==(NonConstEq s) { return i == s.i; }
    int i;
  };
  const NonConstEq non_arr[] = {{1}, {2}, {3}};
  (void)(span(non_arr) == non_arr);  // expected-error@*:* {{invalid operands to binary expression}}
  (void)(span(non_arr) == span(non_arr));  // expected-error@*:* {{invalid operands to binary expression}}
}

void AsStringViewNotBytes() {
  const int arr[] = {1, 2, 3};
  as_string_view(base::span(arr));  // expected-error@*:* {{no matching function for call to 'as_string_view'}}
}

void SpanFromCstrings() {
  static const char with_null[] = { 'a', 'b', '\0' };
  base::span_from_cstring(with_null);

  // Can't call span_from_cstring and friends with a non-null-terminated char
  // array.
  static const char no_null[] = { 'a', 'b' };
  base::span_from_cstring(no_null);  // expected-error@*:* {{no matching function for call to 'span_from_cstring'}}
  base::span_with_nul_from_cstring(no_null);  // expected-error@*:* {{no matching function for call to 'span_with_nul_from_cstring'}}
  base::byte_span_from_cstring(no_null);  // expected-error@*:* {{no matching function for call to 'byte_span_from_cstring'}}
  base::byte_span_with_nul_from_cstring(no_null);  // expected-error@*:* {{no matching function for call to 'byte_span_with_nul_from_cstring'}}
}

}  // namespace base
