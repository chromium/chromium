// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

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
using range_value_t = std::iter_value_t<iterator_t<Range>>;

}  // namespace ranges

}  // namespace base

#endif  // BASE_RANGES_RANGES_H_
