// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_H_

#include "base/types/strong_alias.h"
#include "chrome/browser/password_manager/android/password_manager_setting.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {

class PasswordSettingsUpdaterAndroidBridge {
 public:
  using SyncingAccount =
      base::StrongAlias<struct SyncingAccountTag, std::string>;

  virtual ~PasswordSettingsUpdaterAndroidBridge() = default;

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

  // Factory function for creating the bridge. Before calling create, ensure
  // that `CanCreateAccessor` returns true.
  static std::unique_ptr<PasswordSettingsUpdaterAndroidBridge> Create();
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_H_
