// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/signin/signin_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/SigninUtils_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "ui/android/window_android.h"

using base::android::JavaParamRef;

// static
void SigninUtils::OpenAccountManagementScreen(
    ui::WindowAndroid* window,
    signin::GAIAServiceType service_type,
    const std::string& email) {
  DCHECK(window);
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SigninUtils_openAccountManagementScreen(
      env, window->GetJavaObject(), static_cast<int>(service_type),
      email.empty() ? nullptr
                    : base::android::ConvertUTF8ToJavaString(env, email));
}

static void JNI_SigninUtils_LogEvent(JNIEnv* env,
                                     jint metric,
                                     jint gaiaServiceType) {
  ProfileMetrics::LogProfileAndroidAccountManagementMenu(
      static_cast<ProfileMetrics::ProfileAndroidAccountManagementMenu>(metric),
      static_cast<signin::GAIAServiceType>(gaiaServiceType));
}
