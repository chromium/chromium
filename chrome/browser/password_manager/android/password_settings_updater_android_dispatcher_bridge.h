// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_DISPATCHER_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_DISPATCHER_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_receiver_bridge.h"
#include "components/password_manager/core/browser/password_manager_setting.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

class PasswordSettingsUpdaterAndroidDispatcherBridge {
 public:
  using SyncingAccount =
      PasswordSettingsUpdaterAndroidReceiverBridge::SyncingAccount;

  virtual ~PasswordSettingsUpdaterAndroidDispatcherBridge() = default;

  // Perform bridge and Java counterpart initialization.
  // `receiver_bridge` is the java counterpart of the
  // `PasswordSettingsUpdaterAndroidReceiverBridge` and should outlive this
  // object.
  virtual void Init(
      base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) = 0;

  // Asynchronously requests the value of `setting` from Google Mobile Services.
  // If `account` is not present, the value will be requested from the local
  // profile (i.e. a profile not tied to any account).
  virtual void GetPasswordSettingValue(absl::optional<SyncingAccount> account,
                                       PasswordManagerSetting setting) = 0;

  // Asynchronously sets the `value` of `setting` in Google Mobile Services.
  // If `account` is not present, the value will be set in the local profile
  // (i.e. a profile not tied to any account).
  virtual void SetPasswordSettingValue(absl::optional<SyncingAccount> account,
                                       PasswordManagerSetting setting,
                                       bool value) = 0;
  // Method that checks whether the settings accessor can be created or whether
  // `Create` would fail. It returns true iff all nontransient prerequisistes
  // are fulfilled. E.g. if the accessor requires a minimum GMS version this
  // method would return false.
  static bool CanCreateAccessor();

  // Factory function for creating the bridge. Before calling create, ensure
  // that `CanCreateAccessor` returns true.
  static std::unique_ptr<PasswordSettingsUpdaterAndroidDispatcherBridge>
  Create();
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_DISPATCHER_BRIDGE_H_
