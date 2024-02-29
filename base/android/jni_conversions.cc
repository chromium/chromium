// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <jni.h>

#include <optional>
#include <string>

#include "base/android/jni_string.h"
#include "base/base_export.h"
#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero {

template <>
BASE_EXPORT std::string FromJniType<std::string>(
    JNIEnv* env,
    const JavaRef<jstring>& input) {
  return base::android::ConvertJavaStringToUTF8(env, input);
}

template <>
BASE_EXPORT ScopedJavaLocalRef<jstring> ToJniType<std::string>(
    JNIEnv* env,
    const std::string& input) {
  return base::android::ConvertUTF8ToJavaString(env, input);
}

template <>
BASE_EXPORT std::u16string FromJniType<std::u16string>(
    JNIEnv* env,
    const JavaRef<jstring>& input) {
  return base::android::ConvertJavaStringToUTF16(env, input);
}

template <>
BASE_EXPORT ScopedJavaLocalRef<jstring> ToJniType<std::u16string>(
    JNIEnv* env,
    const std::u16string& input) {
  return base::android::ConvertUTF16ToJavaString(env, input);
}

// Specialized conversions for std::optional<std::basic_string<T>> since jstring
// is a nullable type but std::basic_string<T> is not.
template <>
BASE_EXPORT std::optional<std::string> FromJniType<std::optional<std::string>>(
    JNIEnv* env,
    const JavaRef<jstring>& j_string) {
  if (!j_string) {
    return std::nullopt;
  }
  return std::optional<std::string>(FromJniType<std::string>(env, j_string));
}

template <>
BASE_EXPORT std::optional<std::u16string>
FromJniType<std::optional<std::u16string>>(JNIEnv* env,
                                           const JavaRef<jstring>& j_string) {
  if (!j_string) {
    return std::nullopt;
  }
  return std::optional<std::u16string>(
      FromJniType<std::u16string>(env, j_string));
}

}  // namespace jni_zero
