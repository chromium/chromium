// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_H_

#include <vector>

#include "chrome/browser/password_manager/android/password_store_android_backend_consumer_bridge.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

// Interface for the native side of the JNI bridge. Simplifies mocking in tests.
// Implementers of this class are expected to be a minimal layer of glue code
// that performs API call on the Java side to Google Mobile for each operation.
// Any logic beyond data conversion should either live in
// `PasswordStoreAndroidBackend` or a component that is used by the java-side of
// this bridge. Consumer bridge is defined separately in
// `PasswordStoreAndroidBackendConsumerBridge`.
class PasswordStoreAndroidBackendBridge {
 public:
  using SyncingAccount =
      PasswordStoreAndroidBackendConsumerBridge::SyncingAccount;
  using Account = PasswordStoreAndroidBackendConsumerBridge::Account;
  using JobId = PasswordStoreAndroidBackendConsumerBridge::JobId;

  virtual ~PasswordStoreAndroidBackendBridge() = default;

  // Triggers an asynchronous request to retrieve all stored passwords. The
  // registered `Consumer` is notified with `OnCompleteWithLogins` when the
  // job with the returned JobId succeeds. `syncing_account` is used to decide
  // which storage to use. If `syncing_account` is absl::nullopt local storage
  // will be used.
  [[nodiscard]] virtual JobId GetAllLogins(Account account) = 0;

  // Triggers an asynchronous request to retrieve all autofillable
  // (non-blocklisted) passwords. The registered `Consumer` is notified with
  // `OnCompleteWithLogins` when the job with the returned JobId succeeds.
  // `syncing_account` is used to decide which storage to use. If
  // `syncing_account` is absl::nullopt local storage will be used.
  [[nodiscard]] virtual JobId GetAutofillableLogins(Account account) = 0;

  // Triggers an asynchronous request to retrieve stored passwords with
  // matching |signon_realm|. The returned results must be validated (e.g
  // matching "sample.com" also returns logins for "not-sample.com").
  // The registered `Consumer` is notified with `OnCompleteWithLogins` when the
  // job with the returned JobId succeeds. `syncing_account` is used to decide
  // which storage to use. If `syncing_account` is absl::nullopt local storage
  // will be used.
  [[nodiscard]] virtual JobId GetLoginsForSignonRealm(
      const std::string& signon_realm,
      Account account) = 0;

  // Triggers an asynchronous request to add |form| to store. The
  // registered `Consumer` is notified with `OnLoginsChanged` when the
  // job with the returned JobId succeeds. `syncing_account` is used to decide
  // which storage to use. If `syncing_account` is absl::nullopt local storage
  // will be used.
  [[nodiscard]] virtual JobId AddLogin(const PasswordForm& form,
                                       Account account) = 0;

  // Triggers an asynchronous request to update |form| in store. The
  // registered `Consumer` is notified with `OnLoginsChanged` when the
  // job with the returned JobId succeeds. `syncing_account` is used to decide
  // which storage to use. If `syncing_account` is absl::nullopt local storage
  // will be used.
  [[nodiscard]] virtual JobId UpdateLogin(const PasswordForm& form,
                                          Account account) = 0;

  // Triggers an asynchronous request to remove |form| from store. The
  // registered `Consumer` is notified with `OnLoginsChanged` when the
  // job with the returned JobId succeeds. `syncing_account` is used to decide
  // which storage to use. If `syncing_account` is absl::nullopt local storage
  // will be used.
  [[nodiscard]] virtual JobId RemoveLogin(const PasswordForm& form,
                                          Account account) = 0;

  // Displays a notification when a store backend request finishes with an
  // unrecoverable error. TODO(crbug.com/1344576) Remove when not required
  // anymore.
  virtual void ShowErrorNotification() = 0;

  // Factory function for creating the bridge. Implementation is pulled in by
  // including an implementation or by defining it explicitly in tests.
  // Ensure `CanCreateBackend` returns true before calling this method.
  // `consumer_bridge` will be set to handle callbacks from the Java side and
  // should outlive this object.
  static std::unique_ptr<PasswordStoreAndroidBackendBridge> Create(
      const PasswordStoreAndroidBackendConsumerBridge& consumer_bridge);

  // Method that checks whether a backend can be created or whether `Create`
  // would fail. It returns true iff all nontransient prerequisistes are
  // fulfilled. E.g. if the backend requires a minimum GMS version this method
  // would return false.
  static bool CanCreateBackend();
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_H_
