// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_android.h"

#include "base/android/content_uri_utils.h"
#include "base/android/scoped_java_ref.h"
#include "base/android/virtual_document_path.h"
#include "base/check.h"
#include "base/files/file_util.h"

namespace {

struct OpenContentUriResult {
  int fd;
  base::android::ScopedJavaGlobalRef<jobject> java_parcel_file_desciptor;
};

base::expected<OpenContentUriResult, base::File::Error> OpenContentUriAndGetFd(
    const base::FilePath& path,
    uint32_t flags) {
  CHECK(path.IsContentUri());

  OpenContentUriResult result;

  result.java_parcel_file_desciptor =
      base::internal::OpenContentUri(path, flags);

  result.fd =
      base::internal::ContentUriGetFd(result.java_parcel_file_desciptor);
  if (result.fd < 0) {
    return base::unexpected(base::File::Error::FILE_ERROR_FAILED);
  }
  return result;
}

}  // namespace

namespace base::files_internal {

OpenAndroidFileResult::OpenAndroidFileResult(
    base::FilePath content_uri,
    int fd,
    base::android::ScopedJavaGlobalRef<jobject> java_parcel_file_descriptor,
    bool created)
    : content_uri(content_uri),
      fd(fd),
      java_parcel_file_descriptor(java_parcel_file_descriptor),
      created(created) {}

OpenAndroidFileResult::OpenAndroidFileResult(OpenAndroidFileResult&&) = default;
OpenAndroidFileResult& OpenAndroidFileResult::operator=(
    OpenAndroidFileResult&&) = default;

OpenAndroidFileResult::~OpenAndroidFileResult() = default;

base::expected<OpenAndroidFileResult, base::File::Error> OpenAndroidFile(
    const base::FilePath& path,
    uint32_t flags) {
  CHECK(path.IsContentUri() || path.IsVirtualDocumentPath());

  auto cu = ResolveToContentUri(path);
  if (cu) {
    if ((flags & File::Flags::FLAG_CREATE)) {
      return base::unexpected(File::Error::FILE_ERROR_EXISTS);
    }

    bool created = flags & File::Flags::FLAG_CREATE_ALWAYS;
    if (auto r = OpenContentUriAndGetFd(*cu, flags); r.has_value()) {
      return OpenAndroidFileResult(*cu, r->fd, r->java_parcel_file_desciptor,
                                   created);
    } else {
      return base::unexpected(r.error());
    }
  }

  // `path` was not resolved to a content URI, meaning it is a virtual document
  // path that does not exist.
  CHECK(path.IsVirtualDocumentPath());

  // If the flags don't instruct file creation, return an error.
  if (!(flags & (File::Flags::FLAG_CREATE | File::Flags::FLAG_CREATE_ALWAYS |
                 File::Flags::FLAG_OPEN_ALWAYS))) {
    return base::unexpected(File::Error::FILE_ERROR_NOT_FOUND);
  }

  std::optional<VirtualDocumentPath> vp =
      VirtualDocumentPath::Parse(path.value());
  CHECK(vp);

  std::optional<std::pair<std::string, bool>> create_result =
      vp->CreateOrOpen();
  if (!create_result) {
    return base::unexpected(File::Error::FILE_ERROR_NOT_A_DIRECTORY);
  }
  base::FilePath content_uri(create_result->first);
  bool created = create_result->second;

  if (auto r = OpenContentUriAndGetFd(content_uri, flags); r.has_value()) {
    return OpenAndroidFileResult(content_uri, r->fd,
                                 r->java_parcel_file_desciptor, created);
  } else {
    return base::unexpected(r.error());
  }
}

}  // namespace base::files_internal
