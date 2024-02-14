// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <jni.h>
#include <string>

#include "base/android/jni_string.h"
#include "base/base_export.h"
#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero {

template <>
BASE_EXPORT std::string ConvertType<std::string>(
    JNIEnv* env,
    const JavaRef<jstring>& input) {
  return base::android::ConvertJavaStringToUTF8(env, input);
}

template <>
BASE_EXPORT std::u16string ConvertType<std::u16string>(
    JNIEnv* env,
    const JavaRef<jstring>& input) {
  return base::android::ConvertJavaStringToUTF16(env, input);
}

}  // namespace jni_zero
