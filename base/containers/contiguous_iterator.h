// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CONTIGUOUS_ITERATOR_H_
#define BASE_CONTAINERS_CONTIGUOUS_ITERATOR_H_

#include <iterator>
#include <type_traits>

#include "base/containers/checked_iterators.h"

namespace base {

namespace internal {

// By default a class is not a contiguous iterator.
template <typename T>
struct IsContiguousIteratorImpl : std::false_type {};

// A pointer to an object is a contiguous iterator.
//
// Reference: https://wg21.link/iterator.traits#5
template <typename T>
struct IsContiguousIteratorImpl<T*> : std::is_object<T> {};

#if defined(_LIBCPP_VERSION)

// libc++ uses std::__wrap_iter for STL iterators. For contiguous iterators
// these wrap raw pointers.
template <typename Iter>
struct IsContiguousIteratorImpl<std::__wrap_iter<Iter>>
    : IsContiguousIteratorImpl<Iter> {};

#elif defined(__GLIBCXX__)

// libstdc++ uses __gnu_cxx::__normal_iterator for STL iterators. For contiguous
// iterators these wrap raw pointers.
template <typename Iter, typename Cont>
struct IsContiguousIteratorImpl<__gnu_cxx::__normal_iterator<Iter, Cont>>
    : IsContiguousIteratorImpl<Iter> {};

#elif defined(_MSC_VER)

// Microsoft's STL does not have a single iterator wrapper class. Explicitly
// instantiate the template for all STL containers that are contiguous.

// All std::vector<T> have contiguous iterators, except for std::vector<bool>.
// Note: MSVC's std::vector<bool> uses `std::_Vb_iterator` as its iterator type,
// thus this won't treat these iterators as contiguous.
template <typename Vec>
struct IsContiguousIteratorImpl<std::_Vector_iterator<Vec>> : std::true_type {};
template <typename Vec>
struct IsContiguousIteratorImpl<std::_Vector_const_iterator<Vec>>
    : std::true_type {};

// All std::array<T, N> have contiguous iterators.
template <typename T, size_t N>
struct IsContiguousIteratorImpl<std::_Array_iterator<T, N>> : std::true_type {};
template <typename T, size_t N>
struct IsContiguousIteratorImpl<std::_Array_const_iterator<T, N>>
    : std::true_type {};

// All std::basic_string<CharT> have contiguous iterators.
template <typename Str>
struct IsContiguousIteratorImpl<std::_String_iterator<Str>> : std::true_type {};
template <typename Str>
struct IsContiguousIteratorImpl<std::_String_const_iterator<Str>>
    : std::true_type {};

// Note: std::valarray<T> also has contiguous storage, but does not expose a
// nested iterator type. In MSVC's implementation `std::begin(valarray<T>)` is
// of type T*, thus it is already covered by the explicit instantiation for
// pointers above.
#endif

// base's CheckedContiguousIterator is a contiguous iterator as well.
template <typename T>
struct IsContiguousIteratorImpl<base::CheckedContiguousIterator<T>>
    : std::true_type {};

}  // namespace internal

// IsContiguousIterator is a type trait that determines whether a given type is
// a contiguous iterator. It is similar to C++20's contiguous_iterator concept,
// but due to a lack of the corresponding contiguous_iterator_tag relies on
// explicitly instantiating the type with iterators that are supposed to be
// contiguous iterators.
// References:
// - https://eel.is/c++draft/iterator.concept.contiguous
// - https://eel.is/c++draft/std.iterator.tags#lib:contiguous_iterator_tag
// - https://wg21.link/n4284
template <typename T>
struct IsContiguousIterator
    : internal::IsContiguousIteratorImpl<
          std::remove_cv_t<std::remove_reference_t<T>>> {};

}  // namespace base

#endif  // BASE_CONTAINERS_CONTIGUOUS_ITERATOR_H_
