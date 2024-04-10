// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_IMPL_H_

#include "base/files/file_error_or.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_lru_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chrome/browser/ash/file_system_provider/opened_cloud_file.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"

namespace ash::file_system_provider {

// The content cache for every mounted FSP. This will serve as the single point
// of orchestration between the LRU cache and the disk persistence layer.
class ContentCacheImpl : public ContentCache {
 public:
  ContentCacheImpl(const base::FilePath& root_dir,
                   BoundContextDatabase context_db);

  ContentCacheImpl(const ContentCacheImpl&) = delete;
  ContentCacheImpl& operator=(const ContentCacheImpl&) = delete;

  ~ContentCacheImpl() override;

  // Creates a `ContentCache` with the concrete implementation.
  static std::unique_ptr<ContentCache> Create(const base::FilePath& root_dir,
                                              BoundContextDatabase context_db);

  bool StartReadBytes(
      const OpenedCloudFile& file,
      net::IOBuffer* buffer,
      int64_t offset,
      int length,
      ProvidedFileSystemInterface::ReadChunkReceivedCallback callback) override;

  bool StartWriteBytes(const OpenedCloudFile& file,
                       net::IOBuffer* buffer,
                       int64_t offset,
                       int length,
                       FileErrorCallback callback) override;

 private:
  void OnBytesRead(
      const base::FilePath& file_path,
      ProvidedFileSystemInterface::ReadChunkReceivedCallback callback,
      FileErrorOrBytesRead error_or_bytes_read);

  void OnBytesWritten(const base::FilePath& file_path,
                      int64_t offset,
                      int length,
                      FileErrorCallback callback,
                      base::File::Error result);

  // Helper method to retrieve the path of a file on disk from the supplied
  // `ctx`.
  const base::FilePath GetPathOnDiskFromContext(const CacheFileContext& ctx);

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath root_dir_;
  ContentLRUCache lru_cache_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  BoundContextDatabase context_db_;

  base::WeakPtrFactory<ContentCacheImpl> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_IMPL_H_
