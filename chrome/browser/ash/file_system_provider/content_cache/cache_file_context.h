// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_FILE_CONTEXT_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_FILE_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace ash::file_system_provider {

// Context relating to a file that is cached on disk. Used to make decisions
// around evicting files from the cache (e.g. until N bytes have been evicted).
struct CacheFileContext {
  int64_t bytes_on_disk;
  base::Time accessed_time;
};

using PathContextPair = std::pair<base::FilePath, CacheFileContext>;

}  // namespace ash::file_system_provider

template <>
struct std::hash<ash::file_system_provider::CacheFileContext> {
  // Enables the key to be retrieved from the std::pair that represents the
  // key-value pair in the LRU cache.
  constexpr const base::FilePath& operator()(
      const ash::file_system_provider::PathContextPair& pair) {
    return pair.first;
  }

  // Returns a hash for the std::pair to enable O(1) lookup in the
  // base::HashingLRUCache (hashes the std::string representation of the
  // base::FilePath key).
  constexpr size_t operator()(
      const ash::file_system_provider::PathContextPair& pair) const {
    return std::hash<std::string>{}(pair.first.value());
  }
};

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_FILE_CONTEXT_H_
