// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_android.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/android/content_uri_utils.h"
#include "base/android/scoped_java_ref.h"
#include "base/android/virtual_document_path.h"
#include "base/check.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/types/expected_macros.h"

namespace {

struct OpenContentUriResult {
  int fd;
  base::android::ScopedJavaGlobalRef<jobject> java_parcel_file_descriptor;
};

base::expected<OpenContentUriResult, base::File::Error> OpenContentUriAndGetFd(
    const base::FilePath& path,
    uint32_t flags) {
  CHECK(path.IsContentUri());

  OpenContentUriResult result;

  result.java_parcel_file_descriptor =
      base::internal::OpenContentUri(path, flags);

  result.fd =
      base::internal::ContentUriGetFd(result.java_parcel_file_descriptor);
  if (result.fd < 0) {
    if (result.java_parcel_file_descriptor) {
      base::internal::ContentUriClose(result.java_parcel_file_descriptor);
    }
    return base::unexpected(base::File::Error::FILE_ERROR_FAILED);
  }
  return result;
}

std::string_view AccessModeToJavaMode(base::AndroidDocumentAccessMode access) {
  using base::AndroidDocumentAccessMode;
  switch (access) {
    case AndroidDocumentAccessMode::kRead:
      return "r";
    case AndroidDocumentAccessMode::kReadWrite:
      return "rw";
    case AndroidDocumentAccessMode::kOverwrite:
      return "wt";
    case AndroidDocumentAccessMode::kReadWriteOverwrite:
      return "rwt";
    case AndroidDocumentAccessMode::kAppend:
      return "wa";
  }
  NOTREACHED();
}

base::expected<OpenContentUriResult, base::File::Error>
OpenContentUriWithModeAndGetFd(const base::FilePath& path,
                               std::string_view mode) {
  CHECK(path.IsContentUri());

  ASSIGN_OR_RETURN(base::android::ScopedJavaLocalRef<jobject> pfd,
                   base::internal::OpenContentUriWithExplicitMode(path, mode));

  int fd = base::internal::ContentUriGetFd(pfd);
  if (fd < 0) {
    base::internal::ContentUriClose(pfd);
    return base::unexpected(base::File::Error::FILE_ERROR_FAILED);
  }
  return OpenContentUriResult{fd,
                              base::android::ScopedJavaGlobalRef<jobject>(pfd)};
}

std::optional<base::files_internal::VirtualDocumentPath> ParseVirtualPath(
    const base::FilePath& path) {
  if (!path.IsVirtualDocumentPath()) {
    return std::nullopt;
  }
  return base::files_internal::VirtualDocumentPath::Parse(path.value());
}

}  // namespace

namespace base {

File::File(
    FilePath path,
    ScopedPlatformFile platform_file,
    base::android::ScopedJavaGlobalRef<jobject> java_parcel_file_descriptor,
    bool async)
    : File(std::move(platform_file), async) {
  path_ = std::move(path);
  java_parcel_file_descriptor_ = std::move(java_parcel_file_descriptor);
}

expected<FilePath, File::Error> ResolveAndroidDocument(
    const FilePath& virtual_path) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  std::optional<files_internal::VirtualDocumentPath> vp =
      ParseVirtualPath(virtual_path);
  if (!vp) {
    return unexpected(File::FILE_ERROR_INVALID_OPERATION);
  }
  ASSIGN_OR_RETURN(files_internal::ResolvedDocument resolved,
                   vp->ResolveDocument());
  if (resolved.is_directory) {
    return unexpected(File::FILE_ERROR_NOT_A_FILE);
  }
  return FilePath(std::move(resolved.content_uri));
}

expected<FilePath, File::Error> CreateAndroidDocument(
    const FilePath& virtual_path,
    std::string_view mime_type) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  std::optional<files_internal::VirtualDocumentPath> vp =
      ParseVirtualPath(virtual_path);
  if (!vp) {
    return unexpected(File::FILE_ERROR_INVALID_OPERATION);
  }
  ASSIGN_OR_RETURN(std::string content_uri, vp->CreateDocument(mime_type));
  return FilePath(std::move(content_uri));
}

File OpenAndroidDocument(const FilePath& content_uri,
                         AndroidDocumentAccessMode access,
                         bool async) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  if (!content_uri.IsContentUri()) {
    return File(File::FILE_ERROR_INVALID_OPERATION);
  }
  auto result =
      OpenContentUriWithModeAndGetFd(content_uri, AccessModeToJavaMode(access));
  if (!result.has_value()) {
    return File(result.error());
  }

  return File(content_uri, ScopedPlatformFile(result->fd),
              std::move(result->java_parcel_file_descriptor), async);
}

namespace files_internal {

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
      return OpenAndroidFileResult(*cu, r->fd, r->java_parcel_file_descriptor,
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
      vp->CreateOrOpen(kUnknownAndroidDocumentMimeType);
  if (!create_result) {
    return base::unexpected(File::Error::FILE_ERROR_NOT_A_DIRECTORY);
  }
  base::FilePath content_uri(create_result->first);
  bool created = create_result->second;

  if (auto r = OpenContentUriAndGetFd(content_uri, flags); r.has_value()) {
    return OpenAndroidFileResult(content_uri, r->fd,
                                 r->java_parcel_file_descriptor, created);
  } else {
    return base::unexpected(r.error());
  }
}

}  // namespace files_internal

}  // namespace base
