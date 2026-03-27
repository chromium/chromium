// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file intentionally uses the `CHECK()` macro instead of the `CHECK_op()`
// macros, as `CHECK()` generates significantly less code and is more likely to
// optimize reasonably, even in non-official release builds. Please do not
// change the `CHECK()` calls back to `CHECK_op()` calls.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_BASE_CONTAINERS_CHECKED_ITERATORS_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_BASE_CONTAINERS_CHECKED_ITERATORS_H_

#include <concepts>
#include <iterator>
#include <memory>
#include <type_traits>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/containers/span_forward_internal.h"

namespace partition_alloc::internal::base {

template <typename T>
class CheckedContiguousIterator {
 public:
  using difference_type = std::ptrdiff_t;
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator_category = std::contiguous_iterator_tag;
  using iterator_concept = std::contiguous_iterator_tag;

  // Required for converting constructor below.
  template <typename U>
  friend class CheckedContiguousIterator;

  // Required to be able to get to the underlying pointer without triggering
  // PA_BASE_CHECK failures.
  template <typename Ptr>
  friend struct std::pointer_traits;

  constexpr CheckedContiguousIterator() = default;

  // Constructs an iterator from `start` to `end`, starting at `start`.
  //
  // # Safety
  // `start` and `end` must point to a single allocation.
  //
  // # Checks
  // This function PA_BASE_CHECKs that `start <= end` and will terminate
  // otherwise.
  PA_UNSAFE_BUFFER_USAGE constexpr CheckedContiguousIterator(T* start,
                                                             const T* end)
      : CheckedContiguousIterator(AssumeValid(start, start, end)) {
    PA_BASE_CHECK(start <= end);
  }

  // Constructs an iterator from `start` to `end`, starting at `current`.
  //
  // # Safety
  // `start`, `current` and `end` must point to a single allocation.
  //
  // # Checks
  // This function PA_BASE_CHECKs that `start <= current <= end` and will
  // terminate otherwise.
  PA_UNSAFE_BUFFER_USAGE constexpr CheckedContiguousIterator(const T* start,
                                                             T* current,
                                                             const T* end)
      : CheckedContiguousIterator(AssumeValid(start, current, end)) {
    PA_BASE_CHECK(start <= current);
    PA_BASE_CHECK(current <= end);
  }

  constexpr CheckedContiguousIterator(const CheckedContiguousIterator& other) =
      default;

  // Converting constructor allowing conversions like CCI<T> to CCI<const T>,
  // but disallowing CCI<const T> to CCI<T> or CCI<Derived> to CCI<Base>, which
  // are unsafe. Furthermore, this is the same condition as used by the
  // converting constructors of std::span<T> and std::unique_ptr<T[]>.
  // See https://wg21.link/n4042 for details.
  template <typename U>
  constexpr CheckedContiguousIterator(const CheckedContiguousIterator<U>& other)
    requires(std::convertible_to<U (*)[], T (*)[]>)
      : start_(other.start_), current_(other.current_), end_(other.end_) {
    // We explicitly don't delegate to the 3-argument constructor here. Its
    // PA_BASE_CHECKs would be redundant, since we expect |other| to maintain
    // its own invariant. However, PA_BASE_DCHECKs never hurt anybody.
    // Presumably.
    PA_BASE_DCHECK(other.start_ <= other.current_);
    PA_BASE_DCHECK(other.current_ <= other.end_);
  }

  ~CheckedContiguousIterator() = default;

  constexpr CheckedContiguousIterator& operator=(
      const CheckedContiguousIterator& other) = default;

  friend constexpr bool operator==(const CheckedContiguousIterator& lhs,
                                   const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ == rhs.current_;
  }

  friend constexpr auto operator<=>(const CheckedContiguousIterator& lhs,
                                    const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ <=> rhs.current_;
  }

  constexpr CheckedContiguousIterator& operator++() {
    PA_BASE_CHECK(current_ != end_);
    // SAFETY: `current_ <= end_` is an invariant maintained internally, and the
    // PA_BASE_CHECK above ensures that we are not at the end yet, so
    // incrementing stays in bounds of the allocation.
    PA_UNSAFE_BUFFERS(++current_);
    return *this;
  }

  constexpr CheckedContiguousIterator operator++(int) {
    CheckedContiguousIterator old = *this;
    ++*this;
    return old;
  }

  constexpr CheckedContiguousIterator& operator--() {
    PA_BASE_CHECK(current_ != start_);
    // SAFETY: `current_ >= start_` is an invariant maintained internally, and
    // the PA_BASE_CHECK above ensures that we are not at the start yet, so
    // decrementing stays in bounds of the allocation.
    PA_UNSAFE_BUFFERS(--current_);
    return *this;
  }

  constexpr CheckedContiguousIterator operator--(int) {
    CheckedContiguousIterator old = *this;
    --*this;
    return old;
  }

  constexpr CheckedContiguousIterator& operator+=(difference_type rhs) {
    // NOTE: Since the max allocation size is PTRDIFF_MAX (in our compilers),
    // subtracting two pointers from the same allocation can not underflow.
    PA_BASE_CHECK(rhs <= end_ - current_);
    PA_BASE_CHECK(rhs >= start_ - current_);
    // SAFETY: `current_ <= end_` is an invariant maintained internally. The
    // checks above ensure:
    // `start_ - current_ <= rhs <= end_ - current_`.
    // Which means:
    // `start_ <= rhs + current <= end_`, so `current_` will remain in bounds of
    // the allocation after adding `rhs`.
    PA_UNSAFE_BUFFERS(current_ += rhs);
    return *this;
  }

  constexpr CheckedContiguousIterator operator+(difference_type rhs) const {
    CheckedContiguousIterator it = *this;
    it += rhs;
    return it;
  }

  constexpr friend CheckedContiguousIterator operator+(
      difference_type lhs,
      const CheckedContiguousIterator& rhs) {
    return rhs + lhs;
  }

  constexpr CheckedContiguousIterator& operator-=(difference_type rhs) {
    // NOTE: Since the max allocation size is PTRDIFF_MAX (in our compilers),
    // subtracting two pointers from the same allocation can not underflow.
    PA_BASE_CHECK(rhs >= current_ - end_);
    PA_BASE_CHECK(rhs <= current_ - start_);
    // SAFETY: `start_ <= current_` is an invariant maintained internally. The
    // checks above ensure:
    // `current_ - end_ <= rhs <= current_ - start_`.
    // Which means:
    // `end_ >= current - rhs >= start_`, so `current_` will remain in bounds
    // of the allocation after subtracting `rhs`.
    PA_UNSAFE_BUFFERS(current_ -= rhs);
    return *this;
  }

  constexpr CheckedContiguousIterator operator-(difference_type rhs) const {
    CheckedContiguousIterator it = *this;
    it -= rhs;
    return it;
  }

  constexpr friend difference_type operator-(
      const CheckedContiguousIterator& lhs,
      const CheckedContiguousIterator& rhs) {
    lhs.CheckComparable(rhs);
    return lhs.current_ - rhs.current_;
  }

  constexpr reference operator*() const {
    PA_BASE_CHECK(current_ != end_);
    return *current_;
  }

  constexpr pointer operator->() const {
    PA_BASE_CHECK(current_ != end_);
    return current_;
  }

  constexpr reference operator[](difference_type rhs) const {
    // NOTE: Since the max allocation size is PTRDIFF_MAX (in our compilers),
    // subtracting two pointers from the same allocation can not underflow.
    PA_BASE_CHECK(rhs >= start_ - current_);
    PA_BASE_CHECK(rhs < end_ - current_);
    // SAFETY: `start_ <= current_ <= end_` is an invariant maintained
    // internally. The checks above ensure:
    // `start_ - current_ <= rhs < end_ - current_`.
    // Which means:
    // `start_ <= current_ + rhs < end_`.
    // So `current_[rhs]` will be a valid dereference of a pointer in the
    // allocation (it is not the pointer toone-past-the-end).
    return PA_UNSAFE_BUFFERS(current_[rhs]);
  }

 private:
  template <typename, size_t, typename>
  friend class ::partition_alloc::internal::base::span;

  // Helper to allow containers such as `span` to elide constructor `CHECK()`'s
  // that begin <= current <= end.
  struct AssumeValid {
    const T* start;
    T* current;
    const T* end;
  };
  constexpr explicit CheckedContiguousIterator(AssumeValid pointers)
      : start_(pointers.start),
        current_(pointers.current),
        end_(pointers.end) {}

  constexpr void CheckComparable(const CheckedContiguousIterator& other) const {
    PA_BASE_CHECK(start_ == other.start_);
    PA_BASE_CHECK(end_ == other.end_);
  }

  const T* start_ = nullptr;
  T* current_ = nullptr;
  const T* end_ = nullptr;
};

template <typename T>
using CheckedContiguousConstIterator = CheckedContiguousIterator<const T>;

}  // namespace partition_alloc::internal::base

// Specialize std::pointer_traits so that we can obtain the underlying raw
// pointer without resulting in PA_BASE_CHECK failures. The important bit is the
// `to_address(pointer)` overload, which is the standard blessed way to
// customize `std::to_address(pointer)` in C++20 [1].
//
// [1] https://wg21.link/pointer.traits.optmem

template <typename T>
struct std::pointer_traits<
    partition_alloc::internal::base::CheckedContiguousIterator<T>> {
  using pointer = partition_alloc::internal::base::CheckedContiguousIterator<T>;
  using element_type = T;
  using difference_type = ptrdiff_t;

  template <typename U>
  using rebind = partition_alloc::internal::base::CheckedContiguousIterator<U>;

  static constexpr pointer pointer_to(element_type& r) noexcept {
    return pointer(&r, &r);
  }

  static constexpr element_type* to_address(pointer p) noexcept {
    return p.current_;
  }
};

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_BASE_CONTAINERS_CHECKED_ITERATORS_H_
