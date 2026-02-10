// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/android/content_uri_test_utils.h"

#include <optional>

#include "base/android/apk_info.h"
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace base::test::android {
namespace {
// Android's file system aliases "/data/data" to "/data/user/0" for the primary
// user. `base::AppendRelativePath()` fails if the base and child paths use
// conflicting aliases (e.g., base uses "/data/user/0" while child uses
// "/data/data").
//
// This helper attempts a standard relative path resolution first. If that
// fails, it normalizes the child path to match the base path's Android alias
// and retries.
bool AppendRelativePathWithAndroidAliases(const base::FilePath& base,
                                          const base::FilePath& child,
                                          base::FilePath* result) {
  if (base.AppendRelativePath(child, result)) {
    return true;
  }

  // Handle Android specific aliasing mismatch.
  // We only handle the case where the base is in the specific user directory
  // (/data/user/0) and the child is in the generic data directory (/data/data).
  const base::FilePath base_prefix(FILE_PATH_LITERAL("/data/user/0"));
  const base::FilePath child_prefix(FILE_PATH_LITERAL("/data/data"));

  // Ensure base path is inside "/data/user/0".
  if (base != base_prefix && !base_prefix.IsParent(base)) {
    return false;
  }

  // Ensure child path is inside "/data/data" and extracts the relative path.
  base::FilePath child_relative;
  if (!child_prefix.AppendRelativePath(child, &child_relative)) {
    return false;
  }

  // Reconstruct 'child' using the 'base' prefix and retry AppendRelativePath.
  base::FilePath child_normalized = base_prefix.Append(child_relative);
  return base.AppendRelativePath(child_normalized, result);
}

std::optional<FilePath> GetInMemoryContentDocumentUriFromCacheDirPath(
    const FilePath& path,
    bool is_tree) {
  base::FilePath cache_dir;
  if (!base::android::GetCacheDirectory(&cache_dir)) {
    return std::nullopt;
  }
  base::FilePath document_id;
  if (!AppendRelativePathWithAndroidAliases(cache_dir, path, &document_id)) {
    return std::nullopt;
  }
  base::FilePath uri(
      base::StrCat({"content://", base::android::apk_info::package_name(),
                    ".docprov/", is_tree ? "tree/" : "document/",
                    base::EscapeAllExceptUnreserved(document_id.value())}));
  return uri;
}
}  // namespace

std::optional<FilePath> GetContentUriFromCacheDirFilePath(
    const FilePath& path) {
  base::FilePath cache_dir;
  if (!base::android::GetCacheDirectory(&cache_dir)) {
    return std::nullopt;
  }
  base::FilePath uri(
      base::StrCat({"content://", base::android::apk_info::package_name(),
                    ".fileprovider/cache/"}));
  if (!AppendRelativePathWithAndroidAliases(cache_dir, path, &uri)) {
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
  base::FilePath uri(
      base::StrCat({"content://", base::android::apk_info::package_name(),
                    ".inmemory/cache/"}));
  if (!AppendRelativePathWithAndroidAliases(cache_dir, path, &uri)) {
    return std::nullopt;
  }
  return uri;
}

std::optional<FilePath> GetInMemoryContentDocumentUriFromCacheDirFilePath(
    const FilePath& path) {
  return GetInMemoryContentDocumentUriFromCacheDirPath(path, /*is_tree=*/false);
}

std::optional<FilePath> GetInMemoryContentTreeUriFromCacheDirDirectory(
    const FilePath& path) {
  return GetInMemoryContentDocumentUriFromCacheDirPath(path, /*is_tree=*/true);
}

std::optional<FilePath> GetVirtualDocumentPathFromCacheDirDirectory(
    const FilePath& path) {
  std::optional<FilePath> content_url =
      GetInMemoryContentTreeUriFromCacheDirDirectory(path);
  if (!content_url) {
    return std::nullopt;
  }
  return base::ResolveToVirtualDocumentPath(*content_url);
}

}  // namespace base::test::android
