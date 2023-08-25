// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/flags/chrome_cached_flags.h"

#include "chrome/android/chrome_jni_headers/ChromeCachedFlags_jni.h"

#include "base/android/jni_string.h"
#include "base/feature_list.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace chrome {
namespace android {

bool IsJavaDrivenFeatureEnabled(const base::Feature& feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_feature_name(
      ConvertUTF8ToJavaString(env, feature.name));
  return Java_ChromeCachedFlags_isEnabled(env, j_feature_name);
}

std::string GetReachedCodeProfilerTrialGroup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> group =
      Java_ChromeCachedFlags_getReachedCodeProfilerTrialGroup(env);
  return ConvertJavaStringToUTF8(env, group);
}

}  // namespace android
}  // namespace chrome
