// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/mtp_file_system_backend_delegate.h"

#include "chrome/browser/media_galleries/fileapi/device_media_async_file_util.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash {

MTPFileSystemBackendDelegate::MTPFileSystemBackendDelegate(
    const base::FilePath& storage_partition_path)
    : device_media_async_file_util_(
          DeviceMediaAsyncFileUtil::Create(storage_partition_path,
                                           NO_MEDIA_FILE_VALIDATION)),
      mtp_watcher_manager_(
          new MTPWatcherManager(device_media_async_file_util_.get())) {
}

MTPFileSystemBackendDelegate::~MTPFileSystemBackendDelegate() {
}

storage::AsyncFileUtil* MTPFileSystemBackendDelegate::GetAsyncFileUtil(
    storage::FileSystemType type) {
  DCHECK_EQ(storage::kFileSystemTypeDeviceMediaAsFileStorage, type);

  return device_media_async_file_util_.get();
}

std::unique_ptr<storage::FileStreamReader>
MTPFileSystemBackendDelegate::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  DCHECK_EQ(storage::kFileSystemTypeDeviceMediaAsFileStorage, url.type());

  return device_media_async_file_util_->GetFileStreamReader(
      url, offset, expected_modification_time, context);
}

std::unique_ptr<storage::FileStreamWriter>
MTPFileSystemBackendDelegate::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset,
    storage::FileSystemContext* context) {
  DCHECK_EQ(storage::kFileSystemTypeDeviceMediaAsFileStorage, url.type());

  // TODO(kinaba): support writing.
  return nullptr;
}

storage::WatcherManager* MTPFileSystemBackendDelegate::GetWatcherManager(
    storage::FileSystemType type) {
  DCHECK_EQ(storage::kFileSystemTypeDeviceMediaAsFileStorage, type);
  return mtp_watcher_manager_.get();
}

}  // namespace ash
