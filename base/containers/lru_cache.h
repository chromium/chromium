// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains a template for a Least Recently Used cache that allows
// constant-time access to items, but easy identification of the
// least-recently-used items for removal. Variations exist to support use as a
// Map (`base::LRUCache`) or Set (`base::LRUCacheSet`). These are
// implemented as aliases of `base::internal::LRUCacheBase`, defined at the
// bottom of this file. `base::HashingLRUCache` and `base::HashingLRUCacheSet`
// are defined in `base/containers/hashing_lru_cache.h`.
//
// The key object (which is identical to the value, in the Set variations) will
// be stored twice, so it should support efficient copying.

#ifndef BASE_CONTAINERS_LRU_CACHE_H_
#define BASE_CONTAINERS_LRU_CACHE_H_

#include <stddef.h>

#include <functional>
#include <list>
#include <map>
#include <utility>

#include "base/containers/lru_cache_internal.h"  // IWYU pragma: export

namespace base {
namespace internal {

template <class KeyType, class KeyCompare>
struct LRUCacheKeyIndex {
  template <class ValueType>
  using Type = std::map<KeyType, ValueType, KeyCompare>;
};

}  // namespace internal

// Implements an LRU cache of `ValueType`, where each value can be uniquely
// referenced by `KeyType`. Entries can be iterated in order of
// least-recently-used to most-recently-used by iterating from `rbegin()` to
// `rend()`, where a "use" is defined as a call to `Put(k, v)` or `Get(k)`.
template <class KeyType, class ValueType, class KeyCompare = std::less<KeyType>>
using LRUCache =
    internal::LRUCacheBase<std::pair<KeyType, ValueType>,
                           internal::GetKeyFromKVPair,
                           internal::LRUCacheKeyIndex<KeyType, KeyCompare>>;

// Implements an LRU cache of `ValueType`, where each value is unique. Entries
// can be iterated in order of least-recently-used to most-recently-used by
// iterating from `rbegin()` to `rend()`, where a "use" is defined as a call to
// `Put(v)` or `Get(v)`.
template <class ValueType, class Compare = std::less<ValueType>>
using LRUCacheSet =
    internal::LRUCacheBase<ValueType,
                           std::identity,
                           internal::LRUCacheKeyIndex<ValueType, Compare>>;

}  // namespace base

#endif  // BASE_CONTAINERS_LRU_CACHE_H_
