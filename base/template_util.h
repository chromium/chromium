// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEMPLATE_UTIL_H_
#define BASE_TEMPLATE_UTIL_H_

#include <stddef.h>

#include <iosfwd>
#include <iterator>
#include <type_traits>
#include <utility>

#include "base/compiler_specific.h"

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

namespace internal {

// The indirection with std::is_enum<T> is required, because instantiating
// std::underlying_type_t<T> when T is not an enum is UB prior to C++20.
template <typename T, bool = std::is_enum_v<T>>
struct IsScopedEnumImpl : std::false_type {};

template <typename T>
struct IsScopedEnumImpl<T, /*std::is_enum_v<T>=*/true>
    : std::negation<std::is_convertible<T, std::underlying_type_t<T>>> {};

}  // namespace internal

// Implementation of C++23's std::is_scoped_enum
//
// Reference: https://en.cppreference.com/w/cpp/types/is_scoped_enum
template <typename T>
struct is_scoped_enum : internal::IsScopedEnumImpl<T> {};

}  // namespace base

#endif  // BASE_TEMPLATE_UTIL_H_
