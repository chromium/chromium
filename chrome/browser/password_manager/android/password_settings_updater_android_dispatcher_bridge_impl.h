// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_DISPATCHER_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_DISPATCHER_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_dispatcher_bridge.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

class PasswordSettingsUpdaterAndroidDispatcherBridgeImpl
    : public PasswordSettingsUpdaterAndroidDispatcherBridge {
 public:
  PasswordSettingsUpdaterAndroidDispatcherBridgeImpl();
  PasswordSettingsUpdaterAndroidDispatcherBridgeImpl(
      PasswordSettingsUpdaterAndroidDispatcherBridgeImpl&&) = delete;
  PasswordSettingsUpdaterAndroidDispatcherBridgeImpl(
      const PasswordSettingsUpdaterAndroidDispatcherBridgeImpl&) = delete;
  PasswordSettingsUpdaterAndroidDispatcherBridgeImpl& operator=(
      PasswordSettingsUpdaterAndroidDispatcherBridgeImpl&&) = delete;
  PasswordSettingsUpdaterAndroidDispatcherBridgeImpl& operator=(
      PasswordSettingsUpdaterAndroidDispatcherBridgeImpl&) = delete;
  ~PasswordSettingsUpdaterAndroidDispatcherBridgeImpl() override;

  void Init(
      base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) override;

  // PasswordSettingsUpdaterAndroidDispatcherBridge implementation.
  void GetPasswordSettingValue(absl::optional<SyncingAccount> account,
                               PasswordManagerSetting setting) override;
  void SetPasswordSettingValue(absl::optional<SyncingAccount> account,
                               PasswordManagerSetting setting,
                               bool value) override;

 private:
  // This object is an instance of PasswordSettingsUpdaterDispatcherBridge, i.e.
  // the Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_DISPATCHER_BRIDGE_IMPL_H_
