// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_

#include <string>

#include "chrome/browser/ash/arc/fileapi/arc_file_system_operation_runner.h"

class GURL;

namespace arc {

namespace file_system_operation_runner_util {

using GetFileSizeCallback = ArcFileSystemOperationRunner::GetFileSizeCallback;
using OpenFileSessionToWriteCallback =
    ArcFileSystemOperationRunner::OpenFileSessionToWriteCallback;
using OpenFileSessionToReadCallback =
    ArcFileSystemOperationRunner::OpenFileSessionToReadCallback;

enum class CloseStatus : int {
  // File operation finished successfully.
  kStatusOk = 0,

  // File operation was cancelled.
  kStatusCancel = 1,

  // File operation completed with error.
  kStatusError = 2,
};

// Utility functions to post a task to run ArcFileSystemOperationRunner methods.
// These functions must be called on the IO thread. Callbacks and observers will
// be called on the IO thread.
void GetFileSizeOnIOThread(const GURL& url, GetFileSizeCallback callback);
void OpenFileSessionToWriteOnIOThread(const GURL& url,
                                      OpenFileSessionToWriteCallback callback);
void OpenFileSessionToReadOnIOThread(const GURL& url,
                                     OpenFileSessionToReadCallback callback);

// Calls to OpenFileSession* must be followed up with a call to CloseFileSession
// once the file is no longer in use to close the Android file descriptor.
void CloseFileSession(const std::string& url_id, const CloseStatus status);

}  // namespace file_system_operation_runner_util
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_
