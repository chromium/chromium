// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BACK_FORWARD_CACHE_SETTINGS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BACK_FORWARD_CACHE_SETTINGS_H_

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"

namespace android_webview {

class AwBackForwardCacheSettings {
 public:
  static AwBackForwardCacheSettings FromJavaAwBackForwardCacheSettings(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& java_back_forward_cache_settings);

  AwBackForwardCacheSettings(int timeout_in_seconds, int max_pages_in_cache);
  ~AwBackForwardCacheSettings() = default;

  int timeout_in_seconds() const { return timeout_in_seconds_; }
  int max_pages_in_cache() const { return max_pages_in_cache_; }

 private:
  const int timeout_in_seconds_;
  const int max_pages_in_cache_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BACK_FORWARD_CACHE_SETTINGS_H_
