// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_SPAN_H_
#define BASE_CONTAINERS_SPAN_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <initializer_list>
#include <iosfwd>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/checked_iterators.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/to_address.h"

// A span is a value type that represents an array of elements of type T. Since
// it only consists of a pointer to memory with an associated size, it is very
// light-weight. It is cheap to construct, copy, move and use spans, so that
// users are encouraged to use it as a pass-by-value parameter. A span does not
// own the underlying memory, so care must be taken to ensure that a span does
// not outlive the backing store.
//
// span is somewhat analogous to std::string_view, but with arbitrary element
// types, allowing mutation if T is non-const.
//
// span is implicitly convertible from C++ arrays, as well as most [1]
// container-like types that provide a data() and size() method (such as
// std::vector<T>). A mutable span<T> can also be implicitly converted to an
// immutable span<const T>.
//
// Consider using a span for functions that take a data pointer and size
// parameter: it allows the function to still act on an array-like type, while
// allowing the caller code to be a bit more concise.
//
// For read-only data access pass a span<const T>: the caller can supply either
// a span<const T> or a span<T>, while the callee will have a read-only view.
// For read-write access a mutable span<T> is required.
//
// Without span:
//   Read-Only:
//     // std::string HexEncode(const uint8_t* data, size_t size);
//     std::vector<uint8_t> data_buffer = GenerateData();
//     std::string r = HexEncode(data_buffer.data(), data_buffer.size());
//
//  Mutable:
//     // ssize_t SafeSNPrintf(char* buf, size_t N, const char* fmt, Args...);
//     char str_buffer[100];
//     SafeSNPrintf(str_buffer, sizeof(str_buffer), "Pi ~= %lf", 3.14);
//
// With span:
//   Read-Only:
//     // std::string HexEncode(span<const uint8_t> data);
//     std::vector<uint8_t> data_buffer = GenerateData();
//     std::string r = HexEncode(data_buffer);
//
//  Mutable:
//     // ssize_t SafeSNPrintf(span<char>, const char* fmt, Args...);
//     char str_buffer[100];
//     SafeSNPrintf(str_buffer, "Pi ~= %lf", 3.14);
//
// Dynamic-extent spans vs fixed-extent spans
// ------------------------------------------
//
// A `span<T>` has a dynamic extent—the size of the sequence of objects it
// refers to is only known at runtime. It is also possible to create a span with
// a fixed size at compile time by specifying the second template parameter,
// e.g. `span<int, 6>` is a span of 6 elements. Operations on a fixed-extent
// span will fail to compile if an index or size would lead to an out-of-bounds
// access.
//
// A fixed-extent span implicitly converts to a dynamic-extent span (e.g.
// `span<int, 6>` is implicitly convertible to `span<int>`), so most code that
// operates on spans of arbitrary length can just accept a `span<T>`: there is
// no need to add an additional overload for specially handling the `span<T, N>`
// case.
//
// There are several ways to go from a dynamic-extent span to a fixed-extent
// span:
// - Use the convenience `to_fixed_extent<N>()` method. This returns
//   `std::nullopt` if `size() != N`.
// - Use `first<N>()`, `last<N>()`, or `subspan<Index, N>()` to create a
//   subsequence of the original span. These methods will `CHECK()` at runtime
//   if the requested subsequence would lead to an out-of-bounds access.
// - Explicitly construct `span<T, N>` from `span<T>`: this will `CHECK()` at
//   runtime if the input span's `size()` is not exactly `N`.
//
// Spans with "const" and pointers
// -------------------------------
//
// Const and pointers can get confusing. Here are vectors of pointers and their
// corresponding spans:
//
//   const std::vector<int*>        =>  span<int* const>
//   std::vector<const int*>        =>  span<const int*>
//   const std::vector<const int*>  =>  span<const int* const>
//
// Spans with smart pointer types for internal storage (raw_ptr<T>).
// --------------------------------------------------------------------------
//
// See base/memory/raw_span.h. Note that base::raw_span<T> should be used for
// class members only, with ordinary base::span<T> used otherwise.
//
// Differences from the C++ standard
// ---------------------------------
//
// http://eel.is/c++draft/views.span contains the latest C++ draft of std::span.
// Chromium tries to follow the draft as close as possible. Differences between
// the draft and the implementation are documented in subsections below.
//
// Differences from [span.overview]:
// - Dynamic spans are implemented as a partial specialization of the regular
//   class template. This leads to significantly simpler checks involving the
//   extent, at the expense of some duplicated code. The same strategy is used
//   by libc++.
//
// Differences from [span.objectrep]:
// - as_bytes() and as_writable_bytes() return spans of uint8_t instead of
//   std::byte.
//
// Differences from [span.cons]:
// - Omits constructors from `std::array`, since separating these from the range
//   constructor is only useful to mark them `noexcept`, and Chromium doesn't
//   care about that.
// - Provides implicit conversion from fixed-extent `span` to `std::span`.
//   `std::span`'s general-purpose range constructor is explicit in this case
//   because it does not have a carve-out for `span`.
//
// Other differences:
// - Using StrictNumeric<size_type> instead of size_type where possible.
// - Allowing internal pointer types other than T*.
//
// Additions beyond the C++ standard draft
// - as_chars() function.
// - as_writable_chars() function.
// - as_byte_span() function.
// - as_writable_byte_span() function.
// - copy_from() method.
// - copy_from_nonoverlapping() method.
// - span_from_ref() function.
// - byte_span_from_ref() function.
// - span_from_cstring() function.
// - span_with_nul_from_cstring() function.
// - byte_span_from_cstring() function.
// - byte_span_with_nul_from_cstring() function.
// - split_at() method.
// - to_fixed_extent() method.
// - get_at() method.
// - span(span&&) move-constructor (helps with non-trivial InternalPtrType).
// - operator=(span&&) move-assignment (helps with non-trivial InternalPtrType).
// - operator==() comparator function.
// - operator<=>() comparator function.
// - operator<<() printing function.
//
// Due to the lack of class template argument deduction guides in C++14
// appropriate make_span() utility functions are provided for historic reasons.

namespace base {

// [span.syn]: Constants
inline constexpr size_t dynamic_extent = std::numeric_limits<size_t>::max();

template <typename ElementType,
          size_t Extent = dynamic_extent,
          typename InternalPtrType = ElementType*>
class span;

}  // namespace base

// Mark `span` as satisfying the `view` and `borrowed_range` concepts. This
// should be done before the definition of `span`, so that any inlined calls to
// range functionality use the correct specializations.
template <typename ElementType, size_t Extent, typename InternalPtrType>
inline constexpr bool
    std::ranges::enable_view<base::span<ElementType, Extent, InternalPtrType>> =
        true;
template <typename ElementType, size_t Extent, typename InternalPtrType>
inline constexpr bool std::ranges::enable_borrowed_range<
    base::span<ElementType, Extent, InternalPtrType>> = true;

namespace base {

namespace internal {

// Exposition-only concept from [span.syn]
template <typename T>
concept IntegralConstantLike =
    std::is_integral_v<decltype(T::value)> &&
    !std::is_same_v<bool, std::remove_const_t<decltype(T::value)>> &&
    std::convertible_to<T, decltype(T::value)> &&
    std::equality_comparable_with<T, decltype(T::value)> &&
    std::bool_constant<T() == T::value>::value &&
    std::bool_constant<static_cast<decltype(T::value)>(T()) == T::value>::value;

// Exposition-only concept from [span.syn]
template <typename T>
inline constexpr size_t MaybeStaticExt = dynamic_extent;
template <typename T>
  requires IntegralConstantLike<T>
inline constexpr size_t MaybeStaticExt<T> = {T::value};

template <typename From, typename To>
concept LegalDataConversion = std::is_convertible_v<From (*)[], To (*)[]>;

// Akin to `std::constructible_from<span, T>`, but meant to be used in a
// type-deducing context where we don't know what args would be deduced;
// `std::constructible_from` can't be directly used in such a case since the
// type parameters must be fully-specified (e.g. `span<int>`), requiring us to
// have that knowledge already.
template <typename T>
concept SpanConstructibleFrom = requires(T&& t) { span(std::forward<T>(t)); };

template <typename T, typename It>
concept CompatibleIter =
    std::contiguous_iterator<It> &&
    LegalDataConversion<std::remove_reference_t<std::iter_reference_t<It>>, T>;

// Disallow general-purpose range construction from types that have dedicated
// constructors.
// Arrays should go through the array constructors.
template <typename T>
inline constexpr bool kCompatibleRangeType = !std::is_array_v<T>;
// `span`s should go through the copy constructor.
template <typename T, size_t N, typename P>
inline constexpr bool kCompatibleRangeType<span<T, N, P>> = false;

template <typename T, typename R>
concept CompatibleRange =
    std::ranges::contiguous_range<R> && std::ranges::sized_range<R> &&
    (std::ranges::borrowed_range<R> ||
     std::is_const_v<T>)&&kCompatibleRangeType<std::remove_cvref_t<R>> &&
    LegalDataConversion<
        std::remove_reference_t<std::ranges::range_reference_t<R>>,
        T>;

// Whether source object extent `X` will work to create a span of fixed extent
// `N`. This is not intended for use in dynamic-extent spans.
template <size_t N, size_t X>
concept FixedExtentConstructibleFromExtent = X == N || X == dynamic_extent;

// Computes a fixed extent if possible from a source container type `T`.
template <typename T>
inline constexpr size_t kComputedExtentImpl = dynamic_extent;
template <typename T, size_t N>
inline constexpr size_t kComputedExtentImpl<T[N]> = N;
template <typename T, size_t N>
inline constexpr size_t kComputedExtentImpl<std::array<T, N>> = N;
template <typename T, size_t N>
inline constexpr size_t kComputedExtentImpl<std::span<T, N>> = N;
template <typename T, size_t N, typename InternalPtrType>
inline constexpr size_t kComputedExtentImpl<span<T, N, InternalPtrType>> = N;
template <typename T>
inline constexpr size_t kComputedExtent =
    kComputedExtentImpl<std::remove_cvref_t<T>>;

// Converts an iterator to an integral value.
//
// This is necessary when comparing iterators from different spans, since
// comparing pointers from different allocations is UB.
template <typename T>
constexpr uintptr_t AsUintptrT(const T& t) {
  return reinterpret_cast<uintptr_t>(to_address(t));
}

template <typename ByteType,
          typename ElementType,
          size_t Extent,
          typename InternalPtrType>
  requires((std::same_as<std::remove_const_t<ByteType>, char> ||
            std::same_as<std::remove_const_t<ByteType>, unsigned char>) &&
           (std::is_const_v<ByteType> || !std::is_const_v<ElementType>))
constexpr auto as_byte_span(
    span<ElementType, Extent, InternalPtrType> s) noexcept {
  constexpr size_t kByteExtent =
      Extent == dynamic_extent ? dynamic_extent : sizeof(ElementType) * Extent;
  // SAFETY: `s.data()` points to at least `s.size_bytes()` bytes' worth of
  // valid elements, so the size computed below must only contain valid
  // elements. Since `ByteType` is an alias to a character type, it has a size
  // of 1 byte, the resulting pointer has no alignment concerns, and it is not
  // UB to access memory contents inside the allocation through it.
  return UNSAFE_BUFFERS(span<ByteType, kByteExtent>(
      reinterpret_cast<ByteType*>(s.data()), s.size_bytes()));
}

}  // namespace internal

template <typename ElementType, size_t Extent, typename InternalPtrType>
class GSL_POINTER span {
 public:
  using element_type = ElementType;
  using value_type = std::remove_cv_t<element_type>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = element_type*;
  using const_pointer = const element_type*;
  using reference = element_type&;
  using const_reference = const element_type&;
  using iterator = CheckedContiguousIterator<element_type>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  static constexpr size_type extent = Extent;

  // [span.cons], span constructors, copy, assignment, and destructor
  constexpr span() noexcept
    requires(extent == 0)
  = default;

  // Constructs a span from a contiguous iterator and a size.
  //
  // # Checks
  // The function CHECKs that `count` matches the template parameter `N` and
  // will terminate otherwise.
  //
  // # Safety
  // The iterator must point to the first of at least `count` many elements, or
  // Undefined Behaviour can result as the span will allow access beyond the
  // valid range of the collection pointed to by the iterator.
  template <typename It>
    requires(internal::CompatibleIter<element_type, It>)
  UNSAFE_BUFFER_USAGE constexpr explicit span(It first,
                                              StrictNumeric<size_type> count)
      :  // The use of to_address() here is to handle the case where the
         // iterator `first` is pointing to the container's `end()`. In that
         // case we can not use the address returned from the iterator, or
         // dereference it through the iterator's `operator*`, but we can store
         // it. We must assume in this case that `count` is 0, since the
         // iterator does not point to valid data. Future hardening of iterators
         // may disallow pulling the address from `end()`, as demonstrated by
         // asserts() in libstdc++:
         // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93960.
         //
         // The span API dictates that the `data()` is accessible when size is
         // 0, since the pointer may be valid, so we cannot prevent storing and
         // giving out an invalid pointer here without breaking API
         // compatibility and our unit tests. Thus protecting against this can
         // likely only be successful from inside iterators themselves, where
         // the context about the pointer is known.
         //
         // We can not protect here generally against an invalid iterator/count
         // being passed in, since we have no context to determine if the
         // iterator or count are valid.
        data_(to_address(first)) {
    CHECK_EQ(size_type{count}, extent);

    // `count != 0` implies non-null `data_`.  Consider using
    // `SpanOrSize<T>` to represent a size that may or may not be
    // accompanied by the actual data.
    DCHECK(count == 0 || !!data_);
  }

  // Constructs a span from a contiguous iterator and a size.
  //
  // # Checks
  // The function CHECKs that `it <= end` and will terminate otherwise.
  //
  // # Safety
  // The begin and end iterators must be for the same allocation or Undefined
  // Behaviour can result as the span will allow access beyond the valid range
  // of the collection pointed to by `begin`.
  template <typename It, typename End>
    requires(internal::CompatibleIter<element_type, It> &&
             std::sized_sentinel_for<End, It> &&
             !std::is_convertible_v<End, size_t>)
  UNSAFE_BUFFER_USAGE constexpr explicit span(It first, End last)
      // SAFETY: The caller must guarantee that the iterator and end sentinel
      // are part of the same allocation, in which case it is the number of
      // elements between the iterators and thus a valid size for the pointer to
      // the element at `first`.
      //
      // We CHECK that `last - first` did not underflow below. Normally checking
      // correctness afterward is flawed, however underflow is not UB and the
      // size is not converted to an invalid pointer (which would be UB) before
      // we CHECK for underflow.
      : UNSAFE_BUFFERS(span(first, static_cast<size_type>(last - first))) {
    // Verify `last - first` did not underflow.
    CHECK(first <= last);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(
      std::type_identity_t<element_type> (&arr LIFETIME_BOUND)[extent]) noexcept
      // SAFETY: The type signature guarantees `arr` contains `extent` elements.
      : UNSAFE_BUFFERS(span(arr, extent)) {}

  template <typename R, size_t N = internal::kComputedExtent<R>>
    requires(internal::CompatibleRange<element_type, R> &&
             internal::FixedExtentConstructibleFromExtent<extent, N>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr explicit(N != extent) span(R&& range LIFETIME_BOUND)
      // SAFETY: `std::ranges::size()` returns the number of elements
      // `std::ranges::data()` will point to, so accessing those elements will
      // be safe.
      : UNSAFE_BUFFERS(
            span(std::ranges::data(range), std::ranges::size(range))) {}
  template <typename R, size_t N = internal::kComputedExtent<R>>
    requires(internal::CompatibleRange<element_type, R> &&
             internal::FixedExtentConstructibleFromExtent<extent, N> &&
             std::ranges::borrowed_range<R>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr explicit(N != extent) span(R&& range)
      // SAFETY: `std::ranges::size()` returns the number of elements
      // `std::ranges::data()` will point to, so accessing those elements will
      // be safe.
      : UNSAFE_BUFFERS(
            span(std::ranges::data(range), std::ranges::size(range))) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr explicit span(std::initializer_list<value_type> il LIFETIME_BOUND)
    requires(std::is_const_v<element_type>)
      // SAFETY: `size()` is exactly the number of elements in the initializer
      // list, so accessing that many will be safe.
      : UNSAFE_BUFFERS(span(il.begin(), il.size())) {}

  constexpr span(const span& other) noexcept = default;
  template <typename OtherElementType,
            size_t OtherExtent,
            typename OtherInternalPtrType>
    requires((OtherExtent == dynamic_extent || extent == OtherExtent) &&
             internal::LegalDataConversion<OtherElementType, element_type>)
  constexpr explicit(OtherExtent == dynamic_extent)
      span(const span<OtherElementType, OtherExtent, OtherInternalPtrType>&
               s) noexcept
      // SAFETY: `size()` is the number of elements that can be safely accessed
      // at `data()`.
      : UNSAFE_BUFFERS(span(s.data(), s.size())) {}
  constexpr span(span&& other) noexcept = default;

  constexpr span& operator=(const span& other) noexcept = default;
  constexpr span& operator=(span&& other) noexcept = default;

  // Bounds-checked copy from a possibly-overlapping span. The spans must be the
  // exact same size. For fixed-extent spans this is enforced by a compile-time
  // constraint; for dynamic-extent spans, this will be verified at runtime (via
  // a `CHECK()` in constructing the appropriate fixed-size span.)
  //
  // This is a non-std extension that is inspired by the Rust
  // slice::copy_from_slice() method.
  //
  // If it's known the spans can not overlap, `copy_from_nonoverlapping()`
  // provides an unsafe alternative that may be more performant.
  constexpr void copy_from(span<const element_type, extent> s)
    requires(!std::is_const_v<element_type>)
  {
    if (internal::AsUintptrT(begin()) <= internal::AsUintptrT(s.begin())) {
      std::ranges::copy(s, begin());
    } else {
      std::ranges::copy_backward(s, end());
    }
  }
  template <typename R, size_t N = internal::kComputedExtent<R>>
    requires(!std::is_const_v<element_type> && N == dynamic_extent &&
             std::convertible_to<R &&, span<const element_type>>)
  constexpr void copy_from(R&& other) {
    return copy_from(span<const element_type, extent>(std::forward<R>(other)));
  }

  // Bounds-checked copy from a non-overlapping span. The spans must be the
  // exact same size. For fixed-extent spans this is enforced by a compile-time
  // constraint; for dynamic-extent spans, this will be verified at runtime (via
  // a `CHECK()` in constructing the appropriate fixed-size span.)
  //
  // This is a non-std extension that is inspired by the Rust
  // slice::copy_from_slice() method.
  //
  // # Safety
  // The `other` span must not overlap with `this` or Undefined Behaviour may
  // occur.
  //
  // If the calling code is not performance sensitive, the safer copy_from()
  // method may be a simpler option.
  constexpr void copy_from_nonoverlapping(span<const element_type, extent> s)
    requires(!std::is_const_v<element_type>)
  {
    DCHECK(internal::AsUintptrT(end()) <= internal::AsUintptrT(s.begin()) ||
           internal::AsUintptrT(begin()) >= internal::AsUintptrT(s.end()));
    std::ranges::copy(s, begin());
  }
  template <typename R, size_t N = internal::kComputedExtent<R>>
    requires(!std::is_const_v<element_type> && N == dynamic_extent &&
             std::convertible_to<R &&, span<const element_type>>)
  constexpr void copy_from_nonoverlapping(R&& other) {
    copy_from_nonoverlapping(
        span<const element_type, extent>(std::forward<R>(other)));
  }

  // Bounds-checked copy from a span into the front of this span. The `other`
  // span must not be larger than this span. For fixed-extent spans this is
  // enforced by a compile-time constraint; for dynamic-extent spans, this will
  // be verified at runtime (via a `CHECK()` in constructing the prefix of this
  // span.)
  //
  // Prefer copy_from() when you expect the entire span to be written to. This
  // method does not make that guarantee and may leave some bytes uninitialized
  // in the destination span, while `copy_from()` ensures the entire span is
  // written which helps prevent bugs.
  //
  // This is sugar for `span.first(other.size()).copy_from(other)` to avoid the
  // need for writing the size twice, while also preserving compile-time size
  // information.
  template <typename R, size_t N = internal::kComputedExtent<R>>
    requires(!std::is_const_v<element_type> &&
             (N <= extent || N == dynamic_extent) &&
             std::convertible_to<R &&, span<const element_type>>)
  constexpr void copy_prefix_from(R&& other) {
    if constexpr (N == dynamic_extent) {
      return first(other.size()).copy_from(other);
    } else {
      return first<N>().copy_from(other);
    }
  }

  // Implicit conversion from span<element_type, extent> to
  // std::span<element_type, extent>.
  //
  // We get other conversions for free from std::span's constructors, but it
  // does not deduce extent on its range constructor.
  operator std::span<element_type, extent>() const {
    return std::span<element_type, extent>(*this);
  }
  operator std::span<const element_type, extent>() const
    requires(!std::is_const_v<element_type>)
  {
    return std::span<const element_type, extent>(*this);
  }

  // [span.sub], span subviews
  template <size_t Count>
  constexpr auto first() const noexcept
    requires(Count <= extent)
  {
    // SAFETY: span provides that data() points to at least `extent` many
    // elements. `Count` is non-negative by its type and `Count <= extent` from
    // the requires condition. So `Count` is a valid new size for `data()`.
    return UNSAFE_BUFFERS(span<element_type, Count>(data(), Count));
  }

  // Returns a span over the first `count` elements.
  //
  // # Checks
  // The function CHECKs that the span contains at least `count` elements and
  // will terminate otherwise.
  constexpr auto first(StrictNumeric<size_type> count) const noexcept {
    CHECK_LE(size_type{count}, extent);
    // SAFETY: span provides that data() points to at least `extent` many
    // elements. `Count` is non-negative by its type and `Count <= extent` from
    // the CHECK above. So `Count` is a valid new size for `data()`.
    return UNSAFE_BUFFERS(span<element_type>(data(), count));
  }

  template <size_t Count>
  constexpr auto last() const noexcept
    requires(Count <= extent)
  {
    // SAFETY: span provides that data() points to at least `extent` many
    // elements. `Count` is non-negative by its type and `Count <= extent` from
    // the requires condition. So `0 <= extent - Count <= extent`, meaning
    // `extent - Count` is a valid new size for `data()` and it will point to
    // `Count` many elements.`
    return UNSAFE_BUFFERS(
        span<element_type, Count>(data() + (extent - Count), Count));
  }

  // Returns a span over the last `count` elements.
  //
  // # Checks
  // The function CHECKs that the span contains at least `count` elements and
  // will terminate otherwise.
  constexpr auto last(StrictNumeric<size_type> count) const noexcept {
    CHECK_LE(size_type{count}, extent);
    // SAFETY: span provides that data() points to at least `extent` many
    // elements. `Count` is non-negative by its type and `Count <= extent` from
    // the CHECK above. So `0 <= Extent - Count <= extent`, meaning
    // `extent - Count` is a valid new size for `data()` and it will point to
    // `Count` many elements.`
    return UNSAFE_BUFFERS(
        span<element_type>(data() + (extent - size_type{count}), count));
  }

  template <size_t Offset, size_t Count = dynamic_extent>
  constexpr auto subspan() const noexcept
    requires(Offset <= extent &&
             (Count == dynamic_extent || Count <= extent - Offset))
  {
    constexpr size_t kExtent =
        Count != dynamic_extent ? Count : extent - Offset;
    // SAFETY: span provides that data() points to at least `extent` many
    // elements.
    //
    // If Count is dynamic_extent, kExtent becomes `extent - Offset`. Since
    // `Offset <= extent` from the requires condition, then `Offset` is a valid
    // offset for data(), and `Offset + kExtent = Offset + extent - Offset =
    // extent >= Offset` is also a valid offset that is not before `Offset`.
    // This makes a span at `Offset` with size `kExtent` valid.
    //
    // Otherwise `Count <= extent - Offset` and `0 <= Offset <= extent` by the
    // requires condition, so `Offset <= extent - Count` and `extent - Count`
    // can not underflow. Then `Offset` is a valid offset for data() and
    // `kExtent` is `Count <= extent - Offset`, so `Offset + kExtent <=
    // Offset + extent - Offset = extent` which makes both `Offset` and
    // `Offset + kExtent` valid offsets for data(), and since `kExtent` is
    // non-negative, `Offset + kExtent` is not before `Offset` so `kExtent` is a
    // valid size for the span at `data() + Offset`.
    return UNSAFE_BUFFERS(
        span<element_type, kExtent>(data() + Offset, kExtent));
  }

  // Returns a span over the first `count` elements starting at the given
  // `offset` from the start of the span.
  //
  // # Checks
  // The function CHECKs that the span contains at least `offset + count`
  // elements, or at least `offset` elements if `count` is not specified, and
  // will terminate otherwise.
  constexpr auto subspan(size_type offset,
                         size_type count = dynamic_extent) const noexcept {
    CHECK_LE(offset, extent);
    CHECK(count == dynamic_extent || count <= extent - offset);
    const size_type new_extent =
        count != dynamic_extent ? count : extent - offset;
    // SAFETY: span provides that data() points to at least `extent` many
    // elements.
    //
    // If Count is dynamic_extent, `new_extent` becomes `extent - offset`. Since
    // `offset <= extent` from the requires condition, then `offset` is a valid
    // offset for data(), and `offset + new_extent = offset + extent - offset =
    // extent >= offset` is also a valid offset that is not before `offset`.
    // This makes a span at `offset` with size `new_extent` valid.
    //
    // Otherwise `count <= extent - offset` and `0 <= offset <= extent` by the
    // requires condition, so `offset <= extent - count` and `extent - count`
    // can not underflow. Then `offset` is a valid offset for data() and
    // `new_extent` is `count <= extent - offset`, so `offset + new_extent <=
    // offset + extent - offset = extent` which makes both `offset` and
    // `offset + new_extent` valid offsets for data(), and since `new_extent` is
    // non-negative, `offset + new_extent` is not before `offset` so
    // `new_extent` is a valid size for the span at `data() + offset`.
    return UNSAFE_BUFFERS(span<element_type>(data() + offset, new_extent));
  }

  template <size_t Offset>
    requires(Offset <= extent)
  constexpr auto split_at() const noexcept {
    return std::pair(first<Offset>(), subspan<Offset, extent - Offset>());
  }

  // Splits a span into two at the given `offset`, returning two spans that
  // cover the full range of the original span.
  //
  // Similar to calling subspan() with the `offset` as the length on the first
  // call, and then the `offset` as the offset in the second.
  //
  // The split_at<Offset>() overload allows construction of a fixed-size span
  // from a compile-time constant. If the input span is fixed-size, both output
  // spans will be. Otherwise, the first will be fixed-size and the second will
  // be dynamic-size.
  //
  // This is a non-std extension that  is inspired by the Rust slice::split_at()
  // and split_at_mut() methods.
  //
  // # Checks
  // The function CHECKs that the span contains at least `offset` elements and
  // will terminate otherwise.
  constexpr auto split_at(size_type offset) const noexcept {
    return std::pair(first(offset), subspan(offset));
  }

  // [span.obs], span observers
  constexpr size_type size() const noexcept { return extent; }

  constexpr size_type size_bytes() const noexcept {
    return extent * sizeof(element_type);
  }

  [[nodiscard]] constexpr bool empty() const noexcept { return extent == 0; }

  // Compares two spans for equality by comparing the objects pointed to by the
  // spans. The operation is defined for spans of different types as long as the
  // types are themselves comparable.
  //
  // For primitive types, this replaces the less safe `memcmp` function, where
  // `memcmp(a.data(), b.data(), a.size()) == 0` can be written as `a == b` and
  // can no longer go outside the bounds of `b`. Otherwise, it replaced
  // std::equal or std::ranges::equal when working with spans, and when no
  // projection is needed.
  //
  // If the spans are of different sizes, they are not equal. If both spans are
  // empty, they are always equal (even though their data pointers may differ).
  //
  // # Implementation note
  // The non-template overloads allow implicit conversions to span for
  // comparison.
  friend constexpr bool operator==(span lhs, span rhs)
    requires(std::is_const_v<element_type> &&
             std::equality_comparable<const element_type>)
  {
    return std::ranges::equal(span<const element_type>(lhs),
                              span<const element_type>(rhs));
  }
  friend constexpr bool operator==(span lhs,
                                   span<const element_type, extent> rhs)
    requires(!std::is_const_v<element_type> &&
             std::equality_comparable<const element_type>)
  {
    return std::ranges::equal(span<const element_type>(lhs), rhs);
  }
  template <typename OtherElementType,
            size_t OtherExtent,
            typename OtherInternalPtrType>
    requires((OtherExtent == dynamic_extent || extent == OtherExtent) &&
             std::equality_comparable_with<const element_type,
                                           const OtherElementType>)
  friend constexpr bool operator==(
      span lhs,
      span<OtherElementType, OtherExtent, OtherInternalPtrType> rhs) {
    return std::ranges::equal(span<const element_type>(lhs),
                              span<const OtherElementType, OtherExtent>(rhs));
  }

  // Compares two spans for ordering by comparing the objects pointed to by the
  // spans. The operation is defined for spans of different types as long as the
  // types are themselves ordered via `<=>`.
  //
  // For primitive types, this replaces the less safe `memcmp` function, where
  // `memcmp(a.data(), b.data(), a.size()) < 0` can be written as `a < b` and
  // can no longer go outside the bounds of `b`.
  //
  // If both spans are empty, they are always equal (even though their data
  // pointers may differ).
  //
  // # Implementation note
  // The non-template overloads allow implicit conversions to span for
  // comparison.
  friend constexpr auto operator<=>(span lhs, span rhs)
    requires(std::is_const_v<element_type> &&
             std::three_way_comparable<const element_type>)
  {
    const auto const_lhs = span<const element_type>(lhs);
    const auto const_rhs = span<const element_type>(rhs);
    return std::lexicographical_compare_three_way(
        const_lhs.begin(), const_lhs.end(), const_rhs.begin(), const_rhs.end());
  }
  friend constexpr auto operator<=>(span lhs,
                                    span<const element_type, extent> rhs)
    requires(!std::is_const_v<element_type> &&
             std::three_way_comparable<const element_type>)
  {
    return span<const element_type>(lhs) <=> rhs;
  }
  template <typename OtherElementType,
            size_t OtherExtent,
            typename OtherInternalPtrType>
    requires((OtherExtent == dynamic_extent || extent == OtherExtent) &&
             std::three_way_comparable_with<const element_type,
                                            const OtherElementType>)
  friend constexpr auto operator<=>(
      span lhs,
      span<OtherElementType, OtherExtent, OtherInternalPtrType> rhs) {
    const auto const_lhs = span<const element_type>(lhs);
    const auto const_rhs = span<const OtherElementType, OtherExtent>(rhs);
    return std::lexicographical_compare_three_way(
        const_lhs.begin(), const_lhs.end(), const_rhs.begin(), const_rhs.end());
  }

  // [span.elem], span element access
  //
  // When `idx` is outside the span, the underlying call will `CHECK()`.
  constexpr reference operator[](size_type idx) const
    requires(extent > 0)
  {
    return *get_at(idx);
  }

  // Returns a pointer to an element in the span.
  //
  // This avoids the construction of a reference to the element, which is
  // important for cases such as in-place new, where the memory is
  // uninitialized.
  //
  // This is sugar for `span.subspan(idx, 1u).data()` which also ensures the
  // returned span has a pointer into and not past the end of the original span.
  //
  // # Checks
  // The function CHECKs that the `idx` is inside the span and will terminate
  // otherwise.
  constexpr pointer get_at(size_type idx) const
    requires(extent > 0)
  {
    CHECK_LT(idx, extent);
    // SAFETY: Since data() always points to at least `extent` elements, the
    // check above ensures `idx < extent` and is thus in range for data().
    return UNSAFE_BUFFERS(data() + idx);
  }

  // Returns a reference to the first element in the span.
  //
  // When `empty()`, the underlying call will `CHECK()`.
  constexpr reference front() const
    requires(extent > 0)
  {
    return operator[](0);
  }

  // Returns a reference to the last element in the span.
  //
  // When `empty()`, the underlying call will `CHECK()`.
  constexpr reference back() const
    requires(extent > 0)
  {
    return operator[](size() - 1);
  }

  // Returns a pointer to the first element in the span. If the span is empty
  // (`size()` is 0), the returned pointer may or may not be null, and it must
  // not be dereferenced.
  //
  // It is always valid to add `size()` to the the pointer in C++ code, though
  // it may be invalid in C code when the span is empty.
  constexpr pointer data() const noexcept { return data_; }

  // [span.iter], span iterator support
  constexpr iterator begin() const noexcept {
    // SAFETY: span provides that `data()` points to at least `extent` many
    // elements, and `extent` is non-negative. So `data() + extent` is a valid
    // pointer for the `data()` allocation.
    return UNSAFE_BUFFERS(iterator(data(), data() + extent));
  }

  constexpr iterator end() const noexcept {
    // SAFETY: span provides that `data()` points to at least `extent` many
    // elements, and `extent` is non-negative. So `data() + extent` is a valid
    // pointer for the `data()` allocation.
    return UNSAFE_BUFFERS(iterator(data(), data() + extent, data() + extent));
  }

  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }

  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

 private:
  // This field is not a raw_ptr<> since span is mostly used for stack
  // variables. Use `raw_span` instead for class fields, which does use
  // raw_ptr<> internally.
  InternalPtrType data_ = nullptr;
};

// [span], class template span
template <typename ElementType, typename InternalPtrType>
class GSL_POINTER span<ElementType, dynamic_extent, InternalPtrType> {
 public:
  using element_type = ElementType;
  using value_type = std::remove_cv_t<element_type>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = element_type*;
  using const_pointer = const element_type*;
  using reference = element_type&;
  using const_reference = const element_type&;
  using iterator = CheckedContiguousIterator<element_type>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  static constexpr size_type extent = dynamic_extent;

  constexpr span() noexcept = default;

  // Constructs a span from a contiguous iterator and a size.
  //
  // # Safety
  // The iterator must point to the first of at least `count` many elements, or
  // Undefined Behaviour can result as the span will allow access beyond the
  // valid range of the collection pointed to by the iterator.
  template <typename It>
    requires(internal::CompatibleIter<element_type, It>)
  UNSAFE_BUFFER_USAGE constexpr span(It first, StrictNumeric<size_type> count)
      // The use of to_address() here is to handle the case where the iterator
      // `first` is pointing to the container's `end()`. In that case we can
      // not use the address returned from the iterator, or dereference it
      // through the iterator's `operator*`, but we can store it. We must
      // assume in this case that `count` is 0, since the iterator does not
      // point to valid data. Future hardening of iterators may disallow
      // pulling the address from `end()`, as demonstrated by asserts() in
      // libstdc++: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93960.
      //
      // The span API dictates that the `data()` is accessible when size is 0,
      // since the pointer may be valid, so we cannot prevent storing and
      // giving out an invalid pointer here without breaking API compatibility
      // and our unit tests. Thus protecting against this can likely only be
      // successful from inside iterators themselves, where the context about
      // the pointer is known.
      //
      // We can not protect here generally against an invalid iterator/count
      // being passed in, since we have no context to determine if the
      // iterator or count are valid.
      : data_(to_address(first)), size_(count) {
    // `count != 0` implies non-null `data_`.  Consider using
    // `SpanOrSize<T>` to represent a size that may or may not be
    // accompanied by the actual data.
    DCHECK(count == 0 || !!data_);
  }

  // Constructs a span from a contiguous iterator and a size.
  //
  // # Safety
  // The first and last iterators must be for the same allocation, and `first <=
  // last` or Undefined Behaviour can result as the span will allow access
  // beyond the valid range of the collection pointed to by `first`.
  template <typename It, typename End>
    requires(internal::CompatibleIter<element_type, It> &&
             std::sized_sentinel_for<End, It> &&
             !std::is_convertible_v<End, size_t>)
  UNSAFE_BUFFER_USAGE constexpr span(It first, End last)
      // SAFETY: The caller must guarantee that the iterator and end sentinel
      // are part of the same allocation, in which case it is the number of
      // elements between the iterators and thus a valid size for the pointer to
      // the element at `first`.
      //
      // We CHECK that `last - first` did not underflow below. Normally checking
      // correctness afterward is flawed, however underflow is not UB and the
      // size is not converted to an invalid pointer (which would be UB) before
      // we CHECK for underflow.
      : UNSAFE_BUFFERS(span(first, static_cast<size_type>(last - first))) {
    // Verify `last - first` did not underflow.
    CHECK(first <= last);
  }

  template <size_t N>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(
      std::type_identity_t<element_type> (&arr LIFETIME_BOUND)[N]) noexcept
      // SAFETY: The type signature guarantees `arr` contains `N` elements.
      : UNSAFE_BUFFERS(span(arr, N)) {}

  template <typename R>
    requires(internal::CompatibleRange<element_type, R>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(R&& range LIFETIME_BOUND)
      // SAFETY: `std::ranges::size()` returns the number of elements
      // `std::ranges::data()` will point to, so accessing those elements will
      // be safe.
      : UNSAFE_BUFFERS(
            span(std::ranges::data(range), std::ranges::size(range))) {}
  template <typename R>
    requires(internal::CompatibleRange<element_type, R> &&
             std::ranges::borrowed_range<R>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(R&& range)
      // SAFETY: `std::ranges::size()` returns the number of elements
      // `std::ranges::data()` will point to, so accessing those elements will
      // be safe.
      : UNSAFE_BUFFERS(
            span(std::ranges::data(range), std::ranges::size(range))) {}

  constexpr span(std::initializer_list<value_type> il LIFETIME_BOUND)
    requires(std::is_const_v<element_type>)
      // SAFETY: `size()` is exactly the number of elements in the initializer
      // list, so accessing that many will be safe.
      : UNSAFE_BUFFERS(span(il.begin(), il.size())) {}

  constexpr span(const span& other) noexcept = default;
  template <typename OtherElementType,
            size_t OtherExtent,
            typename OtherInternalPtrType>
    requires(internal::LegalDataConversion<OtherElementType, element_type>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(
      const span<OtherElementType, OtherExtent, OtherInternalPtrType>&
          s) noexcept
      : data_(s.data()), size_(s.size()) {}
  constexpr span(span&& other) noexcept = default;

  constexpr span& operator=(const span& other) noexcept = default;
  constexpr span& operator=(span&& other) noexcept = default;

  // Bounds-checked copy from a possibly-overlapping span. The spans must be the
  // exact same size or a hard CHECK() occurs.
  //
  // This is a non-std extension that is inspired by the Rust
  // slice::copy_from_slice() method.
  //
  // If it's known the spans can not overlap, `copy_from_nonoverlapping()`
  // provides an unsafe alternative that may be more performant.
  constexpr void copy_from(span<const element_type> s)
    requires(!std::is_const_v<element_type>)
  {
    CHECK_EQ(size(), s.size());
    if (internal::AsUintptrT(begin()) <= internal::AsUintptrT(s.begin())) {
      std::ranges::copy(s, begin());
    } else {
      std::ranges::copy_backward(s, end());
    }
  }

  // Bounds-checked copy from a non-overlapping span. The spans must be the
  // exact same size or a hard CHECK() occurs.
  //
  // This is a non-std extension that is inspired by the Rust
  // slice::copy_from_slice() method.
  //
  // # Safety
  // The `other` span must not overlap with `this` or Undefined Behaviour may
  // occur.
  //
  // If the calling code is not performance sensitive, the safer copy_from()
  // method may be a simpler option.
  constexpr void copy_from_nonoverlapping(span<const element_type> s)
    requires(!std::is_const_v<element_type>)
  {
    CHECK_EQ(size(), s.size());
    DCHECK(internal::AsUintptrT(end()) <= internal::AsUintptrT(s.begin()) ||
           internal::AsUintptrT(begin()) >= internal::AsUintptrT(s.end()));
    std::ranges::copy(s, begin());
  }

  // Bounds-checked copy from a span into the front of this span. The `other`
  // span must not be larger than this span or a hard CHECK() occurs.
  //
  // Prefer copy_from() when you expect the entire span to be written to. This
  // method does not make that guarantee and may leave some bytes uninitialized
  // in the destination span, while `copy_from()` ensures the entire span is
  // written which helps prevent bugs.
  //
  // This is sugar for `span.first(other.size()).copy_from(other)` to avoid the
  // need for writing the size twice, while also preserving compile-time size
  // information.
  constexpr void copy_prefix_from(span<const element_type> s)
    requires(!std::is_const_v<element_type>)
  {
    return first(s.size()).copy_from(s);
  }

  // [span.sub], span subviews
  template <size_t Count>
  constexpr auto first() const noexcept {
    CHECK_LE(Count, size());
    // SAFETY: span provides that data() points to at least `size()` many
    // elements. `Count` is non-negative by its type and `Count <= size()` from
    // the CHECK above. So `Count` is a valid new size for `data()`.
    return UNSAFE_BUFFERS(span<element_type, Count>(data(), Count));
  }

  // Returns a span over the first `count` elements.
  //
  // # Checks
  // The function CHECKs that the span contains at least `count` elements and
  // will terminate otherwise.
  constexpr auto first(StrictNumeric<size_type> count) const noexcept {
    CHECK_LE(size_type{count}, size());
    // SAFETY: span provides that data() points to at least `size()` many
    // elements. `count` is non-negative by its type and `count <= size()` from
    // the CHECK above. So `count` is a valid new size for `data()`.
    return UNSAFE_BUFFERS(span<element_type>(data(), count));
  }

  template <size_t Count>
  constexpr auto last() const noexcept {
    CHECK_LE(Count, size());
    // SAFETY: span provides that data() points to at least `size()` many
    // elements. `Count` is non-negative by its type and `Count <= size()` from
    // the check above. So `0 <= size() - Count <= size()`, meaning
    // `size() - Count` is a valid new size for `data()` and it will point to
    // `Count` many elements.
    return UNSAFE_BUFFERS(
        span<element_type, Count>(data() + (size() - Count), Count));
  }

  // Returns a span over the last `count` elements.
  //
  // # Checks
  // The function CHECKs that the span contains at least `count` elements and
  // will terminate otherwise.
  constexpr auto last(StrictNumeric<size_type> count) const noexcept {
    CHECK_LE(size_type{count}, size());
    // SAFETY: span provides that data() points to at least `size()` many
    // elements. `count` is non-negative by its type and `count <= size()` from
    // the CHECK above. So `0 <= size() - count <= size()`, meaning
    // `size() - count` is a valid new size for `data()` and it will point to
    // `count` many elements.
    return UNSAFE_BUFFERS(
        span<element_type>(data() + (size() - size_type{count}), count));
  }

  template <size_t Offset, size_t Count = dynamic_extent>
  constexpr auto subspan() const noexcept {
    CHECK_LE(Offset, size());
    CHECK(Count == dynamic_extent || Count <= size() - Offset);
    const size_type new_extent =
        Count != dynamic_extent ? Count : size() - Offset;
    // SAFETY: span provides that data() points to at least `size()` many
    // elements.
    //
    // If Count is dynamic_extent, `new_extent` becomes `size() - Offset`. Since
    // `Offset <= size()` from the check above, then `Offset` is a valid offset
    // for data(), and `Offset + new_extent = Offset + size() - Offset = size()
    // >= Offset` is also a valid offset that is not before `Offset`. This makes
    // a span at `Offset` with size `new_extent` valid.
    //
    // Otherwise `Count <= size() - Offset` and `0 <= Offset <= size()` by the
    // check above, so `Offset <= size() - Count` and `size() - Count` can not
    // underflow. Then `Offset` is a valid offset for data() and `new_extent` is
    // `Count <= size() - Offset`, so `Offset + extent <= Offset + size() -
    // Offset = size()` which makes both `Offset` and `Offset + new_extent`
    // valid offsets for data(), and since `new_extent` is non-negative, `Offset
    // + new_extent` is not before `Offset` so `new_extent` is a valid size for
    // the span at `data() + Offset`.
    return UNSAFE_BUFFERS(
        span<element_type, Count>(data() + Offset, new_extent));
  }

  // Returns a span over the first `count` elements starting at the given
  // `offset` from the start of the span.
  //
  // # Checks
  // The function CHECKs that the span contains at least `offset + count`
  // elements, or at least `offset` elements if `count` is not specified, and
  // will terminate otherwise.
  constexpr auto subspan(size_type offset,
                         size_type count = dynamic_extent) const noexcept {
    CHECK_LE(offset, size());
    CHECK(count == dynamic_extent || count <= size() - offset)
        << " count: " << count << " offset: " << offset << " size: " << size();
    const size_type new_extent =
        count != dynamic_extent ? count : size() - offset;
    // SAFETY: span provides that data() points to at least `size()` many
    // elements.
    //
    // If count is dynamic_extent, `new_extent` becomes `size() - offset`. Since
    // `offset <= size()` from the check above, then `offset` is a valid offset
    // for data(), and `offset + new_extent = offset + size() - offset = size()
    // >= offset` is also a valid offset that is not before `offset`. This makes
    // a span at `offset` with size `new_extent` valid.
    //
    // Otherwise `count <= size() - offset` and `0 <= offset <= size()` by the
    // checks above, so `offset <= size() - count` and `size() - count` can not
    // underflow. Then `offset` is a valid offset for data() and `new_extent` is
    // `count <= size() - offset`, so `offset + new_extent <= offset + size() -
    // offset = size()` which makes both `offset` and `offset + new_extent`
    // valid offsets for data(), and since `new_extent` is non-negative, `offset
    // + new_extent` is not before `offset` so `new_extent` is a valid size for
    // the span at `data() + offset`.
    return UNSAFE_BUFFERS(span<element_type>(data() + offset, new_extent));
  }

  // An overload of `split_at` which returns a fixed-size span.
  //
  // # Checks
  // The function CHECKs that the span contains at least `Offset` elements and
  // will terminate otherwise.
  template <size_t Offset>
  constexpr auto split_at() const noexcept {
    CHECK_LE(Offset, size());
    return std::pair(first<Offset>(), subspan(Offset));
  }

  // Splits a span into two at the given `offset`, returning two spans that
  // cover the full range of the original span.
  //
  // Similar to calling subspan() with the `offset` as the length on the first
  // call, and then the `offset` as the offset in the second.
  //
  // The `split_at<Offset>()` overload allows construction of a fixed-size span
  // from a compile-time constant. If the input span is fixed-size, both output
  // spans will be. Otherwise, the first will be fixed-size and the second will
  // be dynamic-size.
  //
  // This is a non-std extension that  is inspired by the Rust slice::split_at()
  // and split_at_mut() methods.
  //
  // # Checks
  // The function CHECKs that the span contains at least `offset` elements and
  // will terminate otherwise.
  constexpr auto split_at(size_type offset) const noexcept {
    return std::pair(first(offset), subspan(offset));
  }

  // [span.obs], span observers
  constexpr size_type size() const noexcept { return size_; }

  constexpr size_type size_bytes() const noexcept {
    return size() * sizeof(element_type);
  }

  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  // Compares two spans for equality by comparing the objects pointed to by the
  // spans. The operation is defined for spans of different types as long as the
  // types are themselves comparable.
  //
  // For primitive types, this replaces the less safe `memcmp` function, where
  // `memcmp(a.data(), b.data(), a.size()) == 0` can be written as `a == b` and
  // can no longer go outside the bounds of `b`. Otherwise, it replaced
  // std::equal or std::ranges::equal when working with spans, and when no
  // projection is needed.
  //
  // If the spans are of different sizes, they are not equal. If both spans are
  // empty, they are always equal (even though their data pointers may differ).
  //
  // # Implementation note
  // The non-template overloads allow implicit conversions to span for
  // comparison.
  friend constexpr bool operator==(span lhs, span rhs)
    requires(std::is_const_v<element_type> &&
             std::equality_comparable<const element_type>)
  {
    return std::ranges::equal(span<const element_type>(lhs),
                              span<const element_type>(rhs));
  }
  friend constexpr bool operator==(span lhs,
                                   span<const element_type, extent> rhs)
    requires(!std::is_const_v<element_type> &&
             std::equality_comparable<const element_type>)
  {
    return std::ranges::equal(span<const element_type>(lhs), rhs);
  }
  template <typename OtherElementType,
            size_t OtherExtent,
            typename OtherInternalPtrType>
    requires(std::equality_comparable_with<const element_type,
                                           const OtherElementType>)
  friend constexpr bool operator==(
      span lhs,
      span<OtherElementType, OtherExtent, OtherInternalPtrType> rhs) {
    return std::ranges::equal(span<const element_type>(lhs),
                              span<const OtherElementType, OtherExtent>(rhs));
  }

  // Compares two spans for ordering by comparing the objects pointed to by the
  // spans. The operation is defined for spans of different types as long as the
  // types are themselves ordered via `<=>`.
  //
  // For primitive types, this replaces the less safe `memcmp` function, where
  // `memcmp(a.data(), b.data(), a.size()) < 0` can be written as `a < b` and
  // can no longer go outside the bounds of `b`.
  //
  // If both spans are empty, they are always equal (even though their data
  // pointers may differ).
  //
  // # Implementation note
  // The non-template overloads allow implicit conversions to span for
  // comparison.
  friend constexpr auto operator<=>(span lhs, span rhs)
    requires(std::is_const_v<element_type> &&
             std::three_way_comparable<const element_type>)
  {
    const auto const_lhs = span<const element_type>(lhs);
    const auto const_rhs = span<const element_type>(rhs);
    return std::lexicographical_compare_three_way(
        const_lhs.begin(), const_lhs.end(), const_rhs.begin(), const_rhs.end());
  }
  friend constexpr auto operator<=>(span lhs,
                                    span<const element_type, extent> rhs)
    requires(!std::is_const_v<element_type> &&
             std::three_way_comparable<const element_type>)
  {
    return span<const element_type>(lhs) <=> rhs;
  }
  template <typename OtherElementType,
            size_t OtherExtent,
            typename OtherInternalPtrType>
    requires(std::three_way_comparable_with<const element_type,
                                            const OtherElementType>)
  friend constexpr auto operator<=>(
      span lhs,
      span<OtherElementType, OtherExtent, OtherInternalPtrType> rhs) {
    const auto const_lhs = span<const element_type>(lhs);
    const auto const_rhs = span<const OtherElementType, OtherExtent>(rhs);
    return std::lexicographical_compare_three_way(
        const_lhs.begin(), const_lhs.end(), const_rhs.begin(), const_rhs.end());
  }

  // [span.elem], span element access
  //
  // When `idx` is outside the span, the underlying call will `CHECK()`.
  constexpr reference operator[](size_type idx) const noexcept {
    return *get_at(idx);
  }

  // Returns a pointer to an element in the span.
  //
  // This avoids the construction of a reference to the element, which is
  // important for cases such as in-place new, where the memory is
  // uninitialized.
  //
  // This is sugar for `span.subspan(idx, 1u).data()` which also ensures the
  // returned span has a pointer into and not past the end of the original span.
  //
  // # Checks
  // The function CHECKs that the `idx` is inside the span and will terminate
  // otherwise.
  constexpr pointer get_at(size_type idx) const noexcept {
    CHECK_LT(idx, size());
    // SAFETY: Since data() always points to at least `size()` elements, the
    // check above ensures `idx < size()` and is thus in range for data().
    return UNSAFE_BUFFERS(data() + idx);
  }

  // Returns a reference to the first element in the span.
  //
  // When `empty()`, the underlying call will `CHECK()`.
  constexpr reference front() const noexcept { return operator[](0); }

  // Returns a reference to the last element in the span.
  //
  // When `empty()`, the underlying call will `CHECK()`.
  constexpr reference back() const noexcept { return operator[](size() - 1); }

  // Returns a pointer to the first element in the span. If the span is empty
  // (`size()` is 0), the returned pointer may or may not be null, and it must
  // not be dereferenced.
  //
  // It is always valid to add `size()` to the the pointer in C++ code, though
  // it may be invalid in C code when the span is empty.
  constexpr pointer data() const noexcept { return data_; }

  // [span.iter], span iterator support
  constexpr iterator begin() const noexcept {
    // SAFETY: span provides that data() points to at least `size()` many
    // elements, and size() is non-negative. So data() + size() is a valid
    // pointer for the data() allocation.
    return UNSAFE_BUFFERS(iterator(data(), data() + size()));
  }

  constexpr iterator end() const noexcept {
    // SAFETY: span provides that data() points to at least `size()` many
    // elements, and size() is non-negative. So data() + size() is a valid
    // pointer for the data() allocation.
    return UNSAFE_BUFFERS(iterator(data(), data() + size(), data() + size()));
  }

  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }

  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

  // Convert a dynamic-extent span to a fixed-extent span. Returns a
  // `span<element_type, Extent>` iff `size() == Extent`; otherwise, returns
  // `std::nullopt`.
  template <size_t Extent>
  constexpr std::optional<span<element_type, Extent>> to_fixed_extent() const {
    return size() == Extent ? std::optional(span<element_type, Extent>(*this))
                            : std::nullopt;
  }

 private:
  // This field is not a raw_ptr<> since span is mostly used for stack
  // variables. Use `raw_span` instead for class fields, which does use
  // raw_ptr<> internally.
  InternalPtrType data_ = nullptr;
  size_t size_ = 0;
};

// [span.deduct], deduction guides.
template <typename It, typename EndOrSize>
  requires(std::contiguous_iterator<It>)
span(It, EndOrSize) -> span<std::remove_reference_t<std::iter_reference_t<It>>,
                            internal::MaybeStaticExt<EndOrSize>>;

template <typename T, size_t N>
span(T (&)[N]) -> span<T, N>;

template <typename R>
  requires(std::ranges::contiguous_range<R>)
span(R&&) -> span<std::remove_reference_t<std::ranges::range_reference_t<R>>,
                  internal::kComputedExtent<R>>;

// [span.objectrep], views of object representation
template <typename ElementType, size_t Extent, typename InternalPtrType>
constexpr auto as_bytes(span<ElementType, Extent, InternalPtrType> s) noexcept {
  return internal::as_byte_span<const uint8_t>(s);
}
template <typename ElementType, size_t Extent, typename InternalPtrType>
  requires(!std::is_const_v<ElementType>)
constexpr auto as_writable_bytes(
    span<ElementType, Extent, InternalPtrType> s) noexcept {
  return internal::as_byte_span<uint8_t>(s);
}

// as_chars() is the equivalent of as_bytes(), except that it returns a
// span of const char rather than const uint8_t. This non-std function is
// added since chrome still represents many things as char arrays which
// rightfully should be uint8_t.
template <typename ElementType, size_t Extent, typename InternalPtrType>
constexpr auto as_chars(span<ElementType, Extent, InternalPtrType> s) noexcept {
  return internal::as_byte_span<const char>(s);
}

// as_writable_chars() is the equivalent of as_writable_bytes(), except that
// it returns a span of char rather than uint8_t. This non-std function is
// added since chrome still represents many things as char arrays which
// rightfully should be uint8_t.
template <typename ElementType, size_t Extent, typename InternalPtrType>
  requires(!std::is_const_v<ElementType>)
constexpr auto as_writable_chars(span<ElementType, Extent, InternalPtrType> s) {
  return internal::as_byte_span<char>(s);
}

// as_string_view() converts a span over byte-sized primitives (holding chars or
// uint8_t) into a std::string_view, where each byte is represented as a char.
// It also accepts any type that can implicitly convert to a span, such as
// arrays.
//
// If you want to view an arbitrary span type as a string, first explicitly
// convert it to bytes via `as_bytes()`.
//
// For spans over bytes, this is sugar for:
// ```
// std::string_view(as_chars(span).begin(), as_chars(span).end())
// ```
constexpr auto as_string_view(span<const char> s) noexcept {
  return std::string_view(s.begin(), s.end());
}
constexpr auto as_string_view(span<const unsigned char> s) noexcept {
  const auto c = as_chars(s);
  return std::string_view(c.begin(), c.end());
}
constexpr auto as_string_view(span<const char16_t> s) noexcept {
  return std::u16string_view(s.begin(), s.end());
}
constexpr auto as_string_view(span<const wchar_t> s) noexcept {
  return std::wstring_view(s.begin(), s.end());
}

namespace internal {

template <typename T>
concept SpanConvertsToStringView = requires {
  { as_string_view(span<T>()) };
};

}  // namespace internal

// Stream output that prints a byte representation.
//
// (Not in `std::`; convenient for debugging.)
//
// TODO(danakj): This could move to a ToString() member method if gtest printers
// were hooked up to ToString().
template <typename ElementType, size_t Extent, typename InternalPtrType>
  requires(internal::SpanConvertsToStringView<ElementType> ||
           requires(const ElementType& t) {
             { ToString(t) };
           })
constexpr std::ostream& operator<<(
    std::ostream& l,
    span<ElementType, Extent, InternalPtrType> r) {
  l << '[';
  if constexpr (internal::SpanConvertsToStringView<ElementType>) {
    const auto sv = as_string_view(r);
    if constexpr (requires { l << sv; }) {
      using T = std::remove_cvref_t<ElementType>;
      if constexpr (std::same_as<wchar_t, T>) {
        l << 'L';
      } else if constexpr (std::same_as<char16_t, T>) {
        l << 'u';
      } else if constexpr (std::same_as<char32_t, T>) {
        l << 'U';
      }
      l << '\"' << sv << '\"';
    } else {
      // base/strings/utf_ostream_operators.h provides streaming support for
      // wchar_t/char16_t, so branching on whether streaming is available will
      // give different results depending on whether code has included that,
      // which can lead to UB due to violating the ODR. We don't want to
      // unconditionally include this header above for compile time reasons, so
      // instead force the rare caller that wants this to do it themselves.
      static_assert(
          requires { l << sv; },
          "include base/strings/utf_ostream_operators.h when streaming spans "
          "of wide chars");
    }
  } else if constexpr (Extent != 0) {
    // It would be nice to use `JoinString()` here, but making that `constexpr`
    // is more trouble than it's worth.
    if (!r.empty()) {
      l << ToString(r.front());
      for (const ElementType& e : r.template subspan<1>()) {
        l << ", " << ToString(e);
      }
    }
  }
  return l << ']';
}

// `span_from_ref` converts a reference to T into a span of length 1.  This is a
// non-std helper that is inspired by the `std::slice::from_ref()` function from
// Rust.
//
// Const references are turned into a `span<const T, 1>` while mutable
// references are turned into a `span<T, 1>`.
template <typename T>
constexpr auto span_from_ref(const T& t LIFETIME_BOUND) noexcept {
  // SAFETY: Given a valid reference to `t` the span of size 1 will be a valid
  // span that points to the `t`.
  return UNSAFE_BUFFERS(span<const T, 1>(std::addressof(t), 1u));
}
template <typename T>
constexpr auto span_from_ref(T& t LIFETIME_BOUND) noexcept {
  // SAFETY: Given a valid reference to `t` the span of size 1 will be a valid
  // span that points to the `t`.
  return UNSAFE_BUFFERS(span<T, 1>(std::addressof(t), 1u));
}

// `byte_span_from_ref` converts a reference to T into a span of uint8_t of
// length sizeof(T).  This is a non-std helper that is a sugar for
// `as_writable_bytes(span_from_ref(x))`.
//
// Const references are turned into a `span<const T, sizeof(T)>` while mutable
// references are turned into a `span<T, sizeof(T)>`.
template <typename T>
constexpr auto byte_span_from_ref(const T& t LIFETIME_BOUND) noexcept {
  return as_bytes(span_from_ref(t));
}
template <typename T>
constexpr auto byte_span_from_ref(T& t LIFETIME_BOUND) noexcept {
  return as_writable_bytes(span_from_ref(t));
}

// Converts a string literal (such as `"hello"`) to a span of `CharT` while
// omitting the terminating NUL character. These two are equivalent:
// ```
// span<char, 5u> s1 = span_from_cstring("hello");
// span<char, 5u> s2 = span(std::string_view("hello"));
// ```
//
// If you want to include the NUL terminator in the span, then use
// `span_with_nul_from_cstring()`.
//
// Internal NUL characters (ie. that are not at the end of the string) are
// always preserved.
template <typename CharT, size_t Extent>
constexpr auto span_from_cstring(const CharT (&str LIFETIME_BOUND)[Extent])
    ENABLE_IF_ATTR(str[Extent - 1u] == CharT{0},
                   "requires string literal as input") {
  return span(str).template first<Extent - 1>();
}

// Converts a string literal (such as `"hello"`) to a span of `CharT` that
// includes the terminating NUL character. These two are equivalent:
// ```
// span<char, 6u> s1 = span_with_nul_from_cstring("hello");
// auto h = std::cstring_view("hello");
// span<char, 6u> s2 =
//     UNSAFE_BUFFERS(span(h.data(), h.size() + 1u));
// ```
//
// If you do not want to include the NUL terminator, then use
// `span_from_cstring()` or use a view type (e.g. `cstring_view` or
// `std::string_view`) in place of a string literal.
//
// Internal NUL characters (ie. that are not at the end of the string) are
// always preserved.
template <typename CharT, size_t Extent>
constexpr auto span_with_nul_from_cstring(
    const CharT (&str LIFETIME_BOUND)[Extent])
    ENABLE_IF_ATTR(str[Extent - 1u] == CharT{0},
                   "requires string literal as input") {
  return span(str);
}

// Converts a string literal (such as `"hello"`) to a span of `uint8_t` while
// omitting the terminating NUL character. These two are equivalent:
// ```
// span<uint8_t, 5u> s1 = byte_span_from_cstring("hello");
// span<uint8_t, 5u> s2 = as_byte_span(std::string_view("hello"));
// ```
//
// If you want to include the NUL terminator in the span, then use
// `byte_span_with_nul_from_cstring()`.
//
// Internal NUL characters (ie. that are not at the end of the string) are
// always preserved.
template <typename CharT, size_t Extent>
constexpr auto byte_span_from_cstring(const CharT (&str LIFETIME_BOUND)[Extent])
    ENABLE_IF_ATTR(str[Extent - 1u] == CharT{0},
                   "requires string literal as input") {
  return as_bytes(span(str).template first<Extent - 1>());
}

// Converts a string literal (such as `"hello"`) to a span of `uint8_t` that
// includes the terminating NUL character. These two are equivalent:
// ```
// span<uint8_t, 6u> s1 = byte_span_with_nul_from_cstring("hello");
// auto h = cstring_view("hello");
// span<uint8_t, 6u> s2 = as_bytes(
//     UNSAFE_BUFFERS(span(h.data(), h.size() + 1u)));
// ```
//
// If you do not want to include the NUL terminator, then use
// `byte_span_from_cstring()` or use a view type (`cstring_view` or
// `std::string_view`) in place of a string literal and `as_byte_span()`.
//
// Internal NUL characters (ie. that are not at the end of the string) are
// always preserved.
template <typename CharT, size_t Extent>
constexpr auto byte_span_with_nul_from_cstring(
    const CharT (&str LIFETIME_BOUND)[Extent])
    ENABLE_IF_ATTR(str[Extent - 1u] == CharT{0},
                   "requires string literal as input") {
  return as_bytes(span(str));
}

// Convenience function for converting an object which is itself convertible
// to span into a span of bytes (i.e. span of const uint8_t). Typically used
// to convert std::string or string-objects holding chars, or std::vector
// or vector-like objects holding other scalar types, prior to passing them
// into an API that requires byte spans.
template <int&... ExplicitArgumentBarrier, typename T>
  requires(internal::SpanConstructibleFrom<const T&>)
constexpr auto as_byte_span(const T& t LIFETIME_BOUND) {
  return as_bytes(span(t));
}
template <int&... ExplicitArgumentBarrier, typename T>
  requires(internal::SpanConstructibleFrom<const T&> &&
           std::ranges::borrowed_range<T>)
constexpr auto as_byte_span(const T& t) {
  return as_bytes(span(t));
}

template <int&... ExplicitArgumentBarrier, typename ElementType, size_t Extent>
constexpr auto as_byte_span(const ElementType (&arr LIFETIME_BOUND)[Extent]) {
  return as_bytes(span<const ElementType, Extent>(arr));
}

// Convenience function for converting an object which is itself convertible
// to span into a span of mutable bytes (i.e. span of uint8_t). Typically used
// to convert std::string or string-objects holding chars, or std::vector
// or vector-like objects holding other scalar types, prior to passing them
// into an API that requires mutable byte spans.
template <int&... ExplicitArgumentBarrier, typename T>
  requires(internal::SpanConstructibleFrom<T &&> &&
           !std::is_const_v<
               typename decltype(span(std::declval<T>()))::element_type>)
// NOTE: `t` is not marked as lifetimebound because the "non-const
// `element_type`" requirement above will in turn require `T` to be a borrowed
// range.
constexpr auto as_writable_byte_span(T&& t) {
  return as_writable_bytes(span(t));
}

// This overload for arrays preserves the compile-time size N of the array in
// the span type signature span<uint8_t, N>.
template <int&... ExplicitArgumentBarrier, typename ElementType, size_t Extent>
  requires(!std::is_const_v<ElementType>)
constexpr auto as_writable_byte_span(
    ElementType (&arr LIFETIME_BOUND)[Extent]) {
  return as_writable_bytes(span<ElementType, Extent>(arr));
}

// Type-deducing helper for constructing a span.
// Deprecated: Use CTAD (i.e. use `span()` directly without template arguments).
// TODO(crbug.com/341907909): Remove.
//
// SAFETY: `it` must point to the first of a (possibly-empty) series of
// contiguous valid elements. If `end_or_size` is a size, the series must
// contain at least that many valid elements; if it is an iterator or sentinel,
// it must refer to the same allocation, and all elements in the range [it,
// end_or_size) must be valid. Otherwise, the span will allow access to invalid
// elements, resulting in UB.
template <int&... ExplicitArgumentBarrier, typename It, typename EndOrSize>
  requires(std::contiguous_iterator<It>)
UNSAFE_BUFFER_USAGE constexpr auto make_span(It it, EndOrSize end_or_size) {
  return UNSAFE_BUFFERS(span(it, end_or_size));
}

// make_span utility function that deduces both the span's value_type and extent
// from the passed in argument.
//
// Usage: auto span = make_span(...);
// Deprecated: Use CTAD (i.e. use `span()` directly without template arguments).
// TODO(crbug.com/341907909): Remove.
template <int&... ExplicitArgumentBarrier, typename R>
  requires(internal::SpanConstructibleFrom<R &&>)
constexpr auto make_span(R&& r LIFETIME_BOUND) {
  return span(std::forward<R>(r));
}
template <int&... ExplicitArgumentBarrier, typename R>
  requires(internal::SpanConstructibleFrom<R &&> &&
           std::ranges::borrowed_range<R>)
constexpr auto make_span(R&& r) {
  return span(std::forward<R>(r));
}

}  // namespace base

#endif  // BASE_CONTAINERS_SPAN_H_
