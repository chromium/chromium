// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/aw_cached_flags.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/common_jni/WebViewCachedFlags_jni.h"

bool android_webview::CachedFlags::IsEnabled(const base::Feature& feature) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_WebViewCachedFlags_isFeatureEnabled(env,
                                                  std::string(feature.name));
}
