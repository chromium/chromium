// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_ANDROID_CONTENT_URI_TEST_UTILS_H_
#define BASE_TEST_ANDROID_CONTENT_URI_TEST_UTILS_H_

#include <optional>

namespace base {
class FilePath;

namespace test::android {

// NativeTest app has a FileProvider org.chromium.native_test.fileprovider which
// includes files from its cache dir. If `path` is a path under the cache
// dir such as a path created under a ScopedTempDir, we can map it to a content
// URI.
//
std::optional<FilePath> GetContentUriFromCacheDirFilePath(const FilePath& path);

// Similar to GetContentUriFromCacheDirFilePath() but files are first read into
// memory any file descriptor will not be backed by a local file. This mimics
// how an in-memory or network-backed ContentProvider will behave.
std::optional<FilePath> GetInMemoryContentUriFromCacheDirFilePath(
    const FilePath& path);

}  // namespace test::android
}  // namespace base

#endif  // BASE_TEST_ANDROID_CONTENT_URI_TEST_UTILS_H_
