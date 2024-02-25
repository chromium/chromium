// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_CHECKUP_LAUNCHER_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_CHECKUP_LAUNCHER_HELPER_H_

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/password_manager/core/browser/password_check_referrer_android.h"
#include "ui/android/window_android.h"

class Profile;

// Helper class used to access the methods of the java class
// `PasswordCheckupLauncher` from multiple native side locations.
// This interface is provided as a convenience for testing.
class PasswordCheckupLauncherHelper {
 public:
  PasswordCheckupLauncherHelper() = default;

  PasswordCheckupLauncherHelper(const PasswordCheckupLauncherHelper&) = delete;
  PasswordCheckupLauncherHelper& operator=(
      const PasswordCheckupLauncherHelper&) = delete;

  virtual ~PasswordCheckupLauncherHelper() = 0;

  // Launch the bulk password check in passwords.google.com
  virtual void LaunchCheckupOnlineWithWindowAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& checkupUrl,
      const base::android::JavaRef<jobject>& windowAndroid) = 0;

  // Launch the bulk password check on device.
  // If the user is syncing passwords, |account_email| is the email of the
  // account syncing passwords. |account_email| is an empty string if the user
  // isn't syncing passwords.
  // |account_email| is used to determine whether to display the password check
  // UI for account passwords or for local passwords.
  virtual void LaunchCheckupOnDevice(
      JNIEnv* env,
      Profile* profile,
      ui::WindowAndroid* windowAndroid,
      password_manager::PasswordCheckReferrerAndroid passwordCheckReferrer,
      std::string account_email) = 0;

  // Launch the bulk password check in passwords.google.com using an activity
  // rather than a WindowAndroid
  virtual void LaunchCheckupOnlineWithActivity(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& checkupUrl,
      const base::android::JavaRef<jobject>& activity) = 0;

  // Opens the safety check menu in Chrome Settings.
  virtual void LaunchSafetyCheck(JNIEnv* env,
                                 ui::WindowAndroid* windowAndroid) = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_CHECKUP_LAUNCHER_HELPER_H_
