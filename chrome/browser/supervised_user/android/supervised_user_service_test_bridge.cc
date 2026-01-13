// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "components/supervised_user/core/browser/android/android_parental_controls.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/supervised_user/test_support_jni_headers/SupervisedUserServiceTestBridge_jni.h"

namespace supervised_user {
static void JNI_SupervisedUserServiceTestBridge_EnableBrowserContentFilters(
    JNIEnv* env) {
  AndroidParentalControls& android_parental_controls =
      static_cast<AndroidParentalControls&>(
          g_browser_process->device_parental_controls());
  android_parental_controls.SetBrowserContentFiltersEnabledForTesting(true);
}

static void JNI_SupervisedUserServiceTestBridge_EnableSearchContentFilters(
    JNIEnv* env) {
  AndroidParentalControls& android_parental_controls =
      static_cast<AndroidParentalControls&>(
          g_browser_process->device_parental_controls());
  android_parental_controls.SetSearchContentFiltersEnabledForTesting(true);
}
}  // namespace supervised_user

DEFINE_JNI(SupervisedUserServiceTestBridge)
