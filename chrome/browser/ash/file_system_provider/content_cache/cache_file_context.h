// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_FILE_CONTEXT_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CACHE_FILE_CONTEXT_H_

#include <functional>
#include <utility>

#include "base/files/file_path.h"
#include "base/time/time.h"

namespace ash::file_system_provider {

// Helper to explain the "-1" that is used to denote an unknown ID.
inline constexpr int kUnknownId = -1;

// Context relating to a file that is cached on disk. Used to make decisions
// around evicting files from the cache (e.g. until N bytes have been evicted)
// and guard against multiple writers to a single file.
struct CacheFileContext {
  explicit CacheFileContext(const std::string& version_tag);

  CacheFileContext(CacheFileContext&&);
  CacheFileContext& operator=(CacheFileContext&&);
  CacheFileContext(const CacheFileContext&) = delete;
  CacheFileContext& operator=(const CacheFileContext&) = delete;

  ~CacheFileContext();

  // The number of contiguous bytes that are written to this file currently. If
  // a file write is in progress, this might not represent the entire size of
  // the file on disk.
  int64_t bytes_on_disk = 0;

  // The latest access time, cryptohome is mounted with MS_NOATIME so we need to
  // keep track of this separately.
  base::Time accessed_time = base::Time::Now();

  // The version tag for this specific file.
  std::string version_tag;

  // A unique ID associated with this file that is used to write the file on
  // disk.
  int64_t id = kUnknownId;

  // True if there is an open writer to this file, multiple writers at
  // disjoint offset ranges is currently not supported.
  bool in_progress_writer = false;
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
