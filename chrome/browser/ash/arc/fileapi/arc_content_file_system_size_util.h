// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SIZE_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SIZE_UTIL_H_

class GURL;

#include "base/files/file.h"
#include "base/functional/callback.h"

namespace arc {

class ArcFileSystemOperationRunner;

constexpr int64_t kUnknownFileSize = -1;

using GetFileSizeFromOpenFileCallback =
    base::OnceCallback<void(base::File::Error error, int64_t file_size)>;

// Opens the file and gets the file size from the opened file descriptor.
// It will fail if the file descriptor returned by the open call is not a file.
// Must be run on the IO thread.
void GetFileSizeFromOpenFileOnIOThread(
    const GURL& content_url,
    GetFileSizeFromOpenFileCallback callback);
// Same as GetFileSizeFromOpenFileOnIOThread, but must be run on the UI thread.
void GetFileSizeFromOpenFileOnUIThread(
    const GURL& content_url,
    ArcFileSystemOperationRunner* runner,
    GetFileSizeFromOpenFileCallback callback);

using TruncateCallback = base::OnceCallback<void(base::File::Error error)>;

// Truncates the file to the specified length.
void TruncateOnIOThread(const GURL& content_url,
                        int64_t length,
                        TruncateCallback callback);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SIZE_UTIL_H_
