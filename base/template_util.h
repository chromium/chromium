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

// Uses expression SFINAE to detect whether using operator<< would work.
template <typename T, typename = void>
struct SupportsOstreamOperator : std::false_type {};
template <typename T>
struct SupportsOstreamOperator<T,
                               decltype(void(std::declval<std::ostream&>()
                                             << std::declval<T>()))>
    : std::true_type {};

template <typename T, typename = void>
struct SupportsToString : std::false_type {};
template <typename T>
struct SupportsToString<T, decltype(void(std::declval<T>().ToString()))>
    : std::true_type {};

// Used to detech whether the given type is an iterator.  This is normally used
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
template <typename T, bool = std::is_enum<T>::value>
struct IsScopedEnumImpl : std::false_type {};

template <typename T>
struct IsScopedEnumImpl<T, /*std::is_enum<T>::value=*/true>
    : std::negation<std::is_convertible<T, std::underlying_type_t<T>>> {};

}  // namespace internal

// Implementation of C++23's std::is_scoped_enum
//
// Reference: https://en.cppreference.com/w/cpp/types/is_scoped_enum
template <typename T>
struct is_scoped_enum : internal::IsScopedEnumImpl<T> {};

// Implementation of C++20's std::remove_cvref.
//
// References:
// - https://en.cppreference.com/w/cpp/types/remove_cvref
// - https://wg21.link/meta.trans.other#lib:remove_cvref
template <typename T>
struct remove_cvref {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

// Implementation of C++20's std::remove_cvref_t.
//
// References:
// - https://en.cppreference.com/w/cpp/types/remove_cvref
// - https://wg21.link/meta.type.synop#lib:remove_cvref_t
template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

// Simplified implementation of C++20's std::iter_value_t.
// As opposed to std::iter_value_t, this implementation does not restrict
// the type of `Iter` and does not consider specializations of
// `indirectly_readable_traits`.
//
// Reference: https://wg21.link/readable.traits#2
template <typename Iter>
struct IterValueImpl {
  using value_type = typename std::iterator_traits<Iter>::value_type;
};

template <typename T, bool Cond = false>
struct IterValuePointerImpl {
  // The `iterator_traits<T*>::value_type` member is not defined if T is not an
  // object in C++20.
};
template <typename T>
struct IterValuePointerImpl<T*, true> {
  using value_type = typename std::iterator_traits<T*>::value_type;
};

template <typename T>
struct IterValueImpl<T*> {
  using value_type =
      typename IterValuePointerImpl<T*, std::is_object_v<T>>::value_type;
};

template <typename Iter>
using iter_value_t = typename IterValueImpl<remove_cvref_t<Iter>>::value_type;

// Simplified implementation of C++20's std::iter_reference_t.
// As opposed to std::iter_reference_t, this implementation does not restrict
// the type of `Iter`.
//
// Reference: https://wg21.link/iterator.synopsis#:~:text=iter_reference_t
template <typename Iter>
using iter_reference_t = decltype(*std::declval<Iter&>());

// Simplified implementation of C++20's std::indirect_result_t. As opposed to
// std::indirect_result_t, this implementation does not restrict the type of
// `Func` and `Iters`.
//
// Reference: https://wg21.link/iterator.synopsis#:~:text=indirect_result_t
template <typename Func, typename... Iters>
using indirect_result_t =
    std::invoke_result_t<Func, iter_reference_t<Iters>...>;

// Simplified implementation of C++20's std::projected. As opposed to
// std::projected, this implementation does not explicitly restrict the type of
// `Iter` and `Proj`, but rather does so implicitly by requiring
// `indirect_result_t<Proj, Iter>` is a valid type. This is required for SFINAE
// friendliness.
//
// Reference: https://wg21.link/projected
template <typename Iter,
          typename Proj,
          typename IndirectResultT = indirect_result_t<Proj, Iter>>
struct projected {
  using value_type = remove_cvref_t<IndirectResultT>;

  IndirectResultT operator*() const;  // not defined
};

}  // namespace base

#endif  // BASE_TEMPLATE_UTIL_H_
