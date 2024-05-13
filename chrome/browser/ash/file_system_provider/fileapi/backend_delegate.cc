// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_system_provider/fileapi/backend_delegate.h"

#include <memory>

#include "chrome/browser/ash/file_system_provider/fileapi/buffering_file_stream_reader.h"
#include "chrome/browser/ash/file_system_provider/fileapi/buffering_file_stream_writer.h"
#include "chrome/browser/ash/file_system_provider/fileapi/file_stream_reader.h"
#include "chrome/browser/ash/file_system_provider/fileapi/file_stream_writer.h"
#include "chrome/browser/ash/file_system_provider/fileapi/provider_async_file_util.h"
#include "chrome/browser/ash/file_system_provider/fileapi/watcher_manager.h"
#include "chrome/browser/ash/fileapi/diversion_backend_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_url.h"

using content::BrowserThread;

namespace ash::file_system_provider {
namespace {

// Size of the stream reader internal buffer. At most this number of bytes will
// be read ahead of the requested data.
const int kReaderBufferSize = 512 * 1024;  // 512KB.

// Size of the stream writer internal buffer. At most this number of bytes will
// be postponed for writing.
const int kWriterBufferSize = 512 * 1024;  // 512KB.

}  // namespace

BackendDelegate::BackendDelegate()
    : async_file_util_(new internal::ProviderAsyncFileUtil),
      watcher_manager_(new WatcherManager) {
}

BackendDelegate::~BackendDelegate() = default;

// static
std::unique_ptr<FileSystemBackendDelegate> BackendDelegate::MakeUnique() {
  std::unique_ptr<FileSystemBackendDelegate> wrappee(new BackendDelegate());
  return std::make_unique<ash::DiversionBackendDelegate>(std::move(wrappee));
}

storage::AsyncFileUtil* BackendDelegate::GetAsyncFileUtil(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeProvided, type);
  return async_file_util_.get();
}

std::unique_ptr<storage::FileStreamReader>
BackendDelegate::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeProvided, url.type());

  return std::unique_ptr<storage::FileStreamReader>(
      new BufferingFileStreamReader(
          std::unique_ptr<storage::FileStreamReader>(new FileStreamReader(
              context, url, offset, expected_modification_time)),
          kReaderBufferSize, max_bytes_to_read));
}

std::unique_ptr<storage::FileStreamWriter>
BackendDelegate::CreateFileStreamWriter(const storage::FileSystemURL& url,
                                        int64_t offset,
                                        storage::FileSystemContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeProvided, url.type());

  return std::unique_ptr<storage::FileStreamWriter>(
      new BufferingFileStreamWriter(std::unique_ptr<storage::FileStreamWriter>(
                                        new FileStreamWriter(url, offset)),
                                    kWriterBufferSize));
}

storage::WatcherManager* BackendDelegate::GetWatcherManager(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeProvided, type);
  return watcher_manager_.get();
}

}  // namespace ash::file_system_provider
