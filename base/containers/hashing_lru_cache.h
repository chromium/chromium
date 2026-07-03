// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_HASHING_LRU_CACHE_H_
#define BASE_CONTAINERS_HASHING_LRU_CACHE_H_

#include <functional>
#include <utility>

#include "base/containers/lru_cache_internal.h"  // IWYU pragma: export
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/hash_container_defaults.h"

namespace base {

namespace hashing_lru_cache_internal {

template <class KeyType, class KeyHash, class KeyEqual>
struct HashingLRUCacheKeyIndex {
  template <class ValueType>
  using Type = absl::flat_hash_map<KeyType, ValueType, KeyHash, KeyEqual>;
};

}  // namespace hashing_lru_cache_internal

// Implements an LRU cache of `ValueType`, where each value can be uniquely
// referenced by `KeyType`, and `KeyType` may be hashed for O(1) insertion,
// removal, and lookup. Entries can be iterated in order of least-recently-used
// to most-recently-used by iterating from `rbegin()` to `rend()`, where a "use"
// is defined as a call to `Put(k, v)` or `Get(k)`.
template <class KeyType,
          class ValueType,
          class KeyHash = absl::DefaultHashContainerHash<KeyType>,
          class KeyEqual = absl::DefaultHashContainerEq<KeyType>>
using HashingLRUCache = internal::LRUCacheBase<
    std::pair<KeyType, ValueType>,
    internal::GetKeyFromKVPair,
    hashing_lru_cache_internal::
        HashingLRUCacheKeyIndex<KeyType, KeyHash, KeyEqual>>;

// Implements an LRU cache of `ValueType`, where each value is unique, and may
// be hashed for O(1) insertion, removal, and lookup. Entries can be iterated in
// order of least-recently-used to most-recently-used by iterating from
// `rbegin()` to `rend()`, where a "use" is defined as a call to `Put(v)` or
// `Get(v)`.
template <class ValueType,
          class Hash = absl::DefaultHashContainerHash<ValueType>,
          class Equal = absl::DefaultHashContainerEq<ValueType>>
using HashingLRUCacheSet =
    internal::LRUCacheBase<ValueType,
                           std::identity,
                           hashing_lru_cache_internal::
                               HashingLRUCacheKeyIndex<ValueType, Hash, Equal>>;

}  // namespace base

#endif  // BASE_CONTAINERS_HASHING_LRU_CACHE_H_
