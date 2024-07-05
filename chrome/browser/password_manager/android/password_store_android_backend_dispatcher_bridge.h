// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_DISPATCHER_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_DISPATCHER_BRIDGE_H_

#include <vector>

#include "chrome/browser/password_manager/android/password_store_android_backend_receiver_bridge.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

// Interface for the native side of the JNI bridge. Simplifies mocking in tests.
// Implementers of this class are expected to be a minimal layer of glue code
// that performs API call on the Java side to Google Mobile for each operation.
// Any logic beyond data conversion should either live in
// `PasswordStoreAndroidBackend` or a component that is used by the java-side of
// this bridge. Receiver bridge is defined separately in
// `PasswordStoreAndroidBackendReceiverBridge`.
// All methods should be called on the same single threaded sequence bound to
// the background thread. Instance could be constructed and destroyed on any
// thread.
class PasswordStoreAndroidBackendDispatcherBridge {
 public:
  using JobId = PasswordStoreAndroidBackendReceiverBridge::JobId;

  virtual ~PasswordStoreAndroidBackendDispatcherBridge() = default;

  // Perform bridge and Java counterpart initialization. This method should be
  // executed on the same thread where all operations will run.
  // `receiver_bridge` is the java counterpart of the
  // `PasswordStoreAndroidBackendReceiverBridge` and should outlive this object.
  virtual void Init(
      base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) = 0;

  // Triggers an asynchronous request to retrieve all stored passwords. The
  // registered `Consumer` is notified with `OnCompleteWithLogins` via the
  // receiver bridge when the job with the given JobId succeeds.
  // `account` is used to decide which storage to use. If
  // `account` is empty, the local storage will be used.
  virtual void GetAllLogins(JobId job_id, std::string account) = 0;

  // Triggers an asynchronous request to retrieve all stored passwords with
  // branding info (App display name and icon). The registered `Consumer` is
  // notified with `OnCompleteWithBrandedLogins` via the receiver bridge when
  // the job with the given JobId succeeds. `account` is used to decide which
  // storage to use. If `account` is empty, the local storage will be used.
  virtual void GetAllLoginsWithBrandingInfo(JobId job_id,
                                            std::string account) = 0;

  // Triggers an asynchronous request to retrieve all autofillable
  // (non-blocklisted) passwords. The registered `Consumer` is notified with
  // `OnCompleteWithLogins` via the receiver bridge when the job with the
  // given JobId succeeds. `account` is used to decide which storage
  // to use. If `account` is empty, the local storage will be used.
  virtual void GetAutofillableLogins(JobId job_id, std::string account) = 0;

  // Triggers an asynchronous request to retrieve stored passwords with
  // matching |signon_realm|. The returned results must be validated (e.g
  // matching "sample.com" also returns logins for "not-sample.com").
  // The registered `Consumer` is notified with `OnCompleteWithLogins` via the
  // receiver bridge when the job with the given JobId succeeds.
  // `account` is used to decide which storage to use. If
  // `account` is empty the local storage will be used.
  virtual void GetLoginsForSignonRealm(JobId job_id,
                                       const std::string& signon_realm,
                                       std::string account) = 0;

  // Triggers an asynchronous request to retrieve stored affiliated passwords
  // matching |signon_realm| or affiliated with |signon_realm| or grouped with
  // |signon_realm|. The registered `Consumer` is notified with
  // `OnCompleteWithLogins` via the receiver bridge when the job with the given
  // JobId succeeds. `account` is used to decide which storage to use.
  // If `account` is empty the local storage will be used.
  virtual void GetAffiliatedLoginsForSignonRealm(
      JobId job_id,
      const std::string& signon_realm,
      std::string account) = 0;

  // Triggers an asynchronous request to add |form| to store. The
  // registered `Consumer` is notified with `OnLoginsChanged` via the receiver
  // bridge when the job with the given JobId succeeds. `account` is
  // used to decide which storage to use. If `account` is empty, the
  // local storage will be used.
  virtual void AddLogin(JobId job_id,
                        const PasswordForm& form,
                        std::string account) = 0;

  // Triggers an asynchronous request to update |form| in store. The
  // registered `Consumer` is notified with `OnLoginsChanged` via the receiver
  // bridge when the job with the given JobId succeeds. `account` is
  // used to decide which storage to use. If `account` is empty, the
  // local storage will be used.
  virtual void UpdateLogin(JobId job_id,
                           const PasswordForm& form,
                           std::string account) = 0;

  // Triggers an asynchronous request to remove |form| from store. The
  // registered `Consumer` is notified with `OnLoginsChanged` via the receiver
  // bridge when the job with the given JobId succeeds. `account` is
  // used to decide which storage to use. If `account` is empty, the
  // local storage will be used.
  virtual void RemoveLogin(JobId job_id,
                           const PasswordForm& form,
                           std::string account) = 0;

  // Factory function for creating the bridge. Implementation is pulled in by
  // including an implementation or by defining it explicitly in tests.
  // Ensure `password_manager_android_util::IsUpmBackendForAccountAvailable`
  // returns true before calling this method.
  static std::unique_ptr<PasswordStoreAndroidBackendDispatcherBridge> Create();
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_DISPATCHER_BRIDGE_H_
