// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"

// Include last. Requires declarations from includes above.
#include "chrome/browser/supervised_user/android_parental_controls_bridge_jni_headers/AndroidParentalControlsBridge_jni.h"

namespace supervised_user {
static bool JNI_AndroidParentalControlsBridge_IsSupervisedLocally(JNIEnv* env) {
  return g_browser_process->device_parental_controls().IsEnabled();
}
}  // namespace supervised_user

DEFINE_JNI(AndroidParentalControlsBridge)
