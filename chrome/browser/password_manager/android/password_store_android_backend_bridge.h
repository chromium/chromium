// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_H_

#include <vector>

#include "base/types/strong_alias.h"
#include "chrome/browser/password_manager/android/password_store_operation_target.h"
#include "components/password_manager/core/browser/android_backend_error.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_backend.h"
#include "components/password_manager/core/browser/password_store_change.h"

namespace password_manager {

// Interface for the native side of the JNI bridge. Simplifies mocking in tests.
// Implementers of this class are expected to be a minimal layer of glue code
// that performs API call on the Java side to Google Mobile for each operation.
// Any logic beyond data conversion should either live in
// `PasswordStoreAndroidBackend` or a component that is used by the java-side of
// this bridge.
class PasswordStoreAndroidBackendBridge {
 public:
  using JobId = base::StrongAlias<struct JobIdTag, int>;
  using SyncingAccount =
      base::StrongAlias<struct SyncingAccountTag, std::string>;
  using Account = absl::variant<PasswordStoreOperationTarget, SyncingAccount>;

  // Each bridge is created with a consumer that will be called when a job is
  // completed. In order to identify which request the response belongs to, the
  // bridge needs to pass a `JobId` back that was returned by the initial
  // request and is unique per bridge.
  class Consumer {
   public:
    virtual ~Consumer() = default;

    // Asynchronous response called with the `job_id` which was passed to the
    // corresponding call to `PasswordStoreAndroidBackendBridge`, and with the
    // requested `passwords`.
    // Used in response to `GetAllLogins`.
    virtual void OnCompleteWithLogins(JobId job_id,
                                      std::vector<PasswordForm> passwords) = 0;

    // Asynchronous response called with the `job_id` which was passed to the
    // corresponding call to `PasswordStoreAndroidBackendBridge`, and with the
    // PasswordChanges.
    // Used in response to 'AddLogin', 'UpdateLogin' and `RemoveLogin`.
    virtual void OnLoginsChanged(JobId job_id, PasswordChanges changes) = 0;

    // Asynchronous response called with the `job_id` which was passed to the
    // corresponding call to `PasswordStoreAndroidBackendBridge`.
    virtual void OnError(JobId job_id, AndroidBackendError error) = 0;

    // Asynchronous success called with the `job_id` which was passed to a
    // subscribe call (or fake thereof) to `PasswordStoreAndroidBackendBridge`.
    virtual void OnSubscribed(JobId job_id) = 0;
    // Asynchronous failure called with the `job_id` which was passed to a
    // subscribe call (or fake thereof) to `PasswordStoreAndroidBackendBridge`.
    virtual void OnSubscribeFailed(JobId job_id, AndroidBackendError error) = 0;
  };

  virtual ~PasswordStoreAndroidBackendBridge() = default;

  // Sets the `consumer` that is notified on job completion.
  virtual void SetConsumer(base::WeakPtr<Consumer> consumer) = 0;

  // Triggers an asynchronous request signaling interest in passwords. The
  // registered `Consumer` is notified with `OnSubscribed` when the job with
  // the returnde JobId has been completed. `syncing_account` is used to decide
  // which storage to use. If `syncing_account` is absl::nullopt local storage
  // will be used.
  [[nodiscard]] virtual JobId Subscribe(Account account) = 0;

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
  static std::unique_ptr<PasswordStoreAndroidBackendBridge> Create();

  // Method that checks whether a backend can be created or whether `Create`
  // would fail. It returns true iff all nontransient prerequisistes are
  // fulfilled. E.g. if the backend requires a minimum GMS version this method
  // would return false.
  static bool CanCreateBackend();
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_H_
