// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/locale_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/LocaleUtils_jni.h"

namespace base {
namespace android {

std::string GetDefaultCountryCode() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_LocaleUtils_getDefaultCountryCode(env);
}

std::string GetDefaultLocaleString() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_LocaleUtils_getDefaultLocaleString(env);
}

std::string GetDefaultLocaleListString() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_LocaleUtils_getDefaultLocaleListString(env);
}

}  // namespace android
}  // namespace base
