// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_HELPER_H_

#include <optional>

#include "chrome/browser/password_manager/android/password_settings_updater_android_receiver_bridge.h"
#include "components/password_manager/core/browser/password_manager_setting.h"

namespace password_manager {

// Interface for the native side of the JNI bridge. Simplifies mocking in tests.
// This helper simplifies bridge usage by executing operations on a proper
// sequence. All methods should be called on the default sequence of the UI
// thread.
class PasswordSettingsUpdaterAndroidBridgeHelper {
 public:
  using SyncingAccount =
      PasswordSettingsUpdaterAndroidReceiverBridge::SyncingAccount;
  using Consumer = PasswordSettingsUpdaterAndroidReceiverBridge::Consumer;

  virtual ~PasswordSettingsUpdaterAndroidBridgeHelper() = default;

  // Factory function for creating the helper. Implementation is pulled in by
  // including an implementation or by defining it explicitly in tests.
  // Ensure `password_manager_android_util::AreMinUpmRequirementsMet`
  // returns true before calling this method.
  static std::unique_ptr<PasswordSettingsUpdaterAndroidBridgeHelper> Create();

  // Sets the `consumer` that is notified on operation completion as defined in
  // `PasswordSettingsUpdaterAndroidReceiverBridge::Consumer`.
  virtual void SetConsumer(base::WeakPtr<Consumer> consumer) = 0;

  // Password settings accessor bridge operations. Each operation is executed
  // asynchronously, the result is reported via consumer callback.
  virtual void GetPasswordSettingValue(std::optional<SyncingAccount> account,
                                       PasswordManagerSetting setting) = 0;

  virtual void SetPasswordSettingValue(std::optional<SyncingAccount> account,
                                       PasswordManagerSetting setting,
                                       bool value) = 0;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_HELPER_H_
