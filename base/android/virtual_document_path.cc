// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/virtual_document_path.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/virtual_document_path_jni/VirtualDocumentPath_jni.h"

namespace base::files_internal {

VirtualDocumentPath::VirtualDocumentPath(
    const base::android::JavaRef<jobject>& obj) {
  obj_.Reset(obj);
}

VirtualDocumentPath::VirtualDocumentPath(const VirtualDocumentPath& path) =
    default;
VirtualDocumentPath& VirtualDocumentPath::operator=(
    const VirtualDocumentPath& path) = default;

VirtualDocumentPath::~VirtualDocumentPath() = default;

std::optional<VirtualDocumentPath> VirtualDocumentPath::Parse(
    const std::string& path) {
  JNIEnv* env = android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj =
      Java_VirtualDocumentPath_parse(env, path);
  if (obj.is_null()) {
    return std::nullopt;
  }
  return VirtualDocumentPath(obj);
}

std::optional<std::string> VirtualDocumentPath::ResolveToContentUri() const {
  auto resolved = ResolveDocument();
  if (!resolved.has_value() || resolved->content_uri.empty()) {
    return std::nullopt;
  }
  return std::move(resolved->content_uri);
}

base::expected<ResolvedDocument, base::File::Error>
VirtualDocumentPath::ResolveDocument() const {
  JNIEnv* env = android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> result =
      Java_VirtualDocumentPath_resolveDocument(env, obj_);
  if (result.is_null()) {
    return base::unexpected(base::File::FILE_ERROR_FAILED);
  }
  base::File::Error error = Java_ResolveResult_getStatus(env, result);
  if (error != base::File::FILE_OK) {
    return base::unexpected(error);
  }
  ResolvedDocument resolved;
  resolved.content_uri = Java_ResolveResult_getContentUriString(env, result);
  resolved.is_directory = Java_ResolveResult_isDirectory(env, result);
  if (resolved.content_uri.empty()) {
    return base::unexpected(base::File::FILE_ERROR_FAILED);
  }
  return resolved;
}

std::string VirtualDocumentPath::ToString() const {
  JNIEnv* env = android::AttachCurrentThread();
  return Java_VirtualDocumentPath_toString(env, obj_);
}

bool VirtualDocumentPath::Mkdir(mode_t mode) const {
  JNIEnv* env = android::AttachCurrentThread();
  return Java_VirtualDocumentPath_mkdir(env, obj_);
}

bool VirtualDocumentPath::WriteFile(span<const uint8_t> data) const {
  JNIEnv* env = android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> bs =
      base::android::ToJavaByteArray(env, data);
  return Java_VirtualDocumentPath_writeFile(env, obj_, bs);
}

base::expected<std::string, base::File::Error>
VirtualDocumentPath::CreateDocument(std::string_view mime_type) const {
  CHECK(!mime_type.empty());
  JNIEnv* env = android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> result =
      Java_VirtualDocumentPath_createDocument(env, obj_, mime_type);
  if (result.is_null()) {
    return base::unexpected(base::File::FILE_ERROR_FAILED);
  }
  base::File::Error error = Java_CreateDocumentResult_getStatus(env, result);
  if (error != base::File::FILE_OK) {
    return base::unexpected(error);
  }
  std::string uri = Java_CreateDocumentResult_getContentUriString(env, result);
  if (uri.empty()) {
    return base::unexpected(base::File::FILE_ERROR_FAILED);
  }
  return uri;
}

std::optional<std::pair<std::string, bool>> VirtualDocumentPath::CreateOrOpen(
    std::string_view mime_type) const {
  auto resolved = ResolveDocument();
  if (resolved.has_value()) {
    if (resolved->is_directory) {
      return std::nullopt;
    }
    return std::make_pair(std::move(resolved->content_uri), false);
  }
  if (resolved.error() != base::File::FILE_ERROR_NOT_FOUND) {
    return std::nullopt;
  }
  auto created = CreateDocument(mime_type);
  if (!created.has_value()) {
    return std::nullopt;
  }
  return std::make_pair(std::move(*created), true);
}

}  // namespace base::files_internal

DEFINE_JNI(VirtualDocumentPath)
