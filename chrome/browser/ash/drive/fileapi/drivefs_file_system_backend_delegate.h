// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DRIVE_FILEAPI_DRIVEFS_FILE_SYSTEM_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_ASH_DRIVE_FILEAPI_DRIVEFS_FILE_SYSTEM_BACKEND_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "chrome/browser/ash/fileapi/file_system_backend_delegate.h"

class Profile;

namespace storage {
class AsyncFileUtil;
class FileSystemContext;
class FileStreamReader;
class FileSystemURL;
class FileStreamWriter;
class WatcherManager;
}  // namespace storage

namespace drive {

// Delegate implementation of the some methods in ash::FileSystemBackend
// for Drive file system.
class DriveFsFileSystemBackendDelegate : public ash::FileSystemBackendDelegate {
 public:
  explicit DriveFsFileSystemBackendDelegate(Profile* profile);

  DriveFsFileSystemBackendDelegate(const DriveFsFileSystemBackendDelegate&) =
      delete;
  DriveFsFileSystemBackendDelegate& operator=(
      const DriveFsFileSystemBackendDelegate&) = delete;

  ~DriveFsFileSystemBackendDelegate() override;

  // FileSystemBackend::Delegate overrides.
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
  std::unique_ptr<storage::AsyncFileUtil> async_file_util_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_ASH_DRIVE_FILEAPI_DRIVEFS_FILE_SYSTEM_BACKEND_DELEGATE_H_
