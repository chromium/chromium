// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_cached_flags.h"

#include <string>

#include "android_webview/common/aw_feature_map.h"
#include "base/android/feature_map.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/common_jni/WebViewCachedFlags_jni.h"

namespace android_webview {

bool CachedFlags::IsEnabled(const base::Feature& feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebViewCachedFlags_isFeatureEnabled(env,
                                                  std::string(feature.name));
}

bool CachedFlags::IsCachedFeatureOverridden(const base::Feature& feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebViewCachedFlags_isFeatureOverridden(env,
                                                     std::string(feature.name));
}

static std::optional<bool> JNI_WebViewCachedFlags_GetStateIfOverridden(
    JNIEnv* env,
    std::string& feature_name) {
  base::android::FeatureMap* feature_map = GetFeatureMap();
  const base::Feature* feature =
      feature_map->FindFeatureExposedToJava(feature_name);
  return base::FeatureList::GetStateIfOverridden(*feature);
}

}  // namespace android_webview

DEFINE_JNI(WebViewCachedFlags)
