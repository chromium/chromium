// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/test_support_jni_headers/ScreenshotCaptureTestHelper_jni.h"
#include "content/public/test/navigation_transition_test_utils.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::JavaParamRef;

namespace {
base::android::ScopedJavaGlobalRef<jobject> java_handler_for_testing_;
}

namespace gesturenav {

void OnNavScreenshotAvailableForTesting(int nav_entry_index,
                                        const SkBitmap& bitmap,
                                        bool requested) {
  Java_ScreenshotCaptureTestHelper_onNavScreenshotAvailable(
      jni_zero::AttachCurrentThread(), java_handler_for_testing_,
      nav_entry_index,
      bitmap.isNull() ? nullptr : gfx::ConvertToJavaBitmap(bitmap), requested);
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

void JNI_ScreenshotCaptureTestHelper_SetNavScreenshotCallbackForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jhandler) {
  java_handler_for_testing_ =
      base::android::ScopedJavaGlobalRef<jobject>(jhandler);
  content::NavigationTransitionTestUtils::SetNavScreenshotCallbackForTesting(
      base::BindRepeating(OnNavScreenshotAvailableForTesting));
}

}  // namespace gesturenav
