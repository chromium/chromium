// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_CHECKUP_LAUNCHER_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_CHECKUP_LAUNCHER_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/password_manager/core/browser/password_check_referrer_android.h"

// Helper class used to access the methods of the java class
// PasswordCheckupLauncher from multiple native side locations
class PasswordCheckupLauncherHelper {
 public:
  // Launch the bulk password check in the Google Account
  static void LaunchCheckupInAccountWithWindowAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& checkupUrl,
      const base::android::JavaRef<jobject>& windowAndroid);

  // Launch the bulk password check locally
  static void LaunchLocalCheckup(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& windowAndroid,
      password_manager::PasswordCheckReferrerAndroid passwordCheckReferrer);

  // Launch the bulk password check in the Google Account using an activity
  // rather than a WindowAndroid
  static void LaunchCheckupInAccountWithActivity(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& checkupUrl,
      const base::android::JavaRef<jobject>& activity);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_CHECKUP_LAUNCHER_HELPER_H_
