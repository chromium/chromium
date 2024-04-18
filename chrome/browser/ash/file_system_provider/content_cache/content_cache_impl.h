// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_IMPL_H_

#include "base/files/file_error_or.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_lru_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chrome/browser/ash/file_system_provider/opened_cloud_file.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"

namespace ash::file_system_provider {
namespace {
constexpr size_t kMaxCacheSize = 500;
}  // namespace

// The content cache for every mounted FSP. This will serve as the single point
// of orchestration between the LRU cache and the disk persistence layer.
class ContentCacheImpl : public ContentCache {
 public:
  ContentCacheImpl(const base::FilePath& root_dir,
                   BoundContextDatabase context_db,
                   size_t max_cache_size);

  ContentCacheImpl(const ContentCacheImpl&) = delete;
  ContentCacheImpl& operator=(const ContentCacheImpl&) = delete;

  ~ContentCacheImpl() override;

  // Creates a `ContentCache` with the concrete implementation.
  static std::unique_ptr<ContentCache> Create(
      const base::FilePath& root_dir,
      BoundContextDatabase context_db,
      size_t max_cache_size = kMaxCacheSize);

  void SetMaxCacheSize(size_t max_cache_size) override;

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

  void LoadFromDisk(base::OnceClosure callback) override;

 private:
  void OnBytesRead(
      const base::FilePath& file_path,
      ProvidedFileSystemInterface::ReadChunkReceivedCallback callback,
      FileErrorOrBytesRead error_or_bytes_read);

  // Called when the database returns an ID that will be used as the file name
  // to write the bytes to disk.
  void OnFileIdGenerated(
      base::OnceCallback<base::File::Error(const base::FilePath& path)>
          write_bytes_callback,
      FileErrorCallback on_bytes_written_callback,
      int64_t* inserted_id,
      bool item_add_success);

  void OnBytesWritten(const base::FilePath& file_path,
                      int64_t offset,
                      int length,
                      FileErrorCallback callback,
                      base::File::Error result);

  // Invoked in the flow of `LoadFromDisk` once all the files have been
  // discovered in the FSP content cache mount directory. The results are keyed
  // by the id (i.e. the file name on disk) with a corresponding
  // `CacheFileContext` containing the total bytes on disk populated.
  void GotFilesFromDisk(base::OnceClosure callback,
                        std::map<int, CacheFileContext> contexts);

  // Invoked in the flow of `LoadFromDisk` once all the items from the database
  // have been retrieved.
  void GotItemsFromContextDatabase(base::OnceClosure callback,
                                   std::map<int, CacheFileContext> contexts,
                                   ContextDatabase::IdToItemMap items);

  // Invoked in the flow of `LoadFromDisk` once all the orphaned files (from
  // disk OR in the DB) have been removed. The `success` vector contains 2 bools
  // indicating the success of the db removal and disk removal (respectively).
  void OnStaleItemsPruned(base::OnceClosure callback,
                          std::vector<bool> prune_success);

  // Generates the absolute path on disk from the supplied `item_id`.
  const base::FilePath GetPathOnDiskFromContext(int64_t item_id);

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath root_dir_;
  ContentLRUCache lru_cache_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  BoundContextDatabase context_db_;
  size_t max_cache_size_;

  base::WeakPtrFactory<ContentCacheImpl> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(
      FileSystemProviderContentCacheImplTest,
      FilesOnDiskAndInDbAreInitializedInTheDatabaseAccessedTimeOrder);
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_IMPL_H_
