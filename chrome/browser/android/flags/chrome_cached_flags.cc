// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/flags/chrome_cached_flags.h"


#include "base/android/jni_string.h"
#include "base/feature_list.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeCachedFlags_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace chrome::android {

bool IsJavaDrivenFeatureEnabled(const base::Feature& feature) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_feature_name(
      ConvertUTF8ToJavaString(env, feature.name));
  return Java_ChromeCachedFlags_isEnabled(env, j_feature_name);
}

}  // namespace chrome::android
