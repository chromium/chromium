// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_ANDROID_CONTENT_URI_TEST_UTILS_H_
#define BASE_TEST_ANDROID_CONTENT_URI_TEST_UTILS_H_

#include <optional>

namespace base {
class FilePath;
class ScopedTempDir;

namespace test::android {

// Returns a content-URI for FileProvider org.chromium.native_test.fileprovider
// representing `path` if it is a valid file or dir under the android app cache
// dir such as a path created in a ScopedTempDir, else returns std::nullopt.
std::optional<FilePath> GetContentUriFromCacheDirFilePath(const FilePath& path);

// Similar to GetContentUriFromCacheDirFilePath() but files are first read into
// memory and any file descriptor will not be backed by a local file. This
// mimics how an in-memory or network-backed ContentProvider will behave.
std::optional<FilePath> GetInMemoryContentUriFromCacheDirFilePath(
    const FilePath& path);

// Returns a DocumentFile Document URI for the specified file which must be
// under the cache dir, else returns std::nullopt. This mimics how a
// DocumentsProvider will work with ACTION_OPEN_DOCUMENT.
std::optional<FilePath> GetInMemoryContentDocumentUriFromCacheDirFilePath(
    const FilePath& path);

// Returns a DocumentFile Tree URI for the specified directory which must be
// under the cache dir, else returns std::nullopt. This mimics how a
// DocumentsProvider will work with ACTION_OPEN_DOCUMENT_TREE.
std::optional<FilePath> GetInMemoryContentTreeUriFromCacheDirDirectory(
    const FilePath& directory);

// Returns a virtual document path for the specified directory which must be
// under the cache dir, else returns std::nullopt.
std::optional<FilePath> GetVirtualDocumentPathFromCacheDirDirectory(
    const FilePath& path);

// Copies a source directory into an existing ScopedTempDir and return the
// virtual document path for it.
//
// This is a workaround for Android security policies that prevent loading
// extensions directly from the file system. This function enables tests by
// copying the extension directory to a temporary location and resolving it to
// a content URI, which can then be used for extension packing.
std::optional<FilePath> CreateCacheCopyAndGetVirtualDocumentPath(
    const FilePath& source_path,
    const ScopedTempDir& temp_dir);

}  // namespace test::android
}  // namespace base

#endif  // BASE_TEST_ANDROID_CONTENT_URI_TEST_UTILS_H_
