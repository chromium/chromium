// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/content_cache_impl.h"

#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/file_system_provider/cloud_file_system.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_file_context.h"
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

}  // namespace

ContentCacheImpl::ContentCacheImpl(const base::FilePath& root_dir,
                                   BoundContextDatabase context_db)
    : root_dir_(root_dir),
      io_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      context_db_(std::move(context_db)) {}

ContentCacheImpl::~ContentCacheImpl() {
  context_db_.Reset();
}

std::unique_ptr<ContentCache> ContentCacheImpl::Create(
    const base::FilePath& root_dir,
    BoundContextDatabase context_db) {
  return std::make_unique<ContentCacheImpl>(root_dir, std::move(context_db));
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

  const auto it = lru_cache_.Get(file.file_path);
  if (it == lru_cache_.end()) {
    VLOG(1) << "Cache miss: entire file is not in cache";
    return false;
  }

  const CacheFileContext& ctx = it->second;
  if (ctx.version_tag != file.version_tag) {
    VLOG(1) << "Cache miss: file is not up to date";
    return false;
  }

  if (ctx.bytes_on_disk < (offset + length)) {
    VLOG(1) << "Cache miss: requested byte range {offset = '" << offset
            << "', length = '" << length
            << "'} not available {bytes_on_disk = '" << ctx.bytes_on_disk
            << "'}";
    return false;
  }

  VLOG(1) << "Cache hit: Range {offset = '" << offset << "', length = '"
          << length << "'} is available";
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ReadBytesBlocking, GetPathOnDiskFromContext(ctx.id),
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
    // No `CacheFileContext` as this is the first write to the cache, let's
    // create it with the supplied version_tag.
    it = lru_cache_.Put(
        PathContextPair(file.file_path, CacheFileContext(file.version_tag)));
  }

  CacheFileContext& ctx = it->second;
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
                       GetPathOnDiskFromContext(ctx.id)),
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
                     GetPathOnDiskFromContext(*inserted_id)),
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

const base::FilePath ContentCacheImpl::GetPathOnDiskFromContext(int64_t id) {
  return root_dir_.Append(base::NumberToString(id));
}

}  // namespace ash::file_system_provider
