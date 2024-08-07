// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/android/content_uri_test_utils.h"

#include "base/android/path_utils.h"
#include "base/files/file_path.h"

namespace base::test::android {

bool GetContentUriFromCacheDirFilePath(const FilePath& file_name,
                                       FilePath* content_uri) {
  base::FilePath cache_dir;
  if (!base::android::GetCacheDirectory(&cache_dir)) {
    return false;
  }
  base::FilePath uri("content://org.chromium.native_test.fileprovider/cache/");
  if (!cache_dir.AppendRelativePath(file_name, &uri)) {
    return false;
  }
  *content_uri = uri;
  return true;
}

}  // namespace base::test::android
