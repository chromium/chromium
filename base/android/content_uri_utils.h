// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_CONTENT_URI_UTILS_H_
#define BASE_ANDROID_CONTENT_URI_UTILS_H_

#include <jni.h>

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"

namespace base {
namespace internal {

// Check whether a content URI exists.
bool ContentUriExists(const FilePath& content_uri);

// Translates File::FLAG_* `open_flags` bitset to Java mode from
// ParcelFileDescriptor#parseMode(): ("r", "w", "wt", "wa", "rw" or "rwt").
// Disallows "w" which has been the source of android security issues.
// Returns nullopt if `open_flags` are not supported.
BASE_EXPORT std::optional<std::string> TranslateOpenFlagsToJavaMode(
    uint32_t open_flags);

// Opens a content URI and returns the file descriptor to the caller.
// `open_flags` is a bitmap of File::FLAG_* values.
// Returns -1 if the URI is invalid.
int OpenContentUri(const FilePath& content_uri, uint32_t open_flags);

// Returns true if file exists and results are populated, else returns false.
// Java code requires a Content-URI to look up file info such as is-dir, size,
// and last-mod, so code that needs to support Content-URI should use
// base::GetFileInfo(FilePath) which calls to this function rather than
// File::GetInfo() which cannot call this.
bool ContentUriGetFileInfo(const FilePath& content_uri, File::Info* results);

// Returns list of files in `content_uri` directory.
std::vector<FileEnumerator::FileInfo> ListContentUriDirectory(
    const FilePath& content_uri);

// Deletes a content URI.
bool DeleteContentUri(const FilePath& content_uri);

}  // namespace internal

// Gets MIME type from a content URI. Returns an empty string if the URI is
// invalid.
BASE_EXPORT std::string GetContentUriMimeType(const FilePath& content_uri);

// Gets the display name from a content URI. Returns true if the name was found.
BASE_EXPORT bool MaybeGetFileDisplayName(const FilePath& content_uri,
                                         std::u16string* file_display_name);

// Build URI using tree_uri and encoded_document_id.
BASE_EXPORT FilePath
ContentUriBuildDocumentUriUsingTree(const FilePath& tree_uri,
                                    const std::string& encoded_document_id);

}  // namespace base

#endif  // BASE_ANDROID_CONTENT_URI_UTILS_H_
