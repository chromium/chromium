// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CONTAINS_H_
#define BASE_CONTAINERS_CONTAINS_H_

#include <type_traits>
#include <utility>

#include "base/ranges/algorithm.h"
#include "base/ranges/ranges.h"
#include "base/template_util.h"

namespace base {

namespace internal {

// Small helper to detect whether a given type has a nested `key_type` typedef.
// Used below to catch misuses of the API for associative containers.
template <typename T, typename SFINAE = void>
struct HasKeyType : std::false_type {};

template <typename T>
struct HasKeyType<T, void_t<typename T::key_type>> : std::true_type {};

// Probe whether a `contains` member function exists and return the result of
// `container.contains(value)` if this is a valid expression. This is the
// highest priority option.
template <typename Container, typename Value>
constexpr auto ContainsImpl(const Container& container,
                            const Value& value,
                            priority_tag<2>)
    -> decltype(container.contains(value)) {
  return container.contains(value);
}

// Probe whether a `find` member function exists and whether its return value
// can be compared with `container.end()`. Intended for STL style maps and sets
// that lack a `contains` member function.
template <typename Container, typename Value>
constexpr auto ContainsImpl(const Container& container,
                            const Value& value,
                            priority_tag<1>)
    -> decltype(container.find(value) != container.end()) {
  return container.find(value) != container.end();
}

// Probe whether a `find` member function exists and whether its return value
// can be compared with `Container::npos`. Intended for STL style strings that
// lack a `contains` member function.
template <typename Container, typename Value>
constexpr auto ContainsImpl(const Container& container,
                            const Value& value,
                            priority_tag<1>)
    -> decltype(container.find(value) != Container::npos) {
  return container.find(value) != Container::npos;
}

// Generic fallback option, using a linear search over `container` to find
// `value`. Has the lowest priority. This will not compile for associative
// containers, as this likely is a performance bug.
template <typename Container, typename Value>
constexpr bool ContainsImpl(const Container& container,
                            const Value& value,
                            priority_tag<0>) {
  static_assert(
      !HasKeyType<Container>::value,
      "Error: About to perform linear search on an associative container. "
      "Either use a more generic comparator (e.g. std::less<>) or, if a linear "
      "search is desired, provide an explicit projection parameter.");
  return ranges::find(container, value) != ranges::end(container);
}

}  // namespace internal

// A general purpose utility to check whether `container` contains `value`. This
// will probe whether a `contains` or `find` member function on `container`
// exists, and fall back to a generic linear search over `container`.
template <typename Container, typename Value>
constexpr bool Contains(const Container& container, const Value& value) {
  return internal::ContainsImpl(container, value, internal::priority_tag<2>());
}

// Overload that allows to provide an additional projection invocable. This
// projection will be applied to every element in `container` before comparing
// it with `value`. This will always perform a linear search.
template <typename Container, typename Value, typename Proj>
constexpr bool Contains(const Container& container,
                        const Value& value,
                        Proj proj) {
  return ranges::find(container, value, std::move(proj)) !=
         ranges::end(container);
}

}  // namespace base

#endif  // BASE_CONTAINERS_CONTAINS_H_
