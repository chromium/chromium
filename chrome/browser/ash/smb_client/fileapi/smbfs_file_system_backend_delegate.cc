// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/smb_client/fileapi/smbfs_file_system_backend_delegate.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/browser/ash/smb_client/fileapi/smbfs_async_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_stream_reader.h"
#include "storage/browser/file_system/file_stream_writer.h"

namespace ash::smb_client {

SmbFsFileSystemBackendDelegate::SmbFsFileSystemBackendDelegate(Profile* profile)
    : async_file_util_(std::make_unique<SmbFsAsyncFileUtil>(profile)) {}

SmbFsFileSystemBackendDelegate::~SmbFsFileSystemBackendDelegate() = default;

storage::AsyncFileUtil* SmbFsFileSystemBackendDelegate::GetAsyncFileUtil(
    storage::FileSystemType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK_EQ(storage::kFileSystemTypeSmbFs, type);
  return async_file_util_.get();
}

std::unique_ptr<storage::FileStreamReader>
SmbFsFileSystemBackendDelegate::CreateFileStreamReader(
    const storage::FileSystemURL& url,
    int64_t offset,
    int64_t max_bytes_to_read,
    const base::Time& expected_modification_time,
    storage::FileSystemContext* context) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

std::unique_ptr<storage::FileStreamWriter>
SmbFsFileSystemBackendDelegate::CreateFileStreamWriter(
    const storage::FileSystemURL& url,
    int64_t offset,
    storage::FileSystemContext* context) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

storage::WatcherManager* SmbFsFileSystemBackendDelegate::GetWatcherManager(
    storage::FileSystemType type) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

}  // namespace ash::smb_client
