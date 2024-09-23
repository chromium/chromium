// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/drive/fileapi/drivefs_file_system_backend_delegate.h"

#include "chrome/browser/ash/drive/fileapi/drivefs_async_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/async_file_util.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"

namespace drive {

DriveFsFileSystemBackendDelegate::DriveFsFileSystemBackendDelegate(
    Profile* profile)
    : async_file_util_(
          std::make_unique<internal::DriveFsAsyncFileUtil>(profile)) {}

DriveFsFileSystemBackendDelegate::~DriveFsFileSystemBackendDelegate() = default;

storage::AsyncFileUtil* DriveFsFileSystemBackendDelegate::GetAsyncFileUtil(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeDriveFs, type);
  return async_file_util_.get();
}

std::unique_ptr<storage::FileStreamReader>
DriveFsFileSystemBackendDelegate::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<storage::FileStreamWriter>
DriveFsFileSystemBackendDelegate::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset,
    storage::FileSystemContext* context) {
  NOTIMPLEMENTED();
  return nullptr;
}

storage::WatcherManager* DriveFsFileSystemBackendDelegate::GetWatcherManager(
    storage::FileSystemType type) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace drive
