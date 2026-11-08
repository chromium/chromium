// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_ANDROID_H_
#define BASE_FILES_FILE_ANDROID_H_

#include <string_view>

#include "base/android/scoped_java_ref.h"
#include "base/base_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/types/expected.h"

namespace base {

// Access modes for OpenAndroidDocument(). Each maps to exactly one
// ParcelFileDescriptor mode string. "w" is not offered because providers
// disagree on whether it truncates.
enum class AndroidDocumentAccessMode {
  kRead,                // "r"
  kReadWrite,           // "rw"
  kOverwrite,           // "wt"
  kReadWriteOverwrite,  // "rwt"
  kAppend,              // "wa"
};

// Resolves a SAF virtual document path (/SAF/...) to the content URI of an
// existing document. FILE_ERROR_NOT_FOUND means a complete provider listing
// had no match. An incomplete or failed listing returns FILE_ERROR_FAILED,
// duplicate display names or an unsupported provider query return
// FILE_ERROR_INVALID_OPERATION, a non-directory intermediate segment returns
// FILE_ERROR_NOT_A_DIRECTORY, and a directory target returns
// FILE_ERROR_NOT_A_FILE.
BASE_EXPORT expected<FilePath, File::Error> ResolveAndroidDocument(
    const FilePath& virtual_path);

// Creates a document of type `mime_type` in the parent directory of
// `virtual_path` with a single DocumentsContract.createDocument() call and
// returns its content URI. The provider may assign a different display name, so
// callers that need to keep operating on the newly created document must retain
// the returned URI rather than resolving `virtual_path` again. `mime_type` must
// not be empty; callers that do not know the type pass
// kUnknownAndroidDocumentMimeType (see base/android/virtual_document_path.h).
// Nothing is deleted if a later step of the caller fails.
BASE_EXPORT expected<FilePath, File::Error> CreateAndroidDocument(
    const FilePath& virtual_path,
    std::string_view mime_type);

// Opens the document identified by `content_uri` with `access`. The returned
// File reports created() == false; callers that created the document keep that
// result from CreateAndroidDocument().
BASE_EXPORT File OpenAndroidDocument(const FilePath& content_uri,
                                     AndroidDocumentAccessMode access,
                                     bool async = false);

namespace files_internal {

struct OpenAndroidFileResult {
  OpenAndroidFileResult(
      base::FilePath content_uri,
      int fd,
      base::android::ScopedJavaGlobalRef<jobject> java_parcel_file_descriptor,
      bool created);

  OpenAndroidFileResult(const OpenAndroidFileResult&) = delete;
  OpenAndroidFileResult& operator=(const OpenAndroidFileResult&) = delete;
  OpenAndroidFileResult(OpenAndroidFileResult&&);
  OpenAndroidFileResult& operator=(OpenAndroidFileResult&&);

  ~OpenAndroidFileResult();

  // The content URI of the file that was opened.
  base::FilePath content_uri;
  // The file descriptor. The caller gains ownership of it through the
  // `java_parcel_file_descriptor` below. When it is closed (in
  // `File::Close()`), the file descriptor is essentially closed.
  int fd;
  // The corresponding Java ParcelFileDescriptor object.
  base::android::ScopedJavaGlobalRef<jobject> java_parcel_file_descriptor;
  // Set to true if the file was created or truncated.
  bool created;
};

// Open an android file (i.e. content URI or a virtual document path) with given
// File::Flag.
base::expected<OpenAndroidFileResult, base::File::Error> OpenAndroidFile(
    const base::FilePath& path,
    uint32_t flags);

}  // namespace files_internal

}  // namespace base

#endif  // BASE_FILES_FILE_ANDROID_H_
