// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CXX20_ERASE_LIST_H_
#define BASE_CONTAINERS_CXX20_ERASE_LIST_H_

#include <list>

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
size_t EraseIf(std::list<T, Allocator>& container, Predicate pred) {
  size_t old_size = container.size();
  container.remove_if(pred);
  return old_size - container.size();
}

template <class T, class Allocator, class Value>
size_t Erase(std::list<T, Allocator>& container, const Value& value) {
  // Unlike std::list::remove, this function template accepts heterogeneous
  // types and does not force a conversion to the container's value type before
  // invoking the == operator.
  return EraseIf(container, [&](const T& cur) { return cur == value; });
}

}  // namespace base

#endif  // BASE_CONTAINERS_CXX20_ERASE_LIST_H_
