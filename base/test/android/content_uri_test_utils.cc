// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/android/content_uri_test_utils.h"

#include "base/android/path_utils.h"
#include "base/files/file_path.h"

namespace base::test::android {

std::optional<FilePath> GetContentUriFromCacheDirFilePath(
    const FilePath& path) {
  base::FilePath cache_dir;
  if (!base::android::GetCacheDirectory(&cache_dir)) {
    return std::nullopt;
  }
  base::FilePath uri("content://org.chromium.native_test.fileprovider/cache/");
  if (!cache_dir.AppendRelativePath(path, &uri)) {
    return std::nullopt;
  }
  return uri;
}

std::optional<FilePath> GetInMemoryContentUriFromCacheDirFilePath(
    const FilePath& path) {
  base::FilePath cache_dir;
  if (!base::android::GetCacheDirectory(&cache_dir)) {
    return std::nullopt;
  }
  base::FilePath uri("content://org.chromium.native_test.inmemory/cache/");
  if (!cache_dir.AppendRelativePath(path, &uri)) {
    return std::nullopt;
  }
  return uri;
}

}  // namespace base::test::android
