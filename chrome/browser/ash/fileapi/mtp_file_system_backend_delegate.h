// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_MTP_FILE_SYSTEM_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_ASH_FILEAPI_MTP_FILE_SYSTEM_BACKEND_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "chrome/browser/ash/fileapi/file_system_backend_delegate.h"
#include "chrome/browser/ash/fileapi/mtp_watcher_manager.h"

namespace base {
class FilePath;
}  // namespace base

namespace storage {
class AsyncFileUtil;
class FileSystemContext;
class FileStreamReader;
class FileSystemURL;
class FileStreamWriter;
class WatcherManager;
}  // namespace storage

class DeviceMediaAsyncFileUtil;

namespace ash {

// This is delegate interface to inject the MTP device file system in Chrome OS
// file API backend.
class MTPFileSystemBackendDelegate : public FileSystemBackendDelegate {
 public:
  explicit MTPFileSystemBackendDelegate(
      const base::FilePath& storage_partition_path);

  MTPFileSystemBackendDelegate(const MTPFileSystemBackendDelegate&) = delete;
  MTPFileSystemBackendDelegate& operator=(const MTPFileSystemBackendDelegate&) =
      delete;

  ~MTPFileSystemBackendDelegate() override;

  // FileSystemBackendDelegate overrides.
  storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) override;
  std::unique_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) override;
  std::unique_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64_t offset,
      storage::FileSystemContext* context) override;
  storage::WatcherManager* GetWatcherManager(
      storage::FileSystemType type) override;

 private:
  std::unique_ptr<DeviceMediaAsyncFileUtil> device_media_async_file_util_;
  std::unique_ptr<MTPWatcherManager> mtp_watcher_manager_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_MTP_FILE_SYSTEM_BACKEND_DELEGATE_H_
