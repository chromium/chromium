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
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "net/base/io_buffer.h"

namespace ash::file_system_provider {

namespace {

base::File::Error WriteBytesBlocking(scoped_refptr<net::IOBuffer> buffer,
                                     int64_t offset,
                                     int length,
                                     const base::FilePath& path) {
  VLOG(1) << "WriteBytesBlocking: {path = '" << path.value() << "', offset = '"
          << offset << "', length = '" << length << "'}";

  // TODO(b/331275523): We should cache this writer fd to avoid opening a new
  // one on every write.
  base::File file(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_WRITE);
  if (file.Write(offset, buffer->data(), length) != length) {
    return base::File::FILE_ERROR_FAILED;
  }

  return base::File::FILE_OK;
}

FileErrorOrBytesRead ReadBytesBlocking(const base::FilePath& path,
                                       scoped_refptr<net::IOBuffer> buffer,
                                       int64_t offset,
                                       int length) {
  // TODO(b/331275058): We should probably cache these readers to avoid opening
  // an FD for every read that we make.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  int bytes_read = file.Read(offset, buffer->data(), length);
  if (bytes_read < 0) {
    return base::unexpected(base::File::FILE_ERROR_FAILED);
  }

  VLOG(1) << "ReadBytesBlocking: {bytes_read = '" << bytes_read
          << "', file.GetLength = '" << file.GetLength() << "', offset = '"
          << offset << "', length = '" << length << "'}";
  return bytes_read;
}

std::map<int, CacheFileContext> GetIdFromCachedFiles(
    const base::FilePath& cache_directory) {
  std::map<int, CacheFileContext> contexts;
  if (cache_directory.empty()) {
    return contexts;
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
    contexts.try_emplace(id, info.GetSize(), id);
  }

  return contexts;
}

bool RemoveAllFilesOnDiskById(
    std::set<base::FilePath> paths_on_disk_to_remove) {
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
  max_cache_items_ = max_cache_items;
  MarkItemsForEviction();
}

void ContentCacheImpl::EvictItems(EvictedItemStatsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool eviction_in_progress = !on_evicted_callbacks_.empty();
  on_evicted_callbacks_.AddUnsafe(std::move(callback));
  if (eviction_in_progress) {
    return;
  }

  EvictedItemStats evicted_items;
  if (cache_items_to_remove_ == 0) {
    on_evicted_callbacks_.Notify(evicted_items);
    return;
  }

  ContentLRUCache::reverse_iterator it = lru_cache_.rbegin();
  std::vector<int64_t> item_ids;
  EvictItemsMarkedForRemoval(it, item_ids, evicted_items);
}

void ContentCacheImpl::EvictItemsMarkedForRemoval(
    ContentLRUCache::reverse_iterator it,
    std::vector<int64_t>& item_ids,
    EvictedItemStats& evicted_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Items in the `lru_cache_` are ordered by least-recently used, so begin at
  // the last item and enumerate (in reverse order) through the list identifying
  // all the items that are marked for removal and delete them from the disk.
  while (it != lru_cache_.rend()) {
    const CacheFileContext& ctx = it->second;
    if (ctx.marked_for_removal) {
      io_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&base::DeleteFile, GetPathOnDiskFromId(ctx.id)),
          base::BindOnce(&ContentCacheImpl::OnItemRemovedFromDisk,
                         weak_ptr_factory_.GetWeakPtr(), it,
                         base::OwnedRef(item_ids),
                         base::OwnedRef(evicted_items)));
      return;
    }
    it++;
  }

  // After all the items have been removed from the disk, a single call can be
  // made to the database to remove the items by their ID. This avoids making
  // individual calls for every item that is removed from disk and just lumps
  // them into a single call.
  context_db_.AsyncCall(&ContextDatabase::RemoveItemsByIds)
      .WithArgs(std::move(item_ids))
      .Then(base::BindOnce(&ContentCacheImpl::OnItemsEvictedFromDatabase,
                           weak_ptr_factory_.GetWeakPtr(),
                           base::OwnedRef(evicted_items)));
}

void ContentCacheImpl::OnItemsEvictedFromDatabase(
    EvictedItemStats& evicted_items,
    bool success) {
  LOG_IF(ERROR, !success) << "Couldn't remove items from database";
  // Now all the items on the disk have been removed, if the database call
  // failed the next time the cache is rebuilt (via `LoadFromDisk`) these items
  // will be attempted to be removed again.
  on_evicted_callbacks_.Notify(evicted_items);
}

void ContentCacheImpl::OnItemRemovedFromDisk(
    ContentLRUCache::reverse_iterator it,
    std::vector<int64_t>& item_ids,
    EvictedItemStats& evicted_items,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (success) {
    item_ids.emplace_back(it->second.id);
    evicted_items.bytes_evicted += it->second.bytes_on_disk;
    lru_cache_.Erase(it);
    evicted_items.num_items++;
    DCHECK_GT(cache_items_to_remove_, 0u);
    cache_items_to_remove_--;
  } else {
    LOG(ERROR) << "Failed to remove " << it->second.id << " from disk";
  }

  // Increment the iterator and continue identifying files to be marked for
  // removal. In the event no more items are identified, all items in `item_ids`
  // will be evicted from the database.
  EvictItemsMarkedForRemoval(++it, item_ids, evicted_items);
}

void ContentCacheImpl::MarkItemsForEviction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The cache size should not include the items that are expected to be removed
  // as these will get evicted on the next eviction cycle.
  size_t cache_items_without_marked_for_removal =
      lru_cache_.size() - cache_items_to_remove_;
  if (cache_items_without_marked_for_removal <= max_cache_items_) {
    VLOG(2) << "No items to evict: {cache_items_without_marked_for_removal = "
            << cache_items_without_marked_for_removal
            << ", max_cache_items = " << max_cache_items_ << "}";
    return;
  }

  // Number of items to evict (including items already marked for removal).
  size_t items_to_evict = lru_cache_.size() - max_cache_items_;
  VLOG(2) << items_to_evict << " items to be marked for removal, including "
          << cache_items_to_remove_ << " already marked";

  ContentLRUCache::reverse_iterator it = lru_cache_.rbegin();
  while (items_to_evict > 0) {
    CacheFileContext& ctx = it->second;
    const base::FilePath& path = it->first;
    if (!ctx.marked_for_removal) {
      VLOG(2) << "Marking '" << path.value() << "' for removal";
      ctx.marked_for_removal = true;
      cache_items_to_remove_++;
    } else {
      VLOG(2) << "Item '" << path.value() << "' already marked for removal";
    }
    items_to_evict--;
    it++;
  }
}

bool ContentCacheImpl::StartReadBytes(
    const OpenedCloudFile& file,
    net::IOBuffer* buffer,
    int64_t offset,
    int length,
    ProvidedFileSystemInterface::ReadChunkReceivedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VLOG(1) << "ReadBytes {path = '" << file.file_path << "', version_tag = '"
          << file.version_tag << "', offset = '" << offset << "', length = '"
          << length << "'}";

  ContentLRUCache::iterator it = lru_cache_.Get(file.file_path);
  if (it == lru_cache_.end()) {
    VLOG(1) << "Cache miss: entire file is not in cache";
    return false;
  }

  const CacheFileContext& ctx = it->second;
  if (ctx.marked_for_removal) {
    VLOG(1) << "Cache miss: file marked for removal";
    return false;
  }

  if (ctx.version_tag != file.version_tag) {
    VLOG(1) << "Cache miss: file is not up to date";
    return false;
  }

  if (offset == ctx.bytes_on_disk && offset == file.bytes_in_cloud) {
    VLOG(1) << "Ignored request: offset is at EOF";
    callback.Run(0, false, base::File::FILE_OK);
    return true;
  }

  // In the event the offset exceeds the known `bytes_on_disk` then we can't
  // reliably serve this data from the content cache.
  if (offset >= ctx.bytes_on_disk) {
    VLOG(1) << "Cache miss: requested byte range {offset = '" << offset
            << "', length = '" << length
            << "'} not available {bytes_on_disk = '" << ctx.bytes_on_disk
            << "'}";
    return false;
  }

  // It's possible that the file on disk can't entirely fulfill the offset +
  // length bytes request. In this instance, the callback will be invoked with
  // `bytes_read` (which will be less than length) and it's up to the caller to
  // make a follow up call for the remainder (which will then be served from the
  // underlying FSP).
  VLOG(1) << "Cache hit: Range {offset = '" << offset << "', length = '"
          << length << "', bytes_on_disk = '" << ctx.bytes_on_disk
          << "'} is available";
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadBytesBlocking, GetPathOnDiskFromId(ctx.id),
                     base::WrapRefCounted(buffer), offset, length),
      base::BindOnce(&ContentCacheImpl::OnBytesRead,
                     weak_ptr_factory_.GetWeakPtr(), file.file_path,
                     std::move(callback)));

  return true;
}

void ContentCacheImpl::OnBytesRead(
    const base::FilePath& file_path,
    ProvidedFileSystemInterface::ReadChunkReceivedCallback callback,
    FileErrorOrBytesRead error_or_bytes_read) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::File::Error result = error_or_bytes_read.error_or(base::File::FILE_OK);
  VLOG(1) << "OnBytesRead: " << base::File::ErrorToString(result);

  if (result != base::File::FILE_OK) {
    callback.Run(/*bytes_read=*/0, /*has_more=*/false, result);
    return;
  }

  ContentLRUCache::iterator it = lru_cache_.Get(file_path);
  DCHECK(it != lru_cache_.end());

  // Update the accessed time to now, but don't wait for the database to return,
  // just fire and forget.
  CacheFileContext& ctx = it->second;
  ctx.accessed_time = base::Time::Now();
  context_db_.AsyncCall(&ContextDatabase::UpdateAccessedTime)
      .WithArgs(ctx.id, ctx.accessed_time)
      .Then(base::BindOnce([](bool success) {
        LOG_IF(ERROR, !success) << "Couldn't update access time on read";
      }));

  int bytes_read = error_or_bytes_read.value();
  VLOG(1) << "OnBytesRead {bytes_read = '" << bytes_read << "'}";
  callback.Run(bytes_read, /*has_more=*/false, base::File::FILE_OK);
}

bool ContentCacheImpl::StartWriteBytes(const OpenedCloudFile& file,
                                       net::IOBuffer* buffer,
                                       int64_t offset,
                                       int length,
                                       FileErrorCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (file.version_tag.empty()) {
    VLOG(1) << "Empty version tag can't be written to cache";
    return false;
  }

  ContentLRUCache::iterator it = lru_cache_.Get(file.file_path);
  if (it == lru_cache_.end()) {
    // The file doesn't exist in the cache yet, create `CacheFileContext` with
    // the supplied version_tag.
    it = lru_cache_.Put(
        PathContextPair(file.file_path, CacheFileContext(file.version_tag)));
    MarkItemsForEviction();
  }

  CacheFileContext& ctx = it->second;
  if (ctx.marked_for_removal) {
    VLOG(1) << "Cache miss: file marked for removal";
    return false;
  }

  if (ctx.bytes_on_disk != offset) {
    VLOG(1) << "Unsupported write offset supplied {bytes_on_disk = '"
            << ctx.bytes_on_disk << "', offset = '" << offset << "'}";
    return false;
  }

  if (ctx.in_progress_writer) {
    VLOG(1)
        << "Writer is in progress already, multi offset writers not supported";
    return false;
  }
  ctx.in_progress_writer = true;

  auto write_bytes_callback = base::BindOnce(
      &WriteBytesBlocking, base::WrapRefCounted(buffer), offset, length);
  auto on_bytes_written_callback = base::BindOnce(
      &ContentCacheImpl::OnBytesWritten, weak_ptr_factory_.GetWeakPtr(),
      file.file_path, offset, length, std::move(callback));

  if (ctx.id == kUnknownId) {
    // An unknown ID means this is the first write to the filesystem. Let's
    // retrieve an ID first that will be used as the actual file name on disk.
    context_db_.AsyncCall(&ContextDatabase::AddItem)
        .WithArgs(file.file_path, file.version_tag, ctx.accessed_time, &ctx.id)
        .Then(base::BindOnce(&ContentCacheImpl::OnFileIdGenerated,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(write_bytes_callback),
                             std::move(on_bytes_written_callback), &ctx.id));
  } else {
    // The ID has already been created and is known on disk, bypass generating
    // the ID and simply start writing to the file.
    io_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(std::move(write_bytes_callback),
                       GetPathOnDiskFromId(ctx.id)),
        std::move(on_bytes_written_callback));
  }

  VLOG(1) << "Conditions satisified, starting to write file to disk";
  return true;
}

void ContentCacheImpl::OnFileIdGenerated(
    base::OnceCallback<base::File::Error(const base::FilePath& path)>
        write_bytes_callback,
    FileErrorCallback on_bytes_written_callback,
    int64_t* inserted_id,
    bool item_add_success) {
  if (!item_add_success) {
    LOG(ERROR) << "Failed to add item to the database";
    std::move(on_bytes_written_callback).Run(base::File::FILE_ERROR_FAILED);
    return;
  }

  DCHECK(inserted_id);
  DCHECK_GT(*inserted_id, 0);
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(std::move(write_bytes_callback),
                     GetPathOnDiskFromId(*inserted_id)),
      std::move(on_bytes_written_callback));
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
    ctx.bytes_on_disk = offset + length;
    ctx.accessed_time = base::Time::Now();

    // Keep the accessed time up to date.
    context_db_.AsyncCall(&ContextDatabase::UpdateAccessedTime)
        .WithArgs(ctx.id, ctx.accessed_time)
        .Then(base::BindOnce([](bool success) {
          LOG_IF(ERROR, !success) << "Couldn't update access time on write";
        }));
  }
  ctx.in_progress_writer = false;

  VLOG(1) << "OnBytesWritten: {offset = '" << offset << "', length = '"
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
      FROM_HERE, base::BindOnce(&GetIdFromCachedFiles, root_dir_),
      base::BindOnce(&ContentCacheImpl::GotFilesFromDisk,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContentCacheImpl::GotFilesFromDisk(
    base::OnceClosure callback,
    std::map<int, CacheFileContext> contexts) {
  // Get all the items from the database.
  context_db_.AsyncCall(&ContextDatabase::GetAllItems)
      .Then(base::BindOnce(&ContentCacheImpl::GotItemsFromContextDatabase,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           std::move(contexts)));
}

void ContentCacheImpl::GotItemsFromContextDatabase(
    base::OnceClosure callback,
    std::map<int, CacheFileContext> contexts_on_disk,
    ContextDatabase::IdToItemMap items_in_db) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Identify files on disk that have no entry in the database.
  std::set<base::FilePath> paths_on_disk_to_remove;
  std::list<PathContextPair> cached_files;
  for (auto& [id, ctx] : contexts_on_disk) {
    ContextDatabase::IdToItemMap::const_iterator item_it = items_in_db.find(id);
    if (item_it == items_in_db.end()) {
      paths_on_disk_to_remove.emplace(
          root_dir_.Append(base::NumberToString(ctx.id)));
    } else {
      ctx.accessed_time = item_it->second.accessed_time;
      ctx.version_tag = item_it->second.version_tag;
      cached_files.emplace_back(item_it->second.fsp_path, std::move(ctx));
    }
  }

  // Identify SQL entries that have no file on disk.
  std::vector<int64_t> ids_in_db_to_remove;
  for (const auto& [id, item] : items_in_db) {
    if (!contexts_on_disk.contains(id)) {
      ids_in_db_to_remove.emplace_back(id);
    }
  }

  cached_files.sort([](const PathContextPair& lhs, const PathContextPair& rhs) {
    // The files should be in least-recently used order, the underlying data
    // structure maintains them in order on access but not on initialization so
    // we have to ensure order now. This means the most-recently used is the
    // 0-th item, where eviction will take place on the last element.
    return lhs.second.accessed_time > rhs.second.accessed_time;
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
      base::BindOnce(&RemoveAllFilesOnDiskById, paths_on_disk_to_remove),
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
    if (!cache_file_context.marked_for_removal) {
      cached_file_paths.push_back(file_path);
    }
  }
  return cached_file_paths;
}

}  // namespace ash::file_system_provider
