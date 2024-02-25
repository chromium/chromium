// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DRIVE_FILEAPI_DRIVEFS_ASYNC_FILE_UTIL_H_
#define CHROME_BROWSER_ASH_DRIVE_FILEAPI_DRIVEFS_ASYNC_FILE_UTIL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/async_file_util_adapter.h"

class Profile;

namespace drive::internal {

// The implementation of storage::AsyncFileUtil for DriveFS File System. This
// forwards to a AsyncFileUtil for native files by default.
class DriveFsAsyncFileUtil : public storage::AsyncFileUtilAdapter {
 public:
  explicit DriveFsAsyncFileUtil(Profile* profile);

  DriveFsAsyncFileUtil(const DriveFsAsyncFileUtil&) = delete;
  DriveFsAsyncFileUtil& operator=(const DriveFsAsyncFileUtil&) = delete;

  ~DriveFsAsyncFileUtil() override;

  // AsyncFileUtil overrides:
  void CopyFileLocal(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOptionSet options,
      CopyFileProgressCallback progress_callback,
      StatusCallback callback) override;
  void DeleteRecursively(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      StatusCallback callback) override;

 private:
  const raw_ptr<Profile, DanglingUntriaged> profile_;

  base::WeakPtrFactory<DriveFsAsyncFileUtil> weak_factory_{this};
};

}  // namespace drive::internal

#endif  // CHROME_BROWSER_ASH_DRIVE_FILEAPI_DRIVEFS_ASYNC_FILE_UTIL_H_
