// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_MAP_UTIL_H_
#define BASE_CONTAINERS_MAP_UTIL_H_

#include <memory>

#include "base/types/to_address.h"

namespace base {

namespace internal {

template <typename Map>
using MappedType = typename Map::mapped_type;

}  // namespace internal

// Returns a pointer to the const value associated with the given key if it
// exists, or null otherwise.
template <typename Map, typename Key>
constexpr const internal::MappedType<Map>* FindOrNull(const Map& map,
                                                      const Key& key) {
  auto it = map.find(key);
  return it != map.end() ? &it->second : nullptr;
}

// Returns a pointer to the value associated with the given key if it exists, or
// null otherwise.
template <typename Map, typename Key>
constexpr internal::MappedType<Map>* FindOrNull(Map& map, const Key& key) {
  auto it = map.find(key);
  return it != map.end() ? &it->second : nullptr;
}

// Returns the const pointer value associated with the given key. If none is
// found, null is returned. The function is designed to be used with a map of
// keys to pointers or smart pointers.
//
// This function does not distinguish between a missing key and a key mapped
// to a null value.
template <typename Map,
          typename Key,
          typename MappedElementType =
              std::pointer_traits<internal::MappedType<Map>>::element_type>
constexpr const MappedElementType* FindPtrOrNull(const Map& map,
                                                 const Key& key) {
  auto it = map.find(key);
  return it != map.end() ? base::to_address(it->second) : nullptr;
}

// Returns the pointer value associated with the given key. If none is found,
// null is returned. The function is designed to be used with a map of keys to
// pointers or smart pointers.
//
// This function does not distinguish between a missing key and a key mapped
// to a null value.
template <typename Map,
          typename Key,
          typename MappedElementType =
              std::pointer_traits<internal::MappedType<Map>>::element_type>
constexpr MappedElementType* FindPtrOrNull(Map& map, const Key& key) {
  auto it = map.find(key);
  return it != map.end() ? base::to_address(it->second) : nullptr;
}

}  // namespace base

#endif  // BASE_CONTAINERS_MAP_UTIL_H_
