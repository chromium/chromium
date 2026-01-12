// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"

// Include last. Requires declarations from includes above.
#include "chrome/browser/supervised_user/supervised_user_service_bridge_jni_headers/SupervisedUserServiceBridge_jni.h"

namespace supervised_user {
static bool JNI_SupervisedUserServiceBridge_IsSupervisedLocally(
    JNIEnv* env,
    Profile* profile) {
  if (profile->IsIncognitoProfile()) {
    return false;
  }
  return !IsSubjectToParentalControls(*profile->GetPrefs()) &&
         g_browser_process->device_parental_controls().IsEnabled();
}
}  // namespace supervised_user

DEFINE_JNI(SupervisedUserServiceBridge)
