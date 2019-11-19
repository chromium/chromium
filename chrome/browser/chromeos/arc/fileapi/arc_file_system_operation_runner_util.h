// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"

class GURL;

namespace arc {

namespace file_system_operation_runner_util {

using GetFileSizeCallback = ArcFileSystemOperationRunner::GetFileSizeCallback;
using OpenFileToReadCallback =
    ArcFileSystemOperationRunner::OpenFileToReadCallback;
using OpenFileToWriteCallback =
    ArcFileSystemOperationRunner::OpenFileToWriteCallback;

// Utility functions to post a task to run ArcFileSystemOperationRunner methods.
// These functions must be called on the IO thread. Callbacks and observers will
// be called on the IO thread.
void GetFileSizeOnIOThread(const GURL& url, GetFileSizeCallback callback);
void OpenFileToReadOnIOThread(const GURL& url, OpenFileToReadCallback callback);
void OpenFileToWriteOnIOThread(const GURL& url,
                               OpenFileToWriteCallback callback);

}  // namespace file_system_operation_runner_util
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_FILE_SYSTEM_OPERATION_RUNNER_UTIL_H_
