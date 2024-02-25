// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_DISPATCHER_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_DISPATCHER_BRIDGE_IMPL_H_

#include <optional>

#include "base/android/scoped_java_ref.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_dispatcher_bridge.h"

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
  void GetPasswordSettingValue(std::optional<SyncingAccount> account,
                               PasswordManagerSetting setting) override;
  void SetPasswordSettingValue(std::optional<SyncingAccount> account,
                               PasswordManagerSetting setting,
                               bool value) override;

 private:
  // This object is an instance of PasswordSettingsUpdaterDispatcherBridge, i.e.
  // the Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_
      GUARDED_BY_CONTEXT(thread_checker_);

  // All operations should be called on the same background thread.
  // As sequence does not guarantee execution on the same thread in general,
  // a `SEQUENCE_CHECKER` is not sufficient here.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_DISPATCHER_BRIDGE_IMPL_H_
