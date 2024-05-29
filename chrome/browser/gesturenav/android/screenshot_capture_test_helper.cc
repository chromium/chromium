// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/navigation_transition_test_utils.h"
#include "ui/gfx/android/java_bitmap.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/test_support_jni_headers/ScreenshotCaptureTestHelper_jni.h"

using base::android::JavaParamRef;

namespace {
base::android::ScopedJavaGlobalRef<jobject> java_handler_for_testing_;
}

namespace gesturenav {

void OnNavScreenshotAvailableForTesting(int nav_entry_index,
                                        const SkBitmap& bitmap,
                                        bool requested,
                                        SkBitmap& out_override_bitmap) {
  base::android::ScopedJavaLocalRef<jobject> override_bitmap =
      Java_ScreenshotCaptureTestHelper_onNavScreenshotAvailable(
          jni_zero::AttachCurrentThread(), java_handler_for_testing_,
          nav_entry_index,
          bitmap.isNull() ? nullptr : gfx::ConvertToJavaBitmap(bitmap),
          requested);
  if (!override_bitmap.is_null()) {
    gfx::JavaBitmap java_bitmap(override_bitmap);
    SkBitmap skbitmap = gfx::CreateSkBitmapFromJavaBitmap(java_bitmap);
    out_override_bitmap = skbitmap;
  }
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
