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

  // Launch the bulk password check in the Google Account
  void LaunchCheckupInAccountWithWindowAndroid(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& checkupUrl,
      const base::android::JavaRef<jobject>& windowAndroid) override;

  // Launch the bulk password check locally
  void LaunchLocalCheckup(JNIEnv* env,
                          ui::WindowAndroid* window_android,
                          password_manager::PasswordCheckReferrerAndroid
                              passwordCheckReferrer) override;

  // Launch the bulk password check in the Google Account using an activity
  // rather than a WindowAndroid
  void LaunchCheckupInAccountWithActivity(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& checkupUrl,
      const base::android::JavaRef<jobject>& activity) override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_CHECKUP_LAUNCHER_HELPER_IMPL_H_
