// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_checkup_launcher_helper_impl.h"

#include "chrome/browser/profiles/profile.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/PasswordCheckupLauncher_jni.h"

PasswordCheckupLauncherHelperImpl::~PasswordCheckupLauncherHelperImpl() =
    default;

void PasswordCheckupLauncherHelperImpl::LaunchCheckupOnlineWithWindowAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& checkupUrl,
    const base::android::JavaRef<jobject>& windowAndroid) {
  Java_PasswordCheckupLauncher_launchCheckupOnlineWithWindowAndroid(
      env, checkupUrl, windowAndroid);
}

void PasswordCheckupLauncherHelperImpl::LaunchCheckupOnDevice(
    JNIEnv* env,
    Profile* profile,
    ui::WindowAndroid* windowAndroid,
    password_manager::PasswordCheckReferrerAndroid passwordCheckReferrer,
    std::string account_email) {
  if (!windowAndroid) {
    return;
  }
  Java_PasswordCheckupLauncher_launchCheckupOnDevice(
      env, profile->GetJavaObject(), windowAndroid->GetJavaObject(),
      static_cast<int>(passwordCheckReferrer),
      account_email.empty()
          ? nullptr
          : base::android::ConvertUTF8ToJavaString(env, account_email));
}

void PasswordCheckupLauncherHelperImpl::LaunchCheckupOnlineWithActivity(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& checkupUrl,
    const base::android::JavaRef<jobject>& activity) {
  Java_PasswordCheckupLauncher_launchCheckupOnlineWithActivity(env, checkupUrl,
                                                               activity);
}

void PasswordCheckupLauncherHelperImpl::LaunchSafetyCheck(
    JNIEnv* env,
    ui::WindowAndroid* windowAndroid) {
  if (windowAndroid == nullptr) {
    return;
  }
  Java_PasswordCheckupLauncher_launchSafetyCheck(
      env, windowAndroid->GetJavaObject());
}
