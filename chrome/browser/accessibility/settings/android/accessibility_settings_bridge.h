// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_SETTINGS_ANDROID_ACCESSIBILITY_SETTINGS_BRIDGE_H_
#define CHROME_BROWSER_ACCESSIBILITY_SETTINGS_ANDROID_ACCESSIBILITY_SETTINGS_BRIDGE_H_

#include "base/android/jni_android.h"

namespace accessibility {

// JNI bridge for the caret browsing feature.
class AccessibilitySettingsBridge {
 public:
  static jboolean IsCaretBrowsingEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_profile);
  static void SetCaretBrowsingEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_profile,
      jboolean enabled);
};

}  // namespace accessibility

#endif  // CHROME_BROWSER_ACCESSIBILITY_SETTINGS_ANDROID_ACCESSIBILITY_SETTINGS_BRIDGE_H_
