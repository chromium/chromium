// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>
#include <memory>

#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordChangeSuccessTrackerBridge_jni.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "url/android/gurl_android.h"

using password_manager::PasswordChangeSuccessTracker;

namespace {
// Wraps the call to the factory function to obtain the `KeyedService`
// instance for `PasswordChangeSuccessTracker`.
PasswordChangeSuccessTracker* GetPasswordChangeSuccessTracker() {
  return password_manager::PasswordChangeSuccessTrackerFactory::GetInstance()
      ->GetForBrowserContext(ProfileManager::GetLastUsedProfile());
}

}  // namespace

// Called by Java to register the start of a manual password change flow.
void JNI_PasswordChangeSuccessTrackerBridge_OnManualPasswordChangeStarted(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& url,
    const base::android::JavaParamRef<jstring>& username) {
  std::unique_ptr<GURL> native_gurl = url::GURLAndroid::ToNativeGURL(env, url);
  if (!native_gurl->is_empty()) {
    GetPasswordChangeSuccessTracker()->OnManualChangePasswordFlowStarted(
        *native_gurl, ConvertJavaStringToUTF8(env, username),
        PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);
  }
}
