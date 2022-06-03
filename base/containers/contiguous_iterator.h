// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CONTIGUOUS_ITERATOR_H_
#define BASE_CONTAINERS_CONTIGUOUS_ITERATOR_H_

#include <array>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

#include "base/containers/checked_iterators.h"
#include "base/template_util.h"

namespace base {

namespace internal {

template <typename T>
struct PointsToObject : std::is_object<iter_value_t<T>> {};

// A pointer is a contiguous iterator.
// Reference: https://wg21.link/iterator.traits#5
template <typename T>
struct IsPointer : std::is_pointer<T> {};

template <typename T, typename StringT = std::basic_string<iter_value_t<T>>>
struct IsStringIterImpl
    : disjunction<std::is_same<T, typename StringT::const_iterator>,
                  std::is_same<T, typename StringT::iterator>> {};

// An iterator to std::basic_string is contiguous.
// Reference: https://wg21.link/basic.string.general#2
//
// Note: Requires indirection via `IsStringIterImpl` to avoid triggering a
// `static_assert(is_trivial_v<value_type>)` inside libc++'s std::basic_string.
template <typename T>
struct IsStringIter
    : conjunction<std::is_trivial<iter_value_t<T>>, IsStringIterImpl<T>> {};

// An iterator to std::array is contiguous.
// Reference: https://wg21.link/array.overview#1
template <typename T, typename ArrayT = std::array<iter_value_t<T>, 1>>
struct IsArrayIter
    : disjunction<std::is_same<T, typename ArrayT::const_iterator>,
                  std::is_same<T, typename ArrayT::iterator>> {};

// An iterator to a non-bool std::vector is contiguous.
// Reference: https://wg21.link/vector.overview#2
template <typename T, typename VectorT = std::vector<iter_value_t<T>>>
struct IsVectorIter
    : conjunction<negation<std::is_same<iter_value_t<T>, bool>>,
                  disjunction<std::is_same<T, typename VectorT::const_iterator>,
                              std::is_same<T, typename VectorT::iterator>>> {};

// The result of passing a std::valarray to std::begin is a contiguous iterator.
// Note: Since all common standard library implementations (i.e. libc++,
// stdlibc++ and MSVC's STL) just use a pointer here, we perform a similar
// optimization. The corresponding unittest still ensures that this is working
// as intended.
// Reference: https://wg21.link/valarray.range#1
template <typename T>
struct IsValueArrayIter : std::is_pointer<T> {};

// base's CheckedContiguousIterator is a contiguous iterator.
template <typename T, typename ValueT = iter_value_t<T>>
struct IsCheckedContiguousIter
    : disjunction<std::is_same<T, base::CheckedContiguousConstIterator<ValueT>>,
                  std::is_same<T, base::CheckedContiguousIterator<ValueT>>> {};

// Check that the iterator points to an actual object, and is one of the
// iterator types mentioned above.
template <typename T>
struct IsContiguousIteratorImpl
    : conjunction<PointsToObject<T>,
                  disjunction<IsPointer<T>,
                              IsStringIter<T>,
                              IsArrayIter<T>,
                              IsVectorIter<T>,
                              IsValueArrayIter<T>,
                              IsCheckedContiguousIter<T>>> {};

}  // namespace internal

// IsContiguousIterator is a type trait that determines whether a given type is
// a contiguous iterator. It is similar to C++20's contiguous_iterator concept,
// but due to a lack of the corresponding contiguous_iterator_tag relies on
// explicitly instantiating the type with iterators that are supposed to be
// contiguous iterators.
// References:
// - https://wg21.link/iterator.concept.contiguous
// - https://wg21.link/std.iterator.tags#lib:contiguous_iterator_tag
// - https://wg21.link/n4284
template <typename T>
struct IsContiguousIterator
    : internal::IsContiguousIteratorImpl<remove_cvref_t<T>> {};

}  // namespace base

#endif  // BASE_CONTAINERS_CONTIGUOUS_ITERATOR_H_
