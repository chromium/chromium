// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_SPAN_H_
#define BASE_CONTAINERS_SPAN_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <concepts>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/checked_iterators.h"
#include "base/numerics/safe_conversions.h"
#include "base/template_util.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {

// [views.constants]
constexpr size_t dynamic_extent = std::numeric_limits<size_t>::max();

template <typename T,
          size_t Extent = dynamic_extent,
          typename InternalPtrType = T*>
class span;

namespace internal {

template <typename From, typename To>
concept LegalDataConversion =
    std::convertible_to<std::remove_reference_t<From> (*)[],
                        std::remove_reference_t<To> (*)[]>;

template <typename T, typename It>
concept CompatibleIter = std::contiguous_iterator<It> &&
                         LegalDataConversion<std::iter_reference_t<It>, T>;

template <typename T, typename R>
concept CompatibleRange =
    std::ranges::contiguous_range<R> && std::ranges::sized_range<R> &&
    LegalDataConversion<std::ranges::range_reference_t<R>, T> &&
    (std::ranges::borrowed_range<R> || std::is_const_v<T>);

// NOTE: Ideally we'd just use `CompatibleRange`, however this currently breaks
// code that was written prior to C++20 being standardized and assumes providing
// .data() and .size() is sufficient.
// TODO: https://crbug.com/1504998 - Remove in favor of CompatibleRange and fix
// callsites.
template <typename T, typename R>
concept LegacyCompatibleRange = requires(R& r) {
  { *std::ranges::data(r) } -> LegalDataConversion<T>;
  std::ranges::size(r);
};

template <size_t I>
using size_constant = std::integral_constant<size_t, I>;

template <typename T>
struct ExtentImpl : size_constant<dynamic_extent> {};

template <typename T, size_t N>
struct ExtentImpl<T[N]> : size_constant<N> {};

template <typename T, size_t N>
struct ExtentImpl<std::array<T, N>> : size_constant<N> {};

template <typename T, size_t N>
struct ExtentImpl<base::span<T, N>> : size_constant<N> {};

template <typename T>
using Extent = ExtentImpl<std::remove_cvref_t<T>>;

template <typename T>
inline constexpr size_t ExtentV = Extent<T>::value;

// must_not_be_dynamic_extent prevents |dynamic_extent| from being returned in a
// constexpr context.
template <size_t kExtent>
constexpr size_t must_not_be_dynamic_extent() {
  static_assert(
      kExtent != dynamic_extent,
      "EXTENT should only be used for containers with a static extent.");
  return kExtent;
}

}  // namespace internal

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
//     // std::string HexEncode(base::span<const uint8_t> data);
//     std::vector<uint8_t> data_buffer = GenerateData();
//     std::string r = HexEncode(data_buffer);
//
//  Mutable:
//     // ssize_t SafeSNPrintf(base::span<char>, const char* fmt, Args...);
//     char str_buffer[100];
//     SafeSNPrintf(str_buffer, "Pi ~= %lf", 3.14);
//
// Dynamic vs Fixed size spans
// ---------------------------
//
// Normally spans have a dynamic size, which is represented as a type as
// `span<T>`. However it is possible to encode the size of the span into the
// type as a second parameter such as `span<T, N>`. When working with fixed-size
// spans, the compiler will check the size of operations and prevent compilation
// when an invalid size is used for an operation such as assignment or
// `copy_from()`. However operations that produce a new span will make a
// dynamic-sized span by default. See below for how to prevent that.
//
// Fixed-size spans implicitly convert to a dynamic-size span, throwing away the
// compile-time size information from the type signature. So most code should
// work with dynamic-sized `span<T>` types and not worry about the existence of
// fixed-size spans.
//
// It is possible to convert from a dynamic-size to a fixed-size span (or to
// move from a fixed-size span to another fixed-size span) but it requires
// writing an the size explicitly in the code. Methods like `first` can be
// passed a size as a template argument, such as `first<N>()` to generate a
// fixed-size span. And the `make_span` function can be given a compile-time
// size in a similar way with `make_span<N>()`.
//
// Spans with "const" and pointers
// -------------------------------
//
// Const and pointers can get confusing. Here are vectors of pointers and their
// corresponding spans:
//
//   const std::vector<int*>        =>  base::span<int* const>
//   std::vector<const int*>        =>  base::span<const int*>
//   const std::vector<const int*>  =>  base::span<const int* const>
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
// - The constructors from a contiguous range apart from a C array are folded
//   into a single one, using a construct similarly to the one proposed
//   (but not standardized) in https://wg21.link/P1419.
//   The C array constructor is kept so that a span can be constructed from
//   an init list like {{1, 2, 3}}.
//   TODO: https://crbug.com/828324 - Consider adding C++26's constructor from
//   a std::initializer_list instead.
// - The conversion constructors from a contiguous range into a dynamic span
//   don't check for the range concept, but rather whether std::ranges::data
//   and std::ranges::size are well formed. This is due to legacy reasons and
//   should be fixed.
//
// Differences from [span.deduct]:
// - The deduction guides from a contiguous range are folded into a single one,
//   and treat borrowed ranges correctly.
//
// Other differences:
// - Using StrictNumeric<size_t> instead of size_t where possible.
//
// Additions beyond the C++ standard draft
// - as_chars() function.
// - as_writable_chars() function.
// - as_byte_span() function.
// - as_writable_byte_span() function.
// - copy_from() method.
// - span_from_ref() function.
// - byte_span_from_ref() function.
// - split_at() method.
//
// Furthermore, all constructors and methods are marked noexcept due to the lack
// of exceptions in Chromium.
//
// Due to the lack of class template argument deduction guides in C++14
// appropriate make_span() utility functions are provided for historic reasons.

// [span], class template span
template <typename T, size_t N, typename InternalPtrType>
class GSL_POINTER span {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = CheckedContiguousIterator<T>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  static constexpr size_t extent = N;

  // [span.cons], span constructors, copy, assignment, and destructor
  constexpr span() noexcept
    requires(N == 0)
  = default;

  template <typename It>
    requires(internal::CompatibleIter<T, It>)
  explicit constexpr span(It first, StrictNumeric<size_t> count) noexcept
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
        data_(std::to_address(first)) {
    CHECK(N == count);
  }

  template <typename It, typename End>
    requires(internal::CompatibleIter<T, It> &&
             std::sized_sentinel_for<End, It> &&
             !std::convertible_to<End, size_t>)
  explicit constexpr span(It begin, End end) noexcept
      : span(begin, static_cast<size_t>(end - begin)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(T (&arr)[N]) noexcept
      : span(std::ranges::data(arr), std::ranges::size(arr)) {}

  template <typename R, size_t X = internal::ExtentV<R>>
    requires(internal::CompatibleRange<T, R> && (X == N || X == dynamic_extent))
  // NOLINTNEXTLINE(google-explicit-constructor)
  explicit(X == dynamic_extent) constexpr span(R&& range) noexcept
      : span(std::ranges::data(range), std::ranges::size(range)) {}

  // [span.sub], span subviews
  template <size_t Count>
  constexpr span<T, Count> first() const noexcept
    requires(Count <= N)
  {
    return span<T, Count>(data(), Count);
  }

  template <size_t Count>
  constexpr span<T, Count> last() const noexcept
    requires(Count <= N)
  {
    return span<T, Count>(data() + (size() - Count), Count);
  }

  constexpr span<T> first(StrictNumeric<size_t> count) const noexcept {
    CHECK_LE(size_t{count}, size());
    return {data(), count};
  }

  constexpr span<T> last(StrictNumeric<size_t> count) const noexcept {
    CHECK_LE(size_t{count}, size());
    return {data() + (size() - size_t{count}), count};
  }

  template <size_t Offset, size_t Count = dynamic_extent>
  constexpr auto subspan() const noexcept
    requires(Offset <= N && (Count == dynamic_extent || Count <= N - Offset))
  {
    constexpr size_t kExtent = Count != dynamic_extent ? Count : N - Offset;
    return span<T, kExtent>(data() + Offset, kExtent);
  }

  constexpr span<T> subspan(size_t offset,
                            size_t count = dynamic_extent) const noexcept {
    CHECK_LE(offset, size());
    CHECK(count == dynamic_extent || count <= size() - offset);
    return {data() + offset, count != dynamic_extent ? count : size() - offset};
  }

  // Splits a span into two at the given `offset`, returning two spans that
  // cover the full range of the original span.
  //
  // Similar to calling subspan() with the `offset` as the length on the first
  // call, and then the `offset` as the offset in the second.
  //
  // The split_at<N>() overload allows construction of a fixed-size span from a
  // compile-time constant. If the input span is fixed-size, both output output
  // spans will be. Otherwise, the first will be fixed-size and the second will
  // be dynamic-size.
  //
  // This is a non-std extension that  is inspired by the Rust slice::split_at()
  // and split_at_mut() methods.
  constexpr std::pair<span<T>, span<T>> split_at(size_t offset) const noexcept {
    return {first(offset), subspan(offset)};
  }

  template <size_t Offset>
    requires(Offset <= N)
  constexpr std::pair<span<T, Offset>, span<T, N - Offset>> split_at()
      const noexcept {
    return {first<Offset>(), subspan<Offset, N - Offset>()};
  }

  // [span.obs], span observers
  constexpr size_t size() const noexcept { return N; }
  constexpr size_t size_bytes() const noexcept { return size() * sizeof(T); }
  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  // [span.elem], span element access
  constexpr T& operator[](size_t idx) const noexcept {
    CHECK_LT(idx, size());
    return data()[idx];
  }

  constexpr T& front() const noexcept
    requires(N > 0)
  {
    return data()[0];
  }

  constexpr T& back() const noexcept
    requires(N > 0)
  {
    return data()[size() - 1];
  }

  constexpr T* data() const noexcept { return data_; }

  // [span.iter], span iterator support
  constexpr iterator begin() const noexcept {
    return iterator(data(), data() + size());
  }

  constexpr iterator end() const noexcept {
    return iterator(data(), data() + size(), data() + size());
  }

  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }

  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

  // Bounds-checked copy from a non-overlapping span. The spans must be the
  // exact same size or a hard CHECK() occurs. If the two spans overlap,
  // Undefined Behaviour occurs.
  //
  // This is a non-std extension that is inspired by the Rust
  // slice::copy_from_slice() method.
  constexpr void copy_from(span<const T, N> other) {
    CHECK_EQ(size_bytes(), other.size_bytes());
    // Verify non-overlapping in developer builds.
    DCHECK(data() + size() <= other.data() ||
           data() >= other.data() + other.size());
    // When compiling with -Oz, std::ranges::copy() does not get inlined, which
    // makes copy_from() very expensive compared to memcpy for small sizes (up
    // to around 4x slower). We observe that this is because ranges::copy() uses
    // begin()/end() and span's iterators are checked iterators, not just
    // pointers. This additional complexity prevents inlining and breaks the
    // ability for the compiler to eliminate code.
    //
    // See also https://crbug.com/1396134.
    //
    // We also see std::copy() (with pointer arguments! not iterators) optimize
    // and inline better than memcpy() since memcpy() needs to rely on
    // size_bytes(), which while computable at compile time when `other` has a
    // fixed size, the optimizer stumbles on with -Oz.
    //
    // SAFETY: The copy() here does not check bounds, but we have verified that
    // `this` and `other` have the same bounds above (and are pointers of the
    // same type), so `data()` and `other.data()` both have at least
    // `other.size()` elements.
    std::copy(other.data(), other.data() + other.size(), data());
  }

  // Implicit conversion from std::span<T, N> to base::span<T, N>.
  //
  // We get other conversions for free from std::span's constructors, but it
  // does not deduce N on its range constructor.
  span(std::span<std::remove_const_t<T>, N> other)
      :  // SAFETY: std::span contains a valid data pointer and size such
         // that pointer+size remains valid.
        UNSAFE_BUFFERS(
            span(std::ranges::data(other), std::ranges::size(other))) {}
  span(std::span<T, N> other)
    requires(std::is_const_v<T>)
      :  // SAFETY: std::span contains a valid data pointer and size such
         // that pointer+size remains valid.
        UNSAFE_BUFFERS(
            span(std::ranges::data(other), std::ranges::size(other))) {}

  // Implicit conversion from base::span<T, N> to std::span<T, N>.
  //
  // We get other conversions for free from std::span's constructors, but it
  // does not deduce N on its range constructor.
  operator std::span<T, N>() const { return std::span<T, N>(*this); }
  operator std::span<const T, N>() const
    requires(!std::is_const_v<T>)
  {
    return std::span<const T, N>(*this);
  }

 private:
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #constexpr-ctor-field-initializer, #global-scope, #union
  InternalPtrType data_ = nullptr;
};

// [span], class template span
template <typename T, typename InternalPtrType>
class GSL_POINTER span<T, dynamic_extent, InternalPtrType> {
 public:
  using element_type = T;
  using value_type = std::remove_cv_t<T>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using iterator = CheckedContiguousIterator<T>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  static constexpr size_t extent = dynamic_extent;

  constexpr span() noexcept = default;

  template <typename It>
    requires(internal::CompatibleIter<T, It>)
  constexpr span(It first, StrictNumeric<size_t> count) noexcept
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
      : data_(std::to_address(first)), size_(count) {}

  template <typename It, typename End>
    requires(internal::CompatibleIter<T, It> &&
             std::sized_sentinel_for<End, It> &&
             !std::convertible_to<End, size_t>)
  constexpr span(It begin, End end) noexcept
      // Subtracting two iterators gives a ptrdiff_t, but the result should be
      // non-negative: see CHECK below.
      : span(begin, static_cast<size_t>(end - begin)) {
    CHECK(begin <= end);
  }

  template <size_t N>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(T (&arr)[N]) noexcept
      : span(std::ranges::data(arr), std::ranges::size(arr)) {}

  template <typename R>
    requires(internal::LegacyCompatibleRange<T, R>)
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr span(R&& range) noexcept
      : span(std::ranges::data(range), std::ranges::size(range)) {}

  // [span.sub], span subviews
  template <size_t Count>
  constexpr span<T, Count> first() const noexcept {
    CHECK_LE(Count, size());
    return span<T, Count>(data(), Count);
  }

  template <size_t Count>
  constexpr span<T, Count> last() const noexcept {
    CHECK_LE(Count, size());
    return span<T, Count>(data() + (size() - Count), Count);
  }

  constexpr span<T> first(StrictNumeric<size_t> count) const noexcept {
    CHECK_LE(size_t{count}, size());
    return {data(), count};
  }

  constexpr span<T> last(StrictNumeric<size_t> count) const noexcept {
    CHECK_LE(size_t{count}, size());
    return {data() + (size() - size_t{count}), count};
  }

  template <size_t Offset, size_t Count = dynamic_extent>
  constexpr span<T, Count> subspan() const noexcept {
    CHECK_LE(Offset, size());
    CHECK(Count == dynamic_extent || Count <= size() - Offset);
    return span<T, Count>(data() + Offset,
                          Count != dynamic_extent ? Count : size() - Offset);
  }

  constexpr span<T> subspan(size_t offset,
                            size_t count = dynamic_extent) const noexcept {
    CHECK_LE(offset, size());
    CHECK(count == dynamic_extent || count <= size() - offset);
    return {data() + offset, count != dynamic_extent ? count : size() - offset};
  }

  // Splits a span into two at the given `offset`, returning two spans that
  // cover the full range of the original span.
  //
  // Similar to calling subspan() with the `offset` as the length on the first
  // call, and then the `offset` as the offset in the second.
  //
  // The split_at<N>() overload allows construction of a fixed-size span from a
  // compile-time constant. If the input span is fixed-size, both output output
  // spans will be. Otherwise, the first will be fixed-size and the second will
  // be dynamic-size.
  //
  // This is a non-std extension that  is inspired by the Rust slice::split_at()
  // and split_at_mut() methods.
  constexpr std::pair<span<T>, span<T>> split_at(size_t offset) const noexcept {
    return {first(offset), subspan(offset)};
  }

  template <size_t Offset>
  constexpr std::pair<span<T, Offset>, span<T>> split_at() const noexcept {
    CHECK_LE(Offset, size());
    return {first<Offset>(), subspan(Offset)};
  }

  // [span.obs], span observers
  constexpr size_t size() const noexcept { return size_; }
  constexpr size_t size_bytes() const noexcept { return size() * sizeof(T); }
  [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

  // [span.elem], span element access
  constexpr T& operator[](size_t idx) const noexcept {
    CHECK_LT(idx, size());
    return data()[idx];
  }

  constexpr T& front() const noexcept {
    CHECK(!empty());
    return data()[0];
  }

  constexpr T& back() const noexcept {
    CHECK(!empty());
    return data()[size() - 1];
  }

  constexpr T* data() const noexcept { return data_; }

  // [span.iter], span iterator support
  constexpr iterator begin() const noexcept {
    return iterator(data(), data() + size());
  }

  constexpr iterator end() const noexcept {
    return iterator(data(), data() + size(), data() + size());
  }

  constexpr reverse_iterator rbegin() const noexcept {
    return reverse_iterator(end());
  }

  constexpr reverse_iterator rend() const noexcept {
    return reverse_iterator(begin());
  }

  // Bounds-checked copy from a non-overlapping span. The spans must be the
  // exact same size or a hard CHECK() occurs. If the two spans overlap,
  // Undefined Behaviour occurs.
  //
  // This is a non-std extension that is inspired by the Rust
  // slice::copy_from_slice() method.
  constexpr void copy_from(span<const T> other) {
    CHECK_EQ(size_bytes(), other.size_bytes());
    // Verify non-overlapping in developer builds.
    DCHECK(data() + size() <= other.data() ||
           data() >= other.data() + other.size());
    // When compiling with -Oz, std::ranges::copy() does not get inlined, which
    // makes copy_from() very expensive compared to memcpy for small sizes (up
    // to around 4x slower). We observe that this is because ranges::copy() uses
    // begin()/end() and span's iterators are checked iterators, not just
    // pointers. This additional complexity prevents inlining and breaks the
    // ability for the compiler to eliminate code.
    //
    // See also https://crbug.com/1396134.
    //
    // We also see std::copy() (with pointer arguments! not iterators) optimize
    // and inline better than memcpy() since memcpy() needs to rely on
    // size_bytes(), which while computable at compile time when `other` has a
    // fixed size, the optimizer stumbles on with -Oz.
    //
    // SAFETY: The copy() here does not check bounds, but we have verified that
    // `this` and `other` have the same bounds above (and are pointers of the
    // same type), so `data()` and `other.data()` both have at least
    // `other.size()` elements.
    std::copy(other.data(), other.data() + other.size(), data());
  }

 private:
  // This field is not a raw_ptr<> because it was filtered by the rewriter
  // for: #constexpr-ctor-field-initializer, #global-scope, #union
  InternalPtrType data_ = nullptr;
  size_t size_ = 0;
};

// [span.deduct], deduction guides.
template <typename It, typename EndOrSize>
  requires(std::contiguous_iterator<It>)
span(It, EndOrSize) -> span<std::remove_reference_t<std::iter_reference_t<It>>>;

template <
    typename R,
    typename T = std::remove_reference_t<std::ranges::range_reference_t<R>>>
  requires(std::ranges::contiguous_range<R>)
span(R&&)
    -> span<std::conditional_t<std::ranges::borrowed_range<R>, T, const T>,
            internal::ExtentV<R>>;

// [span.objectrep], views of object representation
template <typename T, size_t X>
auto as_bytes(span<T, X> s) noexcept {
  constexpr size_t N = X == dynamic_extent ? dynamic_extent : sizeof(T) * X;
  return span<const uint8_t, N>(reinterpret_cast<const uint8_t*>(s.data()),
                                s.size_bytes());
}

template <typename T, size_t X>
  requires(!std::is_const_v<T>)
auto as_writable_bytes(span<T, X> s) noexcept {
  constexpr size_t N = X == dynamic_extent ? dynamic_extent : sizeof(T) * X;
  return span<uint8_t, N>(reinterpret_cast<uint8_t*>(s.data()), s.size_bytes());
}

// as_chars() is the equivalent of as_bytes(), except that it returns a
// span of const char rather than const uint8_t. This non-std function is
// added since chrome still represents many things as char arrays which
// rightfully should be uint8_t.
template <typename T, size_t X>
auto as_chars(span<T, X> s) noexcept {
  constexpr size_t N = X == dynamic_extent ? dynamic_extent : sizeof(T) * X;
  return span<const char, N>(reinterpret_cast<const char*>(s.data()),
                             s.size_bytes());
}

// as_writable_chars() is the equivalent of as_writable_bytes(), except that
// it returns a span of char rather than uint8_t. This non-std function is
// added since chrome still represents many things as char arrays which
// rightfully should be uint8_t.
template <typename T, size_t X>
  requires(!std::is_const_v<T>)
auto as_writable_chars(span<T, X> s) noexcept {
  constexpr size_t N = X == dynamic_extent ? dynamic_extent : sizeof(T) * X;
  return span<char, N>(reinterpret_cast<char*>(s.data()), s.size_bytes());
}

// Type-deducing helpers for constructing a span.
template <int&... ExplicitArgumentBarrier, typename It>
constexpr auto make_span(It it, StrictNumeric<size_t> size) noexcept {
  using T = std::remove_reference_t<std::iter_reference_t<It>>;
  return span<T>(it, size);
}

template <int&... ExplicitArgumentBarrier,
          typename It,
          typename End,
          typename = std::enable_if_t<!std::is_convertible_v<End, size_t>>>
constexpr auto make_span(It it, End end) noexcept {
  using T = std::remove_reference_t<std::iter_reference_t<It>>;
  return span<T>(it, end);
}

// make_span utility function that deduces both the span's value_type and extent
// from the passed in argument.
//
// Usage: auto span = base::make_span(...);
template <int&... ExplicitArgumentBarrier, typename Container>
constexpr auto make_span(Container&& container) noexcept {
  using T =
      std::remove_pointer_t<decltype(std::data(std::declval<Container>()))>;
  using Extent = internal::Extent<Container>;
  return span<T, Extent::value>(std::forward<Container>(container));
}

// make_span utility functions that allow callers to explicit specify the span's
// extent, the value_type is deduced automatically. This is useful when passing
// a dynamically sized container to a method expecting static spans, when the
// container is known to have the correct size.
//
// Note: This will CHECK that N indeed matches size(container).
//
// Usage: auto static_span = base::make_span<N>(...);
template <size_t N, int&... ExplicitArgumentBarrier, typename It>
constexpr auto make_span(It it, StrictNumeric<size_t> size) noexcept {
  using T = std::remove_reference_t<std::iter_reference_t<It>>;
  return span<T, N>(it, size);
}

template <size_t N,
          int&... ExplicitArgumentBarrier,
          typename It,
          typename End,
          typename = std::enable_if_t<!std::is_convertible_v<End, size_t>>>
constexpr auto make_span(It it, End end) noexcept {
  using T = std::remove_reference_t<std::iter_reference_t<It>>;
  return span<T, N>(it, end);
}

template <size_t N, int&... ExplicitArgumentBarrier, typename Container>
constexpr auto make_span(Container&& container) noexcept {
  using T =
      std::remove_pointer_t<decltype(std::data(std::declval<Container>()))>;
  return span<T, N>(std::data(container), std::size(container));
}

// `span_from_ref` converts a reference to T into a span of length 1.  This is a
// non-std helper that is inspired by the `std::slice::from_ref()` function from
// Rust.
template <typename T>
static constexpr span<T, 1u> span_from_ref(
    T& single_object ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  return span<T, 1u>(&single_object, 1u);
}

// `byte_span_from_ref` converts a reference to T into a span of uint8_t of
// length sizeof(T).  This is a non-std helper that is a sugar for
// `as_writable_bytes(span_from_ref(x))`.
template <typename T>
static constexpr span<const uint8_t, sizeof(T)> byte_span_from_ref(
    const T& single_object ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  return as_bytes(span<const T, 1u>(&single_object, 1u));
}
template <typename T>
static constexpr span<uint8_t, sizeof(T)> byte_span_from_ref(
    T& single_object ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
  return as_writable_bytes(span<T, 1u>(&single_object, 1u));
}

// Convenience function for converting an object which is itself convertible
// to span into a span of bytes (i.e. span of const uint8_t). Typically used
// to convert std::string or string-objects holding chars, or std::vector
// or vector-like objects holding other scalar types, prior to passing them
// into an API that requires byte spans.
template <typename T>
  requires requires(const T& arg) {
    requires !std::is_array_v<std::remove_reference_t<T>>;
    make_span(arg);
  }
constexpr span<const uint8_t> as_byte_span(const T& arg) {
  return as_bytes(make_span(arg));
}

// This overload for arrays preserves the compile-time size N of the array in
// the span type signature span<uint8_t, N>.
template <typename T, size_t N>
constexpr span<const uint8_t, N * sizeof(T)> as_byte_span(T (&arr)[N]) {
  return as_bytes(make_span<N>(arr));
}

// Convenience function for converting an object which is itself convertible
// to span into a span of mutable bytes (i.e. span of uint8_t). Typically used
// to convert std::string or string-objects holding chars, or std::vector
// or vector-like objects holding other scalar types, prior to passing them
// into an API that requires mutable byte spans.
template <typename T>
  requires requires(T&& arg) {
    requires !std::is_array_v<std::remove_reference_t<T>>;
    make_span(arg);
    requires !std::is_const_v<typename decltype(make_span(arg))::element_type>;
  }
constexpr span<uint8_t> as_writable_byte_span(T&& arg) {
  return as_writable_bytes(make_span(arg));
}

// This overload for arrays preserves the compile-time size N of the array in
// the span type signature span<uint8_t, N>.
template <typename T, size_t N>
  requires(!std::is_const_v<T>)
constexpr span<uint8_t, N * sizeof(T)> as_writable_byte_span(T (&arr)[N]) {
  return as_writable_bytes(make_span<N>(arr));
}

}  // namespace base

template <typename T, size_t N, typename Ptr>
inline constexpr bool
    std::ranges::enable_borrowed_range<base::span<T, N, Ptr>> = true;

template <typename T, size_t N, typename Ptr>
inline constexpr bool std::ranges::enable_view<base::span<T, N, Ptr>> = true;

// EXTENT returns the size of any type that can be converted to a |base::span|
// with definite extent, i.e. everything that is a contiguous storage of some
// sort with static size. Specifically, this works for std::array in a constexpr
// context. Note:
//   * |std::size| should be preferred for plain arrays.
//   * In run-time contexts, functions such as |std::array::size| should be
//     preferred.
#define EXTENT(x)                                                          \
  ::base::internal::must_not_be_dynamic_extent<decltype(::base::make_span( \
      x))::extent>()

#endif  // BASE_CONTAINERS_SPAN_H_
