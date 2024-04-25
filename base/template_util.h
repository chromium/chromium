// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEMPLATE_UTIL_H_
#define BASE_TEMPLATE_UTIL_H_

#include <stddef.h>

#include <iterator>
#include <type_traits>

namespace base {

namespace internal {

// Used to detect whether the given type is an iterator.  This is normally used
// with std::enable_if to provide disambiguation for functions that take
// templatzed iterators as input.
template <typename T, typename = void>
struct is_iterator : std::false_type {};

template <typename T>
struct is_iterator<
    T,
    std::void_t<typename std::iterator_traits<T>::iterator_category>>
    : std::true_type {};

// Helper to express preferences in an overload set. If more than one overload
// are available for a given set of parameters the overload with the higher
// priority will be chosen.
template <size_t I>
struct priority_tag : priority_tag<I - 1> {};

template <>
struct priority_tag<0> {};

}  // namespace internal

}  // namespace base

#endif  // BASE_TEMPLATE_UTIL_H_
