// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_CHECKUP_LAUNCHER_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_CHECKUP_LAUNCHER_HELPER_H_

#include "chrome/browser/password_manager/android/password_checkup_launcher_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/android/window_android.h"

class MockPasswordCheckupLauncherHelper : public PasswordCheckupLauncherHelper {
 public:
  MockPasswordCheckupLauncherHelper();
  ~MockPasswordCheckupLauncherHelper() override;
  MOCK_METHOD(void,
              LaunchCheckupOnlineWithWindowAndroid,
              (JNIEnv*,
               const base::android::JavaRef<jstring>&,
               const base::android::JavaRef<jobject>&),
              (override));
  MOCK_METHOD(void,
              LaunchCheckupOnDevice,
              (JNIEnv*,
               Profile*,
               ui::WindowAndroid*,
               password_manager::PasswordCheckReferrerAndroid,
               std::string account_email),
              (override));
  MOCK_METHOD(void,
              LaunchCheckupOnlineWithActivity,
              (JNIEnv*,
               const base::android::JavaRef<jstring>&,
               const base::android::JavaRef<jobject>&),
              (override));
  MOCK_METHOD(void,
              LaunchSafetyCheck,
              (JNIEnv*, ui::WindowAndroid*),
              (override));
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_CHECKUP_LAUNCHER_HELPER_H_
