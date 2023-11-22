// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_checkup_launcher_helper_impl.h"

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
    ui::WindowAndroid* windowAndroid,
    password_manager::PasswordCheckReferrerAndroid passwordCheckReferrer,
    std::string account_email) {
  if (!windowAndroid) {
    return;
  }
  // TODO(b/306669939): Pass the |account_email| to Java and launch the
  // appropriate checkup: for the account or local.
  Java_PasswordCheckupLauncher_launchCheckupOnDevice(
      env, windowAndroid->GetJavaObject(),
      static_cast<int>(passwordCheckReferrer));
}

void PasswordCheckupLauncherHelperImpl::LaunchCheckupOnlineWithActivity(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& checkupUrl,
    const base::android::JavaRef<jobject>& activity) {
  Java_PasswordCheckupLauncher_launchCheckupOnlineWithActivity(env, checkupUrl,
                                                               activity);
}
