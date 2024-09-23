// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_
#define CHROME_BROWSER_ASH_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback_forward.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/common/file_system/file_system_types.h"

namespace base {
class Time;
}  // namespace base

namespace storage {
class AsyncFileUtil;
class FileSystemContext;
class FileStreamReader;
class FileSystemURL;
class FileStreamWriter;
class WatcherManager;
}  // namespace storage

namespace ash {

// This is delegate interface to inject the implementation of the some methods
// of FileSystemBackend.
class FileSystemBackendDelegate {
 public:
  virtual ~FileSystemBackendDelegate() {}

  // Called from FileSystemBackend::GetAsyncFileUtil().
  virtual storage::AsyncFileUtil* GetAsyncFileUtil(
      storage::FileSystemType type) = 0;

  // Called from FileSystemBackend::CreateFileStreamReader().
  virtual std::unique_ptr<storage::FileStreamReader> CreateFileStreamReader(
      const storage::FileSystemURL& url,
      int64_t offset,
      int64_t max_bytes_to_read,
      const base::Time& expected_modification_time,
      storage::FileSystemContext* context) = 0;

  // Called from FileSystemBackend::CreateFileStreamWriter().
  virtual std::unique_ptr<storage::FileStreamWriter> CreateFileStreamWriter(
      const storage::FileSystemURL& url,
      int64_t offset,
      storage::FileSystemContext* context) = 0;

  // Called from the FileSystemWatcherService class. The returned pointer must
  // stay valid until shutdown.
  virtual storage::WatcherManager* GetWatcherManager(
      storage::FileSystemType type) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_FILE_SYSTEM_BACKEND_DELEGATE_H_
