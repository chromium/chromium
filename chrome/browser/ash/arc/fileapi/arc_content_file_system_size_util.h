// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SIZE_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SIZE_UTIL_H_

class GURL;

#include "base/files/file.h"
#include "base/functional/callback.h"

namespace arc {

using TruncateCallback = base::OnceCallback<void(base::File::Error error)>;

// Truncates the file to the specified length.
void TruncateOnIOThread(const GURL& content_url,
                        int64_t length,
                        TruncateCallback callback);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SIZE_UTIL_H_
