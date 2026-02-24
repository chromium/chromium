// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "chrome/android/chrome_jni_headers/GestureNavigationUtils_jni.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"

namespace gesturenav {

// static
static bool JNI_GestureNavigationUtils_ShouldAnimateBackForwardTransitions(
    JNIEnv* env) {
  return content::BackForwardTransitionAnimationManager::
      ShouldAnimateBackForwardTransitions();
}

// static
static jni_zero::ScopedJavaLocalRef<jobject>
JNI_GestureNavigationUtils_SetMinRequiredPhysicalRamMbForTesting(JNIEnv* env,
                                                                 int32_t jMb) {
  auto reset = content::BackForwardTransitionAnimationManager::
      SetMinRequiredPhysicalRamMbForTesting(jMb);
  auto callback =
      base::BindOnce([](base::AutoReset<int> reset) {}, std::move(reset));
  return base::android::ToJniCallback(env, std::move(callback));
}

}  // namespace gesturenav

DEFINE_JNI(GestureNavigationUtils)
