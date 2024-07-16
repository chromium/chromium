// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/content_cache_impl.h"

#include "base/barrier_callback.h"
#include "base/callback_list.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/file_system_provider/cloud_file_system.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_file_context.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/content_lru_cache.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chrome/browser/ash/file_system_provider/content_cache/local_fd.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "net/base/io_buffer.h"

namespace ash::file_system_provider {

namespace {

std::map<int, int64_t> GetFilesOnDisk(const base::FilePath& cache_directory) {
  std::map<int, int64_t> files_on_disk;
  if (cache_directory.empty()) {
    return files_on_disk;
  }

  base::FileEnumerator enumerator(cache_directory, /*recursive=*/false,
                                  base::FileEnumerator::FILES);
  while (!enumerator.Next().empty()) {
    base::FileEnumerator::FileInfo info = enumerator.GetInfo();
    base::FilePath path = info.GetName();
    if (base::StartsWith(path.BaseName().value(), "context.db")) {
      // The context database has multiple variants starting with context.db,
      // let's exclude those for now.
      continue;
    }
    int64_t id = -1;
    if (!base::StringToInt64(path.BaseName().value(), &id)) {
      LOG(ERROR) << "Couldn't extract ID from path";
      // TODO(b/335548274): Should we remove these files from disk, or ignore
      // them.
      continue;
    }
    files_on_disk.try_emplace(id, info.GetSize());
  }

  return files_on_disk;
}

bool RemoveFilesOnDiskByPath(std::set<base::FilePath> paths_on_disk_to_remove) {
  bool success = true;
  for (const base::FilePath& path : paths_on_disk_to_remove) {
    if (!base::DeleteFile(path)) {
      LOG(ERROR) << "Couldn't remove file on disk";
      success = false;
    }
  }
  return success;
}

}  // namespace

ContentCacheImpl::ContentCacheImpl(const base::FilePath& root_dir,
                                   BoundContextDatabase context_db,
                                   size_t max_cache_items)
    : root_dir_(root_dir),
      io_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      context_db_(std::move(context_db)),
      max_cache_items_(max_cache_items) {}

ContentCacheImpl::~ContentCacheImpl() {
  context_db_.Reset();
}

std::unique_ptr<ContentCache> ContentCacheImpl::Create(
    const base::FilePath& root_dir,
    BoundContextDatabase context_db,
    size_t max_cache_items) {
  return std::make_unique<ContentCacheImpl>(root_dir, std::move(context_db),
                                            max_cache_items);
}

void ContentCacheImpl::SetMaxCacheItems(size_t max_cache_items) {
  VLOG(1) << "Cache size changing from " << max_cache_items_ << " items to "
          << max_cache_items << " items";
  max_cache_items_ = max_cache_items;
  EvictExcessItems();
}

void ContentCacheImpl::Notify(ProvidedFileSystemObserver::Changes& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<base::FilePath> to_evict;
  for (const auto& change : changes) {
    ContentLRUCache::iterator it = lru_cache_.Peek(change.entry_path);
    if (it == lru_cache_.end()) {
      VLOG(1) << "File is not in cache";
      continue;
    }

    // Evict any deleted items or items with mismatched version tags from the
    // cache.
    if (change.change_type == storage::WatcherManager::ChangeType::DELETED) {
      VLOG(2) << "File is deleted, evict from the cache";
      to_evict.push_back(change.entry_path);
      continue;
    }

    if (!change.cloud_file_info) {
      // All cached files should have a version_tag.
      VLOG(2) << "No version_tag, evict from the cache";
      to_evict.push_back(change.entry_path);
      continue;
    }

    CacheFileContext& ctx = it->second;
    if (change.cloud_file_info->version_tag != ctx.version_tag()) {
      VLOG(2) << "File version is out of date, evict from the cache";
      to_evict.push_back(change.entry_path);
    }
  }
  EvictItems(to_evict);
}

void ContentCacheImpl::ObservedVersionTag(const base::FilePath& entry_path,
                                          const std::string& version_tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ContentLRUCache::iterator it = lru_cache_.Peek(entry_path);
  if (it == lru_cache_.end()) {
    VLOG(1) << "File is not in cache";
    return;
  }

  CacheFileContext& ctx = it->second;
  if (version_tag != ctx.version_tag()) {
    VLOG(2) << "File version is out of date, evict from the cache";
    Evict(entry_path);
  }
}

void ContentCacheImpl::Evict(const base::FilePath& file_path) {
  std::vector<base::FilePath> file_paths = {file_path};
  EvictItems(file_paths);
}

void ContentCacheImpl::RemoveItems(
    const std::vector<base::FilePath>& fsp_paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<int64_t> item_ids;

  // If possible, remove each item from the disk and the database. Do not wait
  // for each removal to complete. In the case that some removals fail, orphaned
  // items will be cleaned up upon start-up.
  for (const base::FilePath& fsp_path : fsp_paths) {
    ContentLRUCache::iterator it = lru_cache_.Peek(fsp_path);
    if (it == lru_cache_.end()) {
      VLOG(1) << "Context for '" << fsp_path << "' is not in the cache";
      continue;
    }

    CacheFileContext& ctx = it->second;
    if (ctx.HasLocalFDs()) {
      VLOG(2) << "Item '" << fsp_path
              << "' cannot be removed whilst there is an open LocalFD";
      continue;
    }

    if (ctx.path_on_disk().empty()) {
      // TODO(b/339114587): Handle this case better. Remove from the lru_cache
      // immediately and erase from the database.
      VLOG(2) << "Item does not yet have a path on disk";
      continue;
    }

    if (ctx.removal_in_progress()) {
      VLOG(2) << "Item '" << fsp_path << "' is already being removed";
      continue;
    }

    VLOG(1) << "Removing '" << fsp_path << "'";
    ctx.set_removal_in_progress(true);
    item_ids.push_back(ctx.id());

    RemoveItemFromDisk(ctx.path_on_disk(), fsp_path);
  }

  RemoveItemsFromDatabase(item_ids);
}

void ContentCacheImpl::RemoveItemsFromDatabase(std::vector<int64_t>& item_ids) {
  if (item_ids.empty()) {
    return;
  }

  const size_t number_of_items = item_ids.size();

  // Remove items from the database by their ID.
  VLOG(1) << "Attempting to remove " << number_of_items
          << " item(s) from the database";
  context_db_.AsyncCall(&ContextDatabase::RemoveItemsByIds)
      .WithArgs(std::move(item_ids))
      .Then(base::BindOnce(&ContentCacheImpl::OnItemsRemovedFromDatabase,
                           weak_ptr_factory_.GetWeakPtr(), number_of_items));
}

void ContentCacheImpl::OnItemsRemovedFromDatabase(size_t number_of_items,
                                                  bool success) {
  if (success) {
    VLOG(1) << "Removed " << number_of_items << " item(s) from the database";
  } else {
    LOG(ERROR) << "Couldn't remove " << number_of_items
               << " items from database";
  }
  // Now all the items on the disk have been removed, if the database call
  // failed the next time the cache is rebuilt (via `LoadFromDisk`) these items
  // will be attempted to be removed again.
}

void ContentCacheImpl::RemoveItemFromDisk(const base::FilePath& path_on_disk,
                                          const base::FilePath& fsp_path) {
  VLOG(1) << "Attempting to remove " << path_on_disk << " from the disk";
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::DeleteFile, path_on_disk),
      base::BindOnce(&ContentCacheImpl::OnItemRemovedFromDisk,
                     weak_ptr_factory_.GetWeakPtr(), base::OwnedRef(fsp_path)));
}

void ContentCacheImpl::OnItemRemovedFromDisk(const base::FilePath& fsp_path,
                                             bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ContentLRUCache::iterator it = lru_cache_.Peek(fsp_path);
  if (it == lru_cache_.end()) {
    VLOG(1) << "Context for '" << fsp_path << "' is not in the cache";
    return;
  }

  if (!success) {
    LOG(ERROR) << "Failed to remove item with id " << it->second.id()
               << " from disk";
    return;
  }

  VLOG(1) << "Removed item with id " << it->second.id() << " from disk";
  const int64_t bytes_on_disk = it->second.bytes_on_disk();
  lru_cache_.Erase(it);
  DCHECK_GT(evicted_cache_items_, 0u);
  evicted_cache_items_--;

  // Notify all observers.
  for (auto& observer : observers_) {
    observer.OnItemRemovedFromDisk(fsp_path, bytes_on_disk);
  }
}

void ContentCacheImpl::EvictItems(
    const std::vector<base::FilePath>& file_paths) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const base::FilePath& file_path : file_paths) {
    ContentLRUCache::iterator it = lru_cache_.Peek(file_path);
    if (it == lru_cache_.end()) {
      VLOG(2) << "Context for '" << file_path << "' is not in the cache";
      continue;
    }

    CacheFileContext& ctx = it->second;
    if (ctx.evicted()) {
      VLOG(2) << "Item '" << file_path << "' is already evicted";
      continue;
    }

    VLOG(1) << "Evicting '" << file_path << "'";
    ctx.set_evicted(true);
    evicted_cache_items_++;
    // Notify all observers.
    for (auto& observer : observers_) {
      observer.OnItemEvicted(file_path);
    }
  }
  RemoveItems(file_paths);
}

void ContentCacheImpl::EvictExcessItems() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The cache size should not include the items that are expected to be evicted
  // as these will get removed on the next removal cycle.
  size_t cache_items_without_evicted_items =
      lru_cache_.size() - evicted_cache_items_;
  if (cache_items_without_evicted_items <= max_cache_items_) {
    VLOG(2) << "No items to evict: {cache_items_without_evicted_items = "
            << cache_items_without_evicted_items
            << ", max_cache_items = " << max_cache_items_ << "}";
    return;
  }

  size_t items_to_evict = cache_items_without_evicted_items - max_cache_items_;
  VLOG(2) << items_to_evict << " items to be evicted, not including "
          << evicted_cache_items_ << " already evicted";

  // Evict items starting from the least-recently-used until the total number of
  // evicted items brings the size of the cache (without these items) to below
  // the `max_cache_items_`.
  ContentLRUCache::reverse_iterator it = lru_cache_.rbegin();
  std::vector<base::FilePath> to_evict;
  while (to_evict.size() < items_to_evict) {
    CacheFileContext& ctx = it->second;
    if (!ctx.evicted()) {
      to_evict.push_back(it->first);
    }
    it++;
  }
  EvictItems(to_evict);
}

void ContentCacheImpl::ReadBytes(
    const OpenedCloudFile& file,
    scoped_refptr<net::IOBuffer> buffer,
    int64_t offset,
    int length,
    ProvidedFileSystemInterface::ReadChunkReceivedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "ReadBytes {path = '" << file.file_path << "', version_tag = '"
          << file.version_tag << "', offset = '" << offset << "', length = '"
          << length << "'}";

  ContentLRUCache::iterator it = lru_cache_.Peek(file.file_path);
  if (it == lru_cache_.end()) {
    VLOG(1) << "Cache miss: entire file is not in cache";
    callback.Run(/*bytes_read=*/-1, /*has_more=*/false,
                 base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  CacheFileContext& ctx = it->second;

  if (offset == ctx.bytes_on_disk() && offset == file.bytes_in_cloud) {
    VLOG(1) << "Ignored request: offset is at EOF";
    callback.Run(/*bytes_read=*/0, /*has_more=*/false, base::File::FILE_OK);
    return;
  }

  // In the event the offset exceeds the known `bytes_on_disk` then we can't
  // reliably serve this data from the content cache.
  if (offset >= ctx.bytes_on_disk()) {
    VLOG(1) << "Cache miss: requested byte range {offset = '" << offset
            << "', length = '" << length
            << "'} not available {bytes_on_disk = '" << ctx.bytes_on_disk()
            << "'}";
    callback.Run(/*bytes_read=*/-1, /*has_more=*/false,
                 base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  if (!ctx.CanGetLocalFD(file)) {
    VLOG(1) << "Cache miss: not possible to read the file on disk";
    callback.Run(/*bytes_read=*/-1, /*has_more=*/false,
                 base::File::FILE_ERROR_NOT_FOUND);
    return;
  }

  // It's possible that the file on disk can't entirely fulfill the offset +
  // length bytes request. In this instance, the callback will be invoked with
  // `bytes_read` (which will be less than length) and it's up to the caller to
  // make a follow up call for the remainder (which will then be served from the
  // underlying FSP).
  VLOG(1) << "Cache hit: Range {offset = '" << offset << "', length = '"
          << length << "', bytes_on_disk = '" << ctx.bytes_on_disk()
          << "'} is available";

  LocalFD& local_fd = ctx.GetLocalFD(file, io_task_runner_);
  local_fd.ReadBytes(
      buffer, offset, length,
      base::BindOnce(&ContentCacheImpl::OnBytesRead,
                     weak_ptr_factory_.GetWeakPtr(), file.file_path, callback));
}

void ContentCacheImpl::OnBytesRead(
    const base::FilePath& file_path,
    ProvidedFileSystemInterface::ReadChunkReceivedCallback callback,
    FileErrorOrBytesRead error_or_bytes_read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::File::Error result = error_or_bytes_read.error_or(base::File::FILE_OK);
  if (result != base::File::FILE_OK) {
    VLOG(2) << "OnBytesRead result: " << base::File::ErrorToString(result);
    callback.Run(/*bytes_read=*/0, /*has_more=*/false, result);
    return;
  }

  ContentLRUCache::iterator it = lru_cache_.Get(file_path);
  DCHECK(it != lru_cache_.end());

  // Update the accessed time to now, but don't wait for the database to return,
  // just fire and forget.
  CacheFileContext& ctx = it->second;
  ctx.set_accessed_time(base::Time::Now());
  context_db_.AsyncCall(&ContextDatabase::UpdateAccessedTime)
      .WithArgs(ctx.id(), ctx.accessed_time())
      .Then(base::BindOnce([](bool success) {
        LOG_IF(ERROR, !success) << "Couldn't update access time on read";
      }));

  int bytes_read = error_or_bytes_read.value();
  VLOG(2) << "OnBytesRead {bytes_read = '" << bytes_read << "'}";
  callback.Run(bytes_read, /*has_more=*/false, base::File::FILE_OK);
}

void ContentCacheImpl::WriteBytes(const OpenedCloudFile& file,
                                  scoped_refptr<net::IOBuffer> buffer,
                                  int64_t offset,
                                  int length,
                                  FileErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ContentLRUCache::iterator it = lru_cache_.Peek(file.file_path);
  if (it != lru_cache_.end()) {
    WriteBytesToDisk(file, buffer, offset, length, std::move(callback));
    return;
  }

  // The file doesn't exist in the cache yet.
  if (file.version_tag.empty()) {
    VLOG(1) << "Empty version tag can't be written to cache";
    std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  // Add a new CacheFileContext to the lru_cache.
  VLOG(1) << "Adding '" << file.file_path << "' to the cache";
  it = lru_cache_.Put(
      PathContextPair(file.file_path, CacheFileContext(file.version_tag)));
  EvictExcessItems();

  // Add a new entry to the database then retrieve the ID and use it to create
  // a file on disk before writing the bytes to disk.
  std::unique_ptr<int64_t> inserted_id = std::make_unique<int64_t>(-1);
  context_db_.AsyncCall(&ContextDatabase::AddItem)
      .WithArgs(file.file_path, file.version_tag, it->second.accessed_time(),
                inserted_id.get())
      .Then(base::BindOnce(&ContentCacheImpl::OnFileIdGenerated,
                           weak_ptr_factory_.GetWeakPtr(), file, buffer, offset,
                           length, std::move(callback),
                           std::move(inserted_id)));
  }

  void ContentCacheImpl::WriteBytesToDisk(const OpenedCloudFile& file,
                                          scoped_refptr<net::IOBuffer> buffer,
                                          int64_t offset,
                                          int length,
                                          FileErrorCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    ContentLRUCache::iterator it = lru_cache_.Peek(file.file_path);
    if (it == lru_cache_.end()) {
      VLOG(2) << "File removed between WriteBytes and WriteBytesToDisk calls";
      std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
      return;
    }
    CacheFileContext& ctx = it->second;

    if (ctx.bytes_on_disk() != offset) {
      VLOG(1) << "Unsupported write offset supplied {bytes_on_disk = '"
              << ctx.bytes_on_disk() << "', offset = '" << offset << "'}";
      std::move(callback).Run(base::File::FILE_ERROR_INVALID_OPERATION);
      return;
    }

    if (!ctx.CanGetLocalFD(file)) {
      VLOG(1) << "Not possible to write to the file on disk";
      std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
      return;
    }

    if (ctx.has_writer()) {
      VLOG(1) << "Writer is in progress already, multi offset writers not "
                 "supported";
      std::move(callback).Run(base::File::FILE_ERROR_IN_USE);
      return;
    }
    ctx.set_has_writer(true);

    auto on_bytes_written_callback = base::BindOnce(
        &ContentCacheImpl::OnBytesWritten, weak_ptr_factory_.GetWeakPtr(),
        file.file_path, offset, length, std::move(callback));

    LocalFD& local_fd = ctx.GetLocalFD(file, io_task_runner_);
    local_fd.WriteBytes(buffer, offset, length,
                        std::move(on_bytes_written_callback));
  }

void ContentCacheImpl::CloseFile(const OpenedCloudFile& file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(1) << "Closing " << file.file_path;
  if (auto it = lru_cache_.Peek(file.file_path); it != lru_cache_.end()) {
    CacheFileContext& ctx = it->second;
    ctx.CloseLocalFD(file.request_id);
    if (ctx.evicted()) {
      // File was evicted when reading. Remove it.
      std::vector<base::FilePath> file_paths = {file.file_path};
      RemoveItems(file_paths);
    }
  }
}

void ContentCacheImpl::OnFileIdGenerated(const OpenedCloudFile& file,
                                         scoped_refptr<net::IOBuffer> buffer,
                                         int64_t offset,
                                         int length,
                                         FileErrorCallback callback,
                                         std::unique_ptr<int64_t> inserted_id,
                                         bool item_add_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!item_add_success) {
    LOG(ERROR) << "Failed to add item to the database";
    std::move(callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  ContentLRUCache::iterator it = lru_cache_.Peek(file.file_path);
  // TODO(b/339114587): Handle the case where the context gets removed during
  // the file ID generation.
  DCHECK(it != lru_cache_.end());
  DCHECK(inserted_id);
  DCHECK_GT(*inserted_id, 0);
  CacheFileContext& ctx = it->second;
  ctx.set_id(*inserted_id);
  ctx.set_path_on_disk(GetPathOnDiskFromId((*inserted_id)));

  WriteBytesToDisk(file, buffer, offset, length, std::move(callback));
}

void ContentCacheImpl::OnBytesWritten(const base::FilePath& file_path,
                                      int64_t offset,
                                      int length,
                                      FileErrorCallback callback,
                                      base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ContentLRUCache::iterator it = lru_cache_.Get(file_path);
  DCHECK(it != lru_cache_.end());

  CacheFileContext& ctx = it->second;
  if (result == base::File::FILE_OK) {
    ctx.set_bytes_on_disk(offset + length);
    ctx.set_accessed_time(base::Time::Now());

    // Keep the accessed time up to date.
    context_db_.AsyncCall(&ContextDatabase::UpdateAccessedTime)
        .WithArgs(ctx.id(), ctx.accessed_time())
        .Then(base::BindOnce([](bool success) {
          LOG_IF(ERROR, !success) << "Couldn't update access time on write";
        }));
  }
  ctx.set_has_writer(false);

  VLOG(2) << "OnBytesWritten: {offset = '" << offset << "', length = '"
          << length << "', result = '" << base::File::ErrorToString(result)
          << "'}";
  std::move(callback).Run(result);
}

const base::FilePath ContentCacheImpl::GetPathOnDiskFromId(int64_t id) {
  return root_dir_.Append(base::NumberToString(id));
}

void ContentCacheImpl::LoadFromDisk(base::OnceClosure callback) {
  // Identify all the files from the `root_dir_`.
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&GetFilesOnDisk, root_dir_),
      base::BindOnce(&ContentCacheImpl::GotFilesFromDisk,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentCacheImpl::GotFilesFromDisk(base::OnceClosure callback,
                                        std::map<int, int64_t> files_on_disk) {
  // Get all the items from the database.
  context_db_.AsyncCall(&ContextDatabase::GetAllItems)
      .Then(base::BindOnce(&ContentCacheImpl::GotItemsFromContextDatabase,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           std::move(files_on_disk)));
}

void ContentCacheImpl::GotItemsFromContextDatabase(
    base::OnceClosure callback,
    std::map<int, int64_t> files_on_disk,
    ContextDatabase::IdToItemMap items_in_db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::set<base::FilePath> paths_on_disk_to_remove;
  std::list<PathContextPair> cached_files;
  for (auto& [id, bytes_on_disk] : files_on_disk) {
    ContextDatabase::IdToItemMap::const_iterator item_it = items_in_db.find(id);
    if (item_it == items_in_db.end()) {
      // Remove files on disk that have no entry in the database.
      paths_on_disk_to_remove.emplace(
          root_dir_.Append(base::NumberToString(id)));
    } else {
      // Create contexts for each non-orphaned file on the disk using the
      // database entry.
      CacheFileContext ctx(item_it->second.version_tag, bytes_on_disk, id,
                           GetPathOnDiskFromId(id));
      ctx.set_accessed_time(item_it->second.accessed_time);
      cached_files.emplace_back(item_it->second.fsp_path, std::move(ctx));
    }
  }

  std::vector<int64_t> ids_in_db_to_remove;
  for (const auto& [id, item] : items_in_db) {
    if (!files_on_disk.contains(id)) {
      // Remove SQL entries that have no file on disk.
      ids_in_db_to_remove.emplace_back(id);
    }
  }

  cached_files.sort([](const PathContextPair& lhs, const PathContextPair& rhs) {
    // The files should be in least-recently used order, the underlying data
    // structure maintains them in order on access but not on initialization so
    // we have to ensure order now. This means the most-recently used is the
    // 0-th item, where eviction will take place on the last element.
    return lhs.second.accessed_time() > rhs.second.accessed_time();
  });

  VLOG(1) << "Initializing content cache with " << cached_files.size()
          << " items";
  lru_cache_.Init(std::move(cached_files));

  const auto barrier_callback = base::BarrierCallback<bool>(
      2, base::BindOnce(&ContentCacheImpl::OnStaleItemsPruned,
                        weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  VLOG_IF(1, !ids_in_db_to_remove.empty())
      << "Attempting to remove " << ids_in_db_to_remove.size()
      << " item(s) from the database";
  context_db_.AsyncCall(&ContextDatabase::RemoveItemsByIds)
      .WithArgs(ids_in_db_to_remove)
      .Then(barrier_callback);

  VLOG_IF(1, !paths_on_disk_to_remove.empty())
      << "Attempting to remove " << paths_on_disk_to_remove.size()
      << " path(s) from the disk";
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&RemoveFilesOnDiskByPath, paths_on_disk_to_remove),
      barrier_callback);
}

void ContentCacheImpl::OnStaleItemsPruned(base::OnceClosure callback,
                                          std::vector<bool> prune_success) {
  DCHECK_EQ(prune_success.size(), 2u);
  bool db_success = prune_success.at(0);
  bool fs_success = prune_success.at(1);

  LOG_IF(ERROR, !db_success) << "Couldn't remove all stale items from db";
  LOG_IF(ERROR, !fs_success) << "Couldn't remove all stale items from disk";

  // Failing to remove files from db/disk doesn't stop the cache being
  // successfully setup.
  std::move(callback).Run();
}

std::vector<base::FilePath> ContentCacheImpl::GetCachedFilePaths() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<base::FilePath> cached_file_paths;
  for (const auto& [file_path, cache_file_context] : lru_cache_) {
    if (!cache_file_context.evicted()) {
      cached_file_paths.push_back(file_path);
    }
  }
  return cached_file_paths;
}

void ContentCacheImpl::AddObserver(ContentCache::Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void ContentCacheImpl::RemoveObserver(ContentCache::Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

}  // namespace ash::file_system_provider
