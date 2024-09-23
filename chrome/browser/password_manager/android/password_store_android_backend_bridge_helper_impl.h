// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_HELPER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_HELPER_IMPL_H_

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_helper.h"

namespace password_manager {

// Helper class that executes password store backend bridge operations on the
// background thread. All operations are executed sequntally on the same
// physical thread as JNIEnv can not be shared between threads.
// Class does not implement bridge interface as it also handles job id for async
// operations. This class methods should be called from the main thread.
class PasswordStoreAndroidBackendBridgeHelperImpl
    : public PasswordStoreAndroidBackendBridgeHelper {
 public:
  explicit PasswordStoreAndroidBackendBridgeHelperImpl(
      password_manager::IsAccountStore is_account_store);
  PasswordStoreAndroidBackendBridgeHelperImpl(
      base::PassKey<class PasswordStoreAndroidBackendBridgeHelperImplTest>,
      std::unique_ptr<PasswordStoreAndroidBackendReceiverBridge>
          receiver_bridge,
      std::unique_ptr<PasswordStoreAndroidBackendDispatcherBridge>
          dispatcher_bridge);

  PasswordStoreAndroidBackendBridgeHelperImpl(
      PasswordStoreAndroidBackendBridgeHelperImpl&&) = delete;
  PasswordStoreAndroidBackendBridgeHelperImpl(
      const PasswordStoreAndroidBackendBridgeHelperImpl&) = delete;
  PasswordStoreAndroidBackendBridgeHelperImpl& operator=(
      PasswordStoreAndroidBackendBridgeHelperImpl&&) = delete;
  PasswordStoreAndroidBackendBridgeHelperImpl& operator=(
      const PasswordStoreAndroidBackendBridgeHelperImpl&) = delete;
  ~PasswordStoreAndroidBackendBridgeHelperImpl() override;

  // PasswordStoreAndroidBackendBridgeHelper implementation
  bool CanUseGetAffiliatedPasswordsAPI() override;
  bool CanUseGetAllLoginsWithBrandingInfoAPI() override;
  void SetConsumer(base::WeakPtr<Consumer> consumer) override;
  [[nodiscard]] JobId GetAllLogins(std::string account) override;
  [[nodiscard]] JobId GetAllLoginsWithBrandingInfo(
      std::string account) override;
  [[nodiscard]] JobId GetAutofillableLogins(std::string account) override;
  [[nodiscard]] JobId GetLoginsForSignonRealm(const std::string& signon_realm,
                                              std::string account) override;
  [[nodiscard]] JobId GetAffiliatedLoginsForSignonRealm(
      const std::string& signon_realm,
      std::string account) override;
  [[nodiscard]] JobId AddLogin(const password_manager::PasswordForm& form,
                               std::string account) override;
  [[nodiscard]] JobId UpdateLogin(const password_manager::PasswordForm& form,
                                  std::string account) override;
  [[nodiscard]] JobId RemoveLogin(const password_manager::PasswordForm& form,
                                  std::string account) override;

 private:
  JobId GetNextJobId();

  // This object is the proxy to the JNI bridge that handles API callvacks.
  std::unique_ptr<PasswordStoreAndroidBackendReceiverBridge> receiver_bridge_;

  // This object is the proxy to the JNI bridge that dispatch the API requests.
  std::unique_ptr<PasswordStoreAndroidBackendDispatcherBridge>
      dispatcher_bridge_;

  // Background thread pool task runner. Used to execute all backend operations
  // including JNI and/or GMS Core interaction. Limited to a single thread as
  // JNIEnv only suitable for use on a single thread.
  scoped_refptr<base::SingleThreadTaskRunner> background_task_runner_;

  // This member stores the unique ID last used for an API request.
  JobId last_job_id_{0};

  // All methods should be called on the main thread.
  SEQUENCE_CHECKER(main_sequence_checker_);
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_HELPER_IMPL_H_
