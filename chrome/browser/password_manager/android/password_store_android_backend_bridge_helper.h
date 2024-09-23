// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_HELPER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_receiver_bridge.h"

namespace password_manager {

// Interface for the native side of the JNI bridge. Simplifies mocking in tests.
// This helper simplifies bridge usage by managing unique job ids and executing
// operations on a proper sequence. All methods should be called on the default
// sequence of the UI thread.
class PasswordStoreAndroidBackendBridgeHelper {
 public:
  using JobId = PasswordStoreAndroidBackendReceiverBridge::JobId;
  using Consumer = PasswordStoreAndroidBackendReceiverBridge::Consumer;

  virtual ~PasswordStoreAndroidBackendBridgeHelper() = default;

  // Factory function for creating the helper. Implementation is pulled in by
  // including an implementation or by defining it explicitly in tests.
  // Ensure `password_manager_android_util::IsUpmBackendForAccountAvailable`
  // returns true before calling this method.
  static std::unique_ptr<PasswordStoreAndroidBackendBridgeHelper> Create(
      password_manager::IsAccountStore is_account_store);

  // Returns true if GMS Core supports new GetAffiliatedPasswordsAPI API.
  virtual bool CanUseGetAffiliatedPasswordsAPI() = 0;

  // Returns true if GMS Core supports new GetAllLoginsWithBrandingInfo API.
  virtual bool CanUseGetAllLoginsWithBrandingInfoAPI() = 0;

  // Sets the `consumer` that is notified on job completion as defined in
  // `PasswordStoreAndroidBackendReceiverBridge::Consumer`.
  virtual void SetConsumer(base::WeakPtr<Consumer> consumer) = 0;

  // Password store backend dispatcher bridge operations. Each operation is
  // executed asynchronously and could be uniquely identified within the bridge
  // helper instance using the returned JobId.
  [[nodiscard]] virtual JobId GetAllLogins(std::string account) = 0;
  [[nodiscard]] virtual JobId GetAllLoginsWithBrandingInfo(
      std::string Account) = 0;
  [[nodiscard]] virtual JobId GetAutofillableLogins(std::string account) = 0;
  [[nodiscard]] virtual JobId GetLoginsForSignonRealm(
      const std::string& signon_realm,
      std::string account) = 0;
  [[nodiscard]] virtual JobId GetAffiliatedLoginsForSignonRealm(
      const std::string& signon_realm,
      std::string account) = 0;
  [[nodiscard]] virtual JobId AddLogin(
      const password_manager::PasswordForm& form,
      std::string account) = 0;
  [[nodiscard]] virtual JobId UpdateLogin(
      const password_manager::PasswordForm& form,
      std::string account) = 0;
  [[nodiscard]] virtual JobId RemoveLogin(
      const password_manager::PasswordForm& form,
      std::string account) = 0;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_HELPER_H_
