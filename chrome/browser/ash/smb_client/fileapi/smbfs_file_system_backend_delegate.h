// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SMB_CLIENT_FILEAPI_SMBFS_FILE_SYSTEM_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_ASH_SMB_CLIENT_FILEAPI_SMBFS_FILE_SYSTEM_BACKEND_DELEGATE_H_

#include <memory>

#include "chrome/browser/ash/fileapi/file_system_backend_delegate.h"

class Profile;

namespace ash::smb_client {

class SmbFsAsyncFileUtil;

class SmbFsFileSystemBackendDelegate : public FileSystemBackendDelegate {
 public:
  explicit SmbFsFileSystemBackendDelegate(Profile* profile);
  ~SmbFsFileSystemBackendDelegate() override;

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
  std::unique_ptr<SmbFsAsyncFileUtil> async_file_util_;
};

}  // namespace ash::smb_client

#endif  // CHROME_BROWSER_ASH_SMB_CLIENT_FILEAPI_SMBFS_FILE_SYSTEM_BACKEND_DELEGATE_H_
