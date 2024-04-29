// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_

#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_lru_cache.h"
#include "chrome/browser/ash/file_system_provider/opened_cloud_file.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"

namespace ash::file_system_provider {

using FileErrorCallback = base::OnceCallback<void(base::File::Error)>;

// Alias to explain the inner int indicates `bytes_read`.
using FileErrorOrBytesRead = base::FileErrorOr<int>;

// When the eviction process finishes, this defines the total number of items
// evicted along with the total bytes evicted.
struct EvictedItemStats {
  int64_t num_items = 0;
  int64_t bytes_evicted = 0;
};

using EvictedItemStatsCallback = base::OnceCallback<void(EvictedItemStats)>;

// The content cache for every mounted FSP. This will serve as the single point
// of orchestration between the LRU cache and the disk persistence layer.
class ContentCache {
 public:
  virtual ~ContentCache() = default;

  // Sets the maximum size of the cache. If the current number of items exceeds
  // the number set, items will be marked for removal. Call `EvictItems` to
  // remove the items from the cache.
  virtual void SetMaxCacheItems(size_t max_cache_items) = 0;

  // Start reading bytes defined by `file` from the content cache. Returns true
  // when the bytes exist in the content cache and can be read (the actual bytes
  // will be stored in the `buffer` and `callback` invoked on finish) and false
  // if the bytes don't exist.
  virtual bool StartReadBytes(
      const OpenedCloudFile& file,
      net::IOBuffer* buffer,
      int64_t offset,
      int length,
      ProvidedFileSystemInterface::ReadChunkReceivedCallback callback) = 0;

  // Start writing bytes into the cache. Returns true if the bytes are able to
  // be written, currently this means:
  //   - `file` must contain a non-empty version_tag field.
  //   - If the file is already in the cache, the `offset` must be the next
  //     contiguous chunk to be written.
  //   - No other writer must be writing to the file at the moment
  // If any conditions are not satisfied, return false.
  virtual bool StartWriteBytes(const OpenedCloudFile& file,
                               net::IOBuffer* buffer,
                               int64_t offset,
                               int length,
                               FileErrorCallback callback) = 0;

  // Load files from the content cache directory and the SQLite database. In the
  // event files have been orphaned (i.e. they are on disk with no DB entry or
  // vice versa) then prune them appropriately.
  virtual void LoadFromDisk(base::OnceClosure callback) = 0;

  // Returns the file paths of the cached files on disk, in their most recently
  // used order.
  virtual std::vector<base::FilePath> GetCachedFilePaths() = 0;

  // Mark the item with path `file_path` for eviction, if it exists. It will be
  // evicted when `EvictItems()` is called.
  virtual void MarkItemForEviction(const base::FilePath& file_path) = 0;

  // Evict items which have their `marked_for_removal` bool set to true. If an
  // eviction is already in progress, the callback will be queued to be called
  // with the current stats of the in progress eviction.
  virtual void EvictItems(EvictedItemStatsCallback callback) = 0;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_
