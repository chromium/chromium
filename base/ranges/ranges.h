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

namespace base::ranges {

// Implementation of C++20's std::ranges::iterator_t.
//
// Reference: https://wg21.link/ranges.syn#:~:text=iterator_t
template <typename Range>
using iterator_t = decltype(std::begin(std::declval<Range&>()));

// Implementation of C++20's std::ranges::range_value_t.
//
// Reference: https://wg21.link/ranges.syn#:~:text=range_value_t
template <typename Range>
using range_value_t = std::iter_value_t<iterator_t<Range>>;

}  // namespace base::ranges

#endif  // BASE_RANGES_RANGES_H_
