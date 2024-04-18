// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <jni.h>

#include <optional>
#include <string>

#include "base/android/jni_string.h"
#include "base/base_export.h"
#include "base/files/file_path.h"
#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero {

template <>
BASE_EXPORT std::string FromJniType<std::string>(
    JNIEnv* env,
    const JavaRef<jobject>& input) {
  return base::android::ConvertJavaStringToUTF8(
      env, static_cast<jstring>(input.obj()));
}

template <>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniType<std::string>(
    JNIEnv* env,
    const std::string& input) {
  return base::android::ConvertUTF8ToJavaString(env, input);
}

template <>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniType<const char*>(
    JNIEnv* env,
    const char* const& input) {
  return base::android::ConvertUTF8ToJavaString(env, input);
}

template <>
BASE_EXPORT std::u16string FromJniType<std::u16string>(
    JNIEnv* env,
    const JavaRef<jobject>& input) {
  return base::android::ConvertJavaStringToUTF16(
      env, static_cast<jstring>(input.obj()));
}

template <>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniType<std::u16string>(
    JNIEnv* env,
    const std::u16string& input) {
  return base::android::ConvertUTF16ToJavaString(env, input);
}

template <>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniType<std::u16string_view>(
    JNIEnv* env,
    const std::u16string_view& input) {
  return base::android::ConvertUTF16ToJavaString(env, input);
}

template <>
BASE_EXPORT base::FilePath FromJniType<base::FilePath>(
    JNIEnv* env,
    const JavaRef<jobject>& input) {
  return base::FilePath(base::android::ConvertJavaStringToUTF8(
      env, static_cast<jstring>(input.obj())));
}

template <>
BASE_EXPORT ScopedJavaLocalRef<jobject> ToJniType<base::FilePath>(
    JNIEnv* env,
    const base::FilePath& input) {
  return base::android::ConvertUTF8ToJavaString(env, input.value());
}

}  // namespace jni_zero
