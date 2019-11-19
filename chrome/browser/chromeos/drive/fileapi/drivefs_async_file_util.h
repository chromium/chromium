// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_DRIVEFS_ASYNC_FILE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_DRIVEFS_ASYNC_FILE_UTIL_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/async_file_util_adapter.h"

class Profile;

namespace drive {
namespace internal {

// The implementation of storage::AsyncFileUtil for DriveFS File System. This
// forwards to a AsyncFileUtil for native files by default.
class DriveFsAsyncFileUtil : public storage::AsyncFileUtilAdapter {
 public:
  explicit DriveFsAsyncFileUtil(Profile* profile);
  ~DriveFsAsyncFileUtil() override;

  // AsyncFileUtil overrides:
  void CopyFileLocal(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      CopyOrMoveOption option,
      CopyFileProgressCallback progress_callback,
      StatusCallback callback) override;
  void DeleteRecursively(
      std::unique_ptr<storage::FileSystemOperationContext> context,
      const storage::FileSystemURL& url,
      StatusCallback callback) override;

 private:
  Profile* const profile_;

  base::WeakPtrFactory<DriveFsAsyncFileUtil> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DriveFsAsyncFileUtil);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_FILEAPI_DRIVEFS_ASYNC_FILE_UTIL_H_
