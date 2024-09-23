// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FILEAPI_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FILEAPI_BACKEND_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend_delegate.h"

namespace storage {
class AsyncFileUtil;
class FileSystemContext;
class FileStreamReader;
class FileSystemURL;
class FileStreamWriter;
class WatcherManager;
}  // namespace storage

namespace ash::file_system_provider {

// Delegate implementation of the some methods in FileSystemBackend
// for provided file systems.
class BackendDelegate : public FileSystemBackendDelegate {
 public:
  BackendDelegate();

  BackendDelegate(const BackendDelegate&) = delete;
  BackendDelegate& operator=(const BackendDelegate&) = delete;

  ~BackendDelegate() override;

  static std::unique_ptr<FileSystemBackendDelegate> MakeUnique();

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
  std::unique_ptr<storage::WatcherManager> watcher_manager_;
};

}  // namespace ash::file_system_provider

#endif  // CHROME_BROWSER_ASH_FILE_SYSTEM_PROVIDER_FILEAPI_BACKEND_DELEGATE_H_
