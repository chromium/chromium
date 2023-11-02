// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"

#include "chrome/android/chrome_jni_headers/PasswordCheckupLauncher_jni.h"

// static
void PasswordCheckupLauncherHelper::LaunchCheckupInAccountWithWindowAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& checkupUrl,
    const base::android::JavaRef<jobject>& windowAndroid) {
  Java_PasswordCheckupLauncher_launchCheckupInAccountWithWindowAndroid(
      env, checkupUrl, windowAndroid);
}

// static
void PasswordCheckupLauncherHelper::LaunchLocalCheckup(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& windowAndroid,
    password_manager::PasswordCheckReferrerAndroid passwordCheckReferrer) {
  Java_PasswordCheckupLauncher_launchLocalCheckup(
      env, windowAndroid, static_cast<int>(passwordCheckReferrer));
}

// static
void PasswordCheckupLauncherHelper::LaunchCheckupInAccountWithActivity(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& checkupUrl,
    const base::android::JavaRef<jobject>& activity) {
  Java_PasswordCheckupLauncher_launchCheckupInAccountWithActivity(
      env, checkupUrl, activity);
}
