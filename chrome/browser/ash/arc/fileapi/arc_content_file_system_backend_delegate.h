// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_BACKEND_DELEGATE_H_

#include <memory>

#include "chrome/browser/ash/fileapi/file_system_backend_delegate.h"

namespace arc {

// Delegate implementation of the some methods in ash::FileSystemBackend
// for ARC content file system.
class ArcContentFileSystemBackendDelegate
    : public ash::FileSystemBackendDelegate {
 public:
  ArcContentFileSystemBackendDelegate();

  ArcContentFileSystemBackendDelegate(
      const ArcContentFileSystemBackendDelegate&) = delete;
  ArcContentFileSystemBackendDelegate& operator=(
      const ArcContentFileSystemBackendDelegate&) = delete;

  ~ArcContentFileSystemBackendDelegate() override;

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

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_BACKEND_DELEGATE_H_
