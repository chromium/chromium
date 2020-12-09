// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CONTAINS_H_
#define BASE_CONTAINERS_CONTAINS_H_

#include <iterator>
#include <type_traits>

#include "base/template_util.h"

namespace base {

namespace internal {

// Utility type traits used for specializing base::Contains() below.
template <typename Container, typename Element, typename = void>
struct HasFindWithNpos : std::false_type {};

template <typename Container, typename Element>
struct HasFindWithNpos<
    Container,
    Element,
    void_t<decltype(std::declval<const Container&>().find(
                        std::declval<const Element&>()) != Container::npos)>>
    : std::true_type {};

template <typename Container, typename Element, typename = void>
struct HasFindWithEnd : std::false_type {};

template <typename Container, typename Element>
struct HasFindWithEnd<Container,
                      Element,
                      void_t<decltype(std::declval<const Container&>().find(
                                          std::declval<const Element&>()) !=
                                      std::declval<const Container&>().end())>>
    : std::true_type {};

template <typename Container, typename Element, typename = void>
struct HasContains : std::false_type {};

template <typename Container, typename Element>
struct HasContains<Container,
                   Element,
                   void_t<decltype(std::declval<const Container&>().contains(
                       std::declval<const Element&>()))>> : std::true_type {};

}  // namespace internal

// General purpose implementation to check if |container| contains |value|.
template <typename Container,
          typename Value,
          std::enable_if_t<
              !internal::HasFindWithNpos<Container, Value>::value &&
              !internal::HasFindWithEnd<Container, Value>::value &&
              !internal::HasContains<Container, Value>::value>* = nullptr>
bool Contains(const Container& container, const Value& value) {
  using std::begin;
  using std::end;
  return std::find(begin(container), end(container), value) != end(container);
}

// Specialized Contains() implementation for when |container| has a find()
// member function and a static npos member, but no contains() member function.
template <typename Container,
          typename Value,
          std::enable_if_t<internal::HasFindWithNpos<Container, Value>::value &&
                           !internal::HasContains<Container, Value>::value>* =
              nullptr>
bool Contains(const Container& container, const Value& value) {
  return container.find(value) != Container::npos;
}

// Specialized Contains() implementation for when |container| has a find()
// and end() member function, but no contains() member function.
template <typename Container,
          typename Value,
          std::enable_if_t<internal::HasFindWithEnd<Container, Value>::value &&
                           !internal::HasContains<Container, Value>::value>* =
              nullptr>
bool Contains(const Container& container, const Value& value) {
  return container.find(value) != container.end();
}

// Specialized Contains() implementation for when |container| has a contains()
// member function.
template <
    typename Container,
    typename Value,
    std::enable_if_t<internal::HasContains<Container, Value>::value>* = nullptr>
bool Contains(const Container& container, const Value& value) {
  return container.contains(value);
}

}  // namespace base

#endif  // BASE_CONTAINERS_CONTAINS_H_
