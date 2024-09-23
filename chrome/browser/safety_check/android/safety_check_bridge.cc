// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/signin/identity_manager_provider.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_impl.h"
#include "components/safety_check/safety_check.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/android/browser_context_handle.h"
#include "content/public/browser/browser_context.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/safety_check/android/jni_headers/SafetyCheckBridge_jni.h"

static jboolean JNI_SafetyCheckBridge_UserSignedIn(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jhandle) {
  return password_manager::LeakDetectionCheckImpl::HasAccountForRequest(
      signin::GetIdentityManagerForBrowserContext(
          content::BrowserContextFromJavaHandle(jhandle)));
}

static jint JNI_SafetyCheckBridge_CheckSafeBrowsing(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jhandle) {
  return static_cast<int>(
      safety_check::CheckSafeBrowsing(user_prefs::UserPrefs::Get(
          content::BrowserContextFromJavaHandle(jhandle))));
}
