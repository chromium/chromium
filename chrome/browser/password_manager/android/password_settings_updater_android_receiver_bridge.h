// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_RECEIVER_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_RECEIVER_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_api_error_codes.h"
#include "components/password_manager/core/browser/password_manager_setting.h"

namespace password_manager {

class PasswordSettingsUpdaterAndroidReceiverBridge {
 public:
  using SyncingAccount =
      base::StrongAlias<struct SyncingAccountTag, std::string>;

  // Each bridge is created with a consumer that will be called when a setting
  // request is completed.
  class Consumer {
   public:
    virtual ~Consumer() = default;

    // Asynchronous response called when the `value` for `setting` has been
    // retrieved.
    virtual void OnSettingValueFetched(PasswordManagerSetting setting,
                                       bool value) = 0;

    // Asynchronous response called if there is no explicit value set for
    // `setting`.
    virtual void OnSettingValueAbsent(PasswordManagerSetting setting) = 0;

    // Asynchronous response called if there was an error while fetching setting
    // value.
    virtual void OnSettingFetchingError(
        PasswordManagerSetting setting,
        AndroidBackendAPIErrorCode api_error_code) = 0;

    // Asynchronous response called after setting value was set successfully.
    virtual void OnSuccessfulSettingChange(PasswordManagerSetting setting) = 0;

    // Asynchronous response called if there call to change setting value
    // failed.
    virtual void OnFailedSettingChange(
        PasswordManagerSetting setting,
        AndroidBackendAPIErrorCode api_error_code) = 0;
  };

  virtual ~PasswordSettingsUpdaterAndroidReceiverBridge() = default;

  // Returns reference to the Java JNI bridge object.
  virtual base::android::ScopedJavaGlobalRef<jobject> GetJavaBridge() const = 0;

  // Sets the consumer to be called when a setting request finishes.
  virtual void SetConsumer(base::WeakPtr<Consumer> consumer) = 0;

  // Factory function for creating the bridge.
  static std::unique_ptr<PasswordSettingsUpdaterAndroidReceiverBridge> Create();
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_RECEIVER_BRIDGE_H_
