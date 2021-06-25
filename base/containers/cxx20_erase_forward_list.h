// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CXX20_ERASE_FORWARD_LIST_H_
#define BASE_CONTAINERS_CXX20_ERASE_FORWARD_LIST_H_

#include <forward_list>
#include <iterator>

namespace base {

// Erase/EraseIf are based on C++20's uniform container erasure API:
// - https://eel.is/c++draft/libraryindex#:erase
// - https://eel.is/c++draft/libraryindex#:erase_if
// They provide a generic way to erase elements from a container.
// The functions here implement these for the standard containers until those
// functions are available in the C++ standard.
// Note: there is no std::erase for standard associative containers so we don't
// have it either.

template <class T, class Allocator, class Predicate>
size_t EraseIf(std::forward_list<T, Allocator>& container, Predicate pred) {
  // Note: std::forward_list does not have a size() API, thus we need to use the
  // O(n) std::distance work-around. However, given that EraseIf is O(n)
  // already, this should not make a big difference.
  size_t old_size = std::distance(container.begin(), container.end());
  container.remove_if(pred);
  return old_size - std::distance(container.begin(), container.end());
}

template <class T, class Allocator, class Value>
size_t Erase(std::forward_list<T, Allocator>& container, const Value& value) {
  // Unlike std::forward_list::remove, this function template accepts
  // heterogeneous types and does not force a conversion to the container's
  // value type before invoking the == operator.
  return EraseIf(container, [&](const T& cur) { return cur == value; });
}

}  // namespace base

#endif  // BASE_CONTAINERS_CXX20_ERASE_FORWARD_LIST_H_
