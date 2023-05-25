// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_checkup_launcher_helper_impl.h"

#include "chrome/android/chrome_jni_headers/PasswordCheckupLauncher_jni.h"

PasswordCheckupLauncherHelperImpl::~PasswordCheckupLauncherHelperImpl() =
    default;

void PasswordCheckupLauncherHelperImpl::LaunchCheckupInAccountWithWindowAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& checkupUrl,
    const base::android::JavaRef<jobject>& windowAndroid) {
  Java_PasswordCheckupLauncher_launchCheckupInAccountWithWindowAndroid(
      env, checkupUrl, windowAndroid);
}

void PasswordCheckupLauncherHelperImpl::LaunchLocalCheckup(
    JNIEnv* env,
    ui::WindowAndroid* windowAndroid,
    password_manager::PasswordCheckReferrerAndroid passwordCheckReferrer) {
  if (!windowAndroid) {
    return;
  }
  Java_PasswordCheckupLauncher_launchLocalCheckup(
      env, windowAndroid->GetJavaObject(),
      static_cast<int>(passwordCheckReferrer));
}

void PasswordCheckupLauncherHelperImpl::LaunchCheckupInAccountWithActivity(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& checkupUrl,
    const base::android::JavaRef<jobject>& activity) {
  Java_PasswordCheckupLauncher_launchCheckupInAccountWithActivity(
      env, checkupUrl, activity);
}
