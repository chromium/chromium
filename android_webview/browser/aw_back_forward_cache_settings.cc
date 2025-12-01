// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_back_forward_cache_settings.h"

#include <memory>

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwBackForwardCacheSettings_jni.h"

namespace android_webview {

AwBackForwardCacheSettings
AwBackForwardCacheSettings::FromJavaAwBackForwardCacheSettings(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_back_forward_cache_settings) {
  return AwBackForwardCacheSettings(
      Java_AwBackForwardCacheSettings_getTimeoutInSeconds(
          env, java_back_forward_cache_settings),
      Java_AwBackForwardCacheSettings_getMaxPagesInCache(
          env, java_back_forward_cache_settings));
}

AwBackForwardCacheSettings::AwBackForwardCacheSettings(int timeout_in_seconds,
                                                       int max_pages_in_cache)
    : timeout_in_seconds_(timeout_in_seconds),
      max_pages_in_cache_(max_pages_in_cache) {}

}  // namespace android_webview

DEFINE_JNI(AwBackForwardCacheSettings)
