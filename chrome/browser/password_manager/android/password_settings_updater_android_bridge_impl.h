// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PasswordSettingsUpdaterAndroidBridgeImpl
    : public password_manager::PasswordSettingsUpdaterAndroidBridge {
 public:
  PasswordSettingsUpdaterAndroidBridgeImpl();
  PasswordSettingsUpdaterAndroidBridgeImpl(
      PasswordSettingsUpdaterAndroidBridgeImpl&&) = delete;
  PasswordSettingsUpdaterAndroidBridgeImpl(
      const PasswordSettingsUpdaterAndroidBridgeImpl&) = delete;
  PasswordSettingsUpdaterAndroidBridgeImpl& operator=(
      PasswordSettingsUpdaterAndroidBridgeImpl&&) = delete;
  PasswordSettingsUpdaterAndroidBridgeImpl& operator=(
      PasswordSettingsUpdaterAndroidBridgeImpl&) = delete;
  ~PasswordSettingsUpdaterAndroidBridgeImpl() override;

  // PasswordSettingsUpdaterAndroidBridge implementation.
  void SetConsumer(base::WeakPtr<Consumer> consumer) override;
  void GetPasswordSettingValue(
      absl::optional<SyncingAccount> account,
      password_manager::PasswordManagerSetting setting) override;
  void SetPasswordSettingValue(absl::optional<SyncingAccount> account,
                               password_manager::PasswordManagerSetting setting,
                               bool value) override;

  // Called via JNI from PasswordSettingsUpdaterAndroidBridge.java
  void OnSettingValueFetched(JNIEnv* env,
                             jint setting,
                             jboolean offerToSavePasswordsEnabled);
  void OnSettingValueAbsent(JNIEnv* env, jint setting);
  void OnSettingFetchingError(JNIEnv* env,
                              jint setting,
                              jint error,
                              jint api_error_code);
  void OnSuccessfulSettingChange(JNIEnv* env, jint setting);
  void OnFailedSettingChange(JNIEnv* env,
                             jint setting,
                             jint error,
                             jint api_error_code);

 private:
  // The consumer to be notified when a setting request finishes.
  base::WeakPtr<Consumer> consumer_;

  // This object is an instance of PasswordSettingsUpdaterBridge, i.e.
  // the Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_IMPL_H_
