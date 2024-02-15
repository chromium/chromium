// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_CHECKUP_LAUNCHER_HELPER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_CHECKUP_LAUNCHER_HELPER_IMPL_H_

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"
#include "components/password_manager/core/browser/password_check_referrer_android.h"

// Helper class used to access the methods of the java class
// PasswordCheckupLauncher from multiple native side locations
class PasswordCheckupLauncherHelperImpl : public PasswordCheckupLauncherHelper {
 public:
  PasswordCheckupLauncherHelperImpl() = default;

  PasswordCheckupLauncherHelperImpl(const PasswordCheckupLauncherHelperImpl&) =
      delete;
  PasswordCheckupLauncherHelperImpl& operator=(
      const PasswordCheckupLauncherHelperImpl&) = delete;

  ~PasswordCheckupLauncherHelperImpl() override;

  // Launch the bulk password check in passwords.google.com
  void LaunchCheckupOnlineWithWindowAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& checkupUrl,
      const base::android::JavaRef<jobject>& windowAndroid) override;

  // Launch the bulk password check on device.
  // If the user is syncing passwords, |account_email| is the email of the
  // account syncing passwords. |account_email| is an empty string if the user
  // isn't syncing passwords.
  void LaunchCheckupOnDevice(
      JNIEnv* env,
      Profile* profile,
      ui::WindowAndroid* window_android,
      password_manager::PasswordCheckReferrerAndroid passwordCheckReferrer,
      std::string account_email) override;

  // Launch the bulk password check in passwords.google.com using an activity
  // rather than a WindowAndroid
  void LaunchCheckupOnlineWithActivity(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& checkupUrl,
      const base::android::JavaRef<jobject>& activity) override;

  // Opens the safety check menu in Chrome Settings.
  void LaunchSafetyCheck(JNIEnv* env,
                         ui::WindowAndroid* windowAndroid) override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_CHECKUP_LAUNCHER_HELPER_IMPL_H_
