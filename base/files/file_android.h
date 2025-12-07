// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_ANDROID_H_
#define BASE_FILES_FILE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/types/expected.h"

namespace base::files_internal {

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

}  // namespace base::files_internal

#endif  // BASE_FILES_FILE_ANDROID_H_
