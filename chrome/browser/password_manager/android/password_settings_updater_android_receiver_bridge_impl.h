// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_RECEIVER_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_RECEIVER_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_receiver_bridge.h"

namespace password_manager {

class PasswordSettingsUpdaterAndroidReceiverBridgeImpl
    : public PasswordSettingsUpdaterAndroidReceiverBridge {
 public:
  PasswordSettingsUpdaterAndroidReceiverBridgeImpl();
  PasswordSettingsUpdaterAndroidReceiverBridgeImpl(
      PasswordSettingsUpdaterAndroidReceiverBridgeImpl&&) = delete;
  PasswordSettingsUpdaterAndroidReceiverBridgeImpl(
      const PasswordSettingsUpdaterAndroidReceiverBridgeImpl&) = delete;
  PasswordSettingsUpdaterAndroidReceiverBridgeImpl& operator=(
      PasswordSettingsUpdaterAndroidReceiverBridgeImpl&&) = delete;
  PasswordSettingsUpdaterAndroidReceiverBridgeImpl& operator=(
      PasswordSettingsUpdaterAndroidReceiverBridgeImpl&) = delete;
  ~PasswordSettingsUpdaterAndroidReceiverBridgeImpl() override;

  // PasswordSettingsUpdaterAndroidReceiverBridge implementation.
  base::android::ScopedJavaGlobalRef<jobject> GetJavaBridge() const override;

  void SetConsumer(base::WeakPtr<Consumer> consumer) override;

  // Called via JNI from PasswordSettingsUpdaterAndroidReceiverBridge.java
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

  // All callbacks should be called on the default UI sequence.
  SEQUENCE_CHECKER(main_sequence_checker_);
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_RECEIVER_BRIDGE_IMPL_H_
