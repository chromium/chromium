// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/content_cache/content_cache_impl.h"

#include "base/files/file.h"
#include "base/memory/scoped_refptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/file_system_provider/cloud_file_system.h"
#include "chrome/browser/ash/file_system_provider/content_cache/cache_file_context.h"
#include "chrome/browser/ash/file_system_provider/content_cache/context_database.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "net/base/io_buffer.h"

namespace ash::file_system_provider {

namespace {

base::File::Error WriteBytesBlocking(const base::FilePath& path,
                                     scoped_refptr<net::IOBuffer> buffer,
                                     int64_t offset,
                                     int length) {
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
      base::BindOnce(&ReadBytesBlocking, GetPathOnDiskFromContext(ctx),
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

  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WriteBytesBlocking, GetPathOnDiskFromContext(ctx),
                     base::WrapRefCounted(buffer), offset, length),
      base::BindOnce(&ContentCacheImpl::OnBytesWritten,
                     weak_ptr_factory_.GetWeakPtr(), file.file_path, offset,
                     length, std::move(callback)));

  VLOG(1) << "Conditions satisified, starting to write file to disk";
  return true;
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
  ctx.in_progress_writer = false;
  if (result == base::File::FILE_OK) {
    ctx.bytes_on_disk = offset + length;
  }

  VLOG(1) << "OnBytesWritten: {offset = '" << offset << "', length = '"
          << length << "', result = '" << base::File::ErrorToString(result)
          << "'}";
  std::move(callback).Run(result);
}

const base::FilePath ContentCacheImpl::GetPathOnDiskFromContext(
    const CacheFileContext& ctx) {
  return root_dir_.Append(ctx.id.ToString());
}

}  // namespace ash::file_system_provider
