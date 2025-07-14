// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/virtual_document_path.h"

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/virtual_document_path_jni/VirtualDocumentPath_jni.h"

namespace base::android {

VirtualDocumentPath::VirtualDocumentPath(const JavaRef<jobject>& obj) {
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
  ScopedJavaLocalRef<jobject> obj = Java_VirtualDocumentPath_parse(env, path);
  if (obj.is_null()) {
    return std::nullopt;
  }
  return VirtualDocumentPath(obj);
}

std::optional<std::string> VirtualDocumentPath::ResolveToContentUri() const {
  JNIEnv* env = android::AttachCurrentThread();
  std::string uri =
      Java_VirtualDocumentPath_resolveToContentUriString(env, obj_);
  if (uri.empty()) {
    return std::nullopt;
  }
  return uri;
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
  ScopedJavaLocalRef<jbyteArray> bs = ToJavaByteArray(env, data);
  return Java_VirtualDocumentPath_writeFile(env, obj_, bs);
}

}  // namespace base::android
