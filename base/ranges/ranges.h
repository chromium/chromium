// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_RANGES_RANGES_H_
#define BASE_RANGES_RANGES_H_

#include <array>
#include <iterator>
#include <type_traits>
#include <utility>

#include "base/template_util.h"

namespace base {

namespace internal {

// Overload for C array.
template <typename T, size_t N>
constexpr T* begin(T (&array)[N], priority_tag<2>) {
  return array;
}

// Overload for mutable std::array. Required since std::array::begin is not
// constexpr prior to C++17. Needs to dispatch to the const overload since only
// const operator[] is constexpr in C++14.
template <typename T, size_t N>
constexpr T* begin(std::array<T, N>& array, priority_tag<2> tag) {
  return const_cast<T*>(begin(const_cast<const std::array<T, N>&>(array), tag));
}

// Overload for const std::array. Required since std::array::begin is not
// constexpr prior to C++17.
template <typename T, size_t N>
constexpr const T* begin(const std::array<T, N>& array, priority_tag<2>) {
  return N != 0 ? &array[0] : nullptr;
}

// Generic container overload.
template <typename Range>
constexpr auto begin(Range&& range, priority_tag<1>)
    -> decltype(std::forward<Range>(range).begin()) {
  return std::forward<Range>(range).begin();
}

// Overload for free begin() function.
template <typename Range>
constexpr auto begin(Range&& range, priority_tag<0>)
    -> decltype(begin(std::forward<Range>(range))) {
  return begin(std::forward<Range>(range));
}

// Overload for C array.
template <typename T, size_t N>
constexpr T* end(T (&array)[N], priority_tag<2>) {
  return array + N;
}

// Overload for mutable std::array. Required since std::array::end is not
// constexpr prior to C++17. Needs to dispatch to the const overload since only
// const operator[] is constexpr in C++14.
template <typename T, size_t N>
constexpr T* end(std::array<T, N>& array, priority_tag<2> tag) {
  return const_cast<T*>(end(const_cast<const std::array<T, N>&>(array), tag));
}

// Overload for const std::array. Required since std::array::end is not
// constexpr prior to C++17.
template <typename T, size_t N>
constexpr const T* end(const std::array<T, N>& array, priority_tag<2>) {
  return N != 0 ? (&array[0]) + N : nullptr;
}

// Generic container overload.
template <typename Range>
constexpr auto end(Range&& range, priority_tag<1>)
    -> decltype(std::forward<Range>(range).end()) {
  return std::forward<Range>(range).end();
}

// Overload for free end() function.
template <typename Range>
constexpr auto end(Range&& range, priority_tag<0>)
    -> decltype(end(std::forward<Range>(range))) {
  return end(std::forward<Range>(range));
}

}  // namespace internal

namespace ranges {

// Simplified implementation of C++20's std::ranges::begin.
// As opposed to std::ranges::begin, this implementation does does not check
// whether begin() returns an iterator and does not inhibit ADL.
//
// The trailing return type and dispatch to the internal implementation is
// necessary to be SFINAE friendly.
//
// Reference: https://wg21.link/range.access.begin
template <typename Range>
constexpr auto begin(Range&& range) noexcept
    -> decltype(internal::begin(std::forward<Range>(range),
                                internal::priority_tag<2>())) {
  return internal::begin(std::forward<Range>(range),
                         internal::priority_tag<2>());
}

// Simplified implementation of C++20's std::ranges::end.
// As opposed to std::ranges::end, this implementation does does not check
// whether end() returns an iterator and does not inhibit ADL.
//
// The trailing return type and dispatch to the internal implementation is
// necessary to be SFINAE friendly.
//
// Reference: - https://wg21.link/range.access.end
template <typename Range>
constexpr auto end(Range&& range) noexcept
    -> decltype(internal::end(std::forward<Range>(range),
                              internal::priority_tag<2>())) {
  return internal::end(std::forward<Range>(range), internal::priority_tag<2>());
}

// Implementation of C++20's std::ranges::iterator_t.
//
// Reference: https://wg21.link/ranges.syn#:~:text=iterator_t
template <typename Range>
using iterator_t = decltype(ranges::begin(std::declval<Range&>()));

// Implementation of C++20's std::ranges::range_value_t.
//
// Reference: https://wg21.link/ranges.syn#:~:text=range_value_t
template <typename Range>
using range_value_t = iter_value_t<iterator_t<Range>>;

}  // namespace ranges

}  // namespace base

#endif  // BASE_RANGES_RANGES_H_
