// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flags/android/cached_feature_flags.h"

#include "chrome/browser/flags/jni_headers/CachedFeatureFlags_jni.h"

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "content/public/common/content_features.h"
#include "content/public/common/network_service_util.h"

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
  return Java_CachedFeatureFlags_isEnabled(env, j_feature_name);
}

std::string GetReachedCodeProfilerTrialGroup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> group =
      Java_CachedFeatureFlags_getReachedCodeProfilerTrialGroup(env);
  return ConvertJavaStringToUTF8(env, group);
}

}  // namespace android
}  // namespace chrome

static jboolean JNI_CachedFeatureFlags_IsNetworkServiceWarmUpEnabled(
    JNIEnv* env) {
  return content::IsOutOfProcessNetworkService() &&
         base::FeatureList::IsEnabled(features::kWarmUpNetworkProcess);
}
