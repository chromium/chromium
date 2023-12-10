// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_HELPER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_HELPER_IMPL_H_

#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_bridge_helper.h"
#include "chrome/browser/password_manager/android/password_settings_updater_android_dispatcher_bridge.h"

namespace password_manager {

// Helper class that executes password accessor bridge operations on the
// background thread. All operations are executed sequentally on the same
// physical thread as JNIEnv can not be shared between threads.
// This class methods should be called from the UI thread.
class PasswordSettingsUpdaterAndroidBridgeHelperImpl
    : public PasswordSettingsUpdaterAndroidBridgeHelper {
 public:
  PasswordSettingsUpdaterAndroidBridgeHelperImpl();
  PasswordSettingsUpdaterAndroidBridgeHelperImpl(
      base::PassKey<class PasswordSettingsUpdaterAndroidBridgeHelperImplTest>,
      std::unique_ptr<PasswordSettingsUpdaterAndroidReceiverBridge>
          receiver_bridge,
      std::unique_ptr<PasswordSettingsUpdaterAndroidDispatcherBridge>
          dispatcher_bridge);

  PasswordSettingsUpdaterAndroidBridgeHelperImpl(
      PasswordSettingsUpdaterAndroidBridgeHelperImpl&&) = delete;
  PasswordSettingsUpdaterAndroidBridgeHelperImpl(
      const PasswordSettingsUpdaterAndroidBridgeHelperImpl&) = delete;
  PasswordSettingsUpdaterAndroidBridgeHelperImpl& operator=(
      PasswordSettingsUpdaterAndroidBridgeHelperImpl&&) = delete;
  PasswordSettingsUpdaterAndroidBridgeHelperImpl& operator=(
      const PasswordSettingsUpdaterAndroidBridgeHelperImpl&) = delete;
  ~PasswordSettingsUpdaterAndroidBridgeHelperImpl() override;

  // PasswordSettingsUpdaterAndroidBridgeHelper implementation
  void SetConsumer(base::WeakPtr<Consumer> consumer) override;
  void GetPasswordSettingValue(std::optional<SyncingAccount> account,
                               PasswordManagerSetting setting) override;

  void SetPasswordSettingValue(std::optional<SyncingAccount> account,
                               PasswordManagerSetting setting,
                               bool value) override;

 private:
  // This object is the proxy to the JNI bridge that handles API callbacks.
  std::unique_ptr<PasswordSettingsUpdaterAndroidReceiverBridge>
      receiver_bridge_;

  // This object is the proxy to the JNI bridge that dispatch the API requests.
  std::unique_ptr<PasswordSettingsUpdaterAndroidDispatcherBridge>
      dispatcher_bridge_;

  // Background thread pool task runner. Used to execute all backend operations
  // including JNI and/or GMS Core interaction. Limited to a single thread as
  // JNIEnv only suitable for use on a single thread.
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;

  // All methods should be called on the main thread.
  SEQUENCE_CHECKER(main_sequence_checker_);
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_ANDROID_BRIDGE_HELPER_IMPL_H_
