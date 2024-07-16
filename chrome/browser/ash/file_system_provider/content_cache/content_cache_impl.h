// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_IMPL_H_

#include "base/callback_list.h"
#include "base/files/file_error_or.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_lru_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chrome/browser/ash/file_system_provider/content_cache/local_fd.h"
#include "chrome/browser/ash/file_system_provider/opened_cloud_file.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"

namespace ash::file_system_provider {

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
  static std::unique_ptr<ContentCache> Create(const base::FilePath& root_dir,
                                              BoundContextDatabase context_db,
                                              size_t max_cache_items = 500);

  void SetMaxCacheItems(size_t max_cache_items) override;

  void ReadBytes(
      const OpenedCloudFile& file,
      scoped_refptr<net::IOBuffer> buffer,
      int64_t offset,
      int length,
      ProvidedFileSystemInterface::ReadChunkReceivedCallback callback) override;

  void WriteBytes(const OpenedCloudFile& file,
                  scoped_refptr<net::IOBuffer> buffer,
                  int64_t offset,
                  int length,
                  FileErrorCallback callback) override;

  void CloseFile(const OpenedCloudFile& file) override;

  void LoadFromDisk(base::OnceClosure callback) override;

  std::vector<base::FilePath> GetCachedFilePaths() override;

  void Notify(ProvidedFileSystemObserver::Changes& changes) override;

  void ObservedVersionTag(const base::FilePath& entry_path,
                          const std::string& version_tag) override;

  void Evict(const base::FilePath& file_path) override;

  void AddObserver(ContentCache::Observer* observer) override;
  void RemoveObserver(ContentCache::Observer* observer) override;

 private:
  void OnBytesRead(
      const base::FilePath& file_path,
      ProvidedFileSystemInterface::ReadChunkReceivedCallback callback,
      FileErrorOrBytesRead error_or_bytes_read);

  // Called when the database returns an ID that will be used as the file name
  // to write the bytes to disk.
  void OnFileIdGenerated(const OpenedCloudFile& file,
                         scoped_refptr<net::IOBuffer> buffer,
                         int64_t offset,
                         int length,
                         FileErrorCallback callback,
                         std::unique_ptr<int64_t> inserted_id,
                         bool item_add_success);

  void WriteBytesToDisk(const OpenedCloudFile& file,
                        scoped_refptr<net::IOBuffer> buffer,
                        int64_t offset,
                        int length,
                        FileErrorCallback callback);

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
                        std::map<int, int64_t> files_on_disk);

  // Invoked in the flow of `LoadFromDisk` once all the items from the database
  // have been retrieved.
  void GotItemsFromContextDatabase(base::OnceClosure callback,
                                   std::map<int, int64_t> files_on_disk,
                                   ContextDatabase::IdToItemMap items);

  // Invoked in the flow of `LoadFromDisk` once all the orphaned files (from
  // disk OR in the DB) have been removed. The `success` vector contains 2 bools
  // indicating the success of the db removal and disk removal (respectively).
  void OnStaleItemsPruned(base::OnceClosure callback,
                          std::vector<bool> prune_success);

  // Removes items individually from the disk and the lru_cache. Removes items
  // in bulk from the database.
  void RemoveItems(const std::vector<base::FilePath>& fsp_paths);

  // Removes items in bulk from the database.
  void RemoveItemsFromDatabase(std::vector<int64_t>& item_ids);

  void OnItemsRemovedFromDatabase(size_t number_of_items, bool success);

  // Removes an item with `path_on_disk` from the disk. Upon success, removes
  // item with `fsp_path` from the lru cache.
  void RemoveItemFromDisk(const base::FilePath& path_on_disk,
                          const base::FilePath& fsp_path);

  void OnItemRemovedFromDisk(const base::FilePath& fsp_path, bool success);

  // Evict the items with `file_paths`. The items are still accessible to
  // current FSP requests but inaccessible to new FSP requests. All items that
  // aren't being accessed by current FSP requests will be removed from the disk
  // and the database. Each remaining item will be removed once the last FSP
  // request for the item completes with `CloseFile()`.
  void EvictItems(const std::vector<base::FilePath>& file_paths);

  // The cache has maximum bounds on the number of items available. In the event
  // this boundary is exceeded, excess items should be evicted. There may
  // already be evicted items still in the cache (yet to be removed). The
  // remaining items to evict will be the least-recently used items.
  // TODO(b/330602540): Update the logic to also evict items when the maximum
  // size threshold has been reached.
  void EvictExcessItems();

  // Generates the absolute path on disk from the supplied `item_id`.
  const base::FilePath GetPathOnDiskFromId(int64_t item_id);

  SEQUENCE_CHECKER(sequence_checker_);

  const base::FilePath root_dir_;
  ContentLRUCache lru_cache_ GUARDED_BY_CONTEXT(sequence_checker_);

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  BoundContextDatabase context_db_;

  size_t max_cache_items_;
  // Number of evicted items that will be removed on the next removal cycle.
  size_t evicted_cache_items_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  base::ObserverList<ContentCache::Observer> observers_;
  base::WeakPtrFactory<ContentCacheImpl> weak_ptr_factory_{this};
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_CONTENT_CACHE_CONTENT_CACHE_IMPL_H_
