// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_

#include "base/files/file_error_or.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_lru_cache.h"
#include "chrome/browser/ash/file_system_provider/opened_cloud_file.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"

namespace ash::file_system_provider {

using FileErrorCallback = base::OnceCallback<void(base::File::Error)>;

// Alias to explain the inner int indicates `bytes_read`.
using FileErrorOrBytesRead = base::FileErrorOr<int>;

// The content cache for every mounted FSP. This will serve as the single point
// of orchestration between the LRU cache and the disk persistence layer.
class ContentCache {
 public:
  virtual ~ContentCache() = default;

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
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_H_
