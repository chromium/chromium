// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_paths.h"

#include "base/android/path_utils.h"
#include "base/base_paths_android.h"
#include "base/files/file_util.h"
#include "base/path_service.h"

namespace android_webview {

bool PathProvider(int key, base::FilePath* result) {
  base::FilePath cur;

  switch (key) {
    case DIR_CRASH_DUMPS:
      if (!base::android::GetCacheDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("Crashpad"));
      break;
    case DIR_COMPONENTS_ROOT:
      if (!base::android::GetDataDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("components"))
                .Append(FILE_PATH_LITERAL("cus"));
      break;
    case DIR_COMPONENTS_TEMP:
      if (!base::android::GetCacheDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("components"))
                .Append(FILE_PATH_LITERAL("cus"));
      break;
    case DIR_SAFE_BROWSING:
      if (!base::android::GetCacheDirectory(&cur))
        return false;
      cur = cur.Append(FILE_PATH_LITERAL("SafeBrowsing"));
      break;
    case DIR_LOCAL_TRACES:
      if (!base::android::GetCacheDirectory(&cur)) {
        return false;
      }
      cur = cur.Append(FILE_PATH_LITERAL("Local Traces"));
      break;
    default:
      return false;
  }

  *result = cur;
  return true;
}

void RegisterPathProvider() {
  base::PathService::RegisterProvider(PathProvider, PATH_START, PATH_END);
}

}  // namespace android_webview
