// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_CXX20_ERASE_MAP_H_
#define BASE_CONTAINERS_CXX20_ERASE_MAP_H_

#include <map>

#include "base/containers/cxx20_erase_internal.h"

namespace base {

// EraseIf is based on C++20's uniform container erasure API:
// - https://eel.is/c++draft/libraryindex#:erase
// - https://eel.is/c++draft/libraryindex#:erase_if
// They provide a generic way to erase elements from a container.
// The functions here implement these for the standard containers until those
// functions are available in the C++ standard.
// Note: there is no std::erase for standard associative containers so we don't
// have it either.

template <class Key, class T, class Compare, class Allocator, class Predicate>
size_t EraseIf(std::map<Key, T, Compare, Allocator>& container,
               Predicate pred) {
  return internal::IterateAndEraseIf(container, pred);
}

template <class Key, class T, class Compare, class Allocator, class Predicate>
size_t EraseIf(std::multimap<Key, T, Compare, Allocator>& container,
               Predicate pred) {
  return internal::IterateAndEraseIf(container, pred);
}

}  // namespace base

#endif  // BASE_CONTAINERS_CXX20_ERASE_MAP_H_
