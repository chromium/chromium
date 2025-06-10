// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "chrome/browser/profiles/profile.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/supervised_user/test_support_jni_headers/SupervisedUserPreferencesTestBridge_jni.h"

namespace supervised_user {
void JNI_SupervisedUserPreferencesTestBridge_EnableBrowserContentFilters(
    JNIEnv* env,
    Profile* profile) {
  EnableBrowserContentFilters(*profile->GetPrefs());
}
}  // namespace supervised_user
