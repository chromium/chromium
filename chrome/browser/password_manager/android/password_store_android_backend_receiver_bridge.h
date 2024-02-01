// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_RECEIVER_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_RECEIVER_BRIDGE_H_

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/types/strong_alias.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/android_backend_error.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace password_manager {

// Interface for the native side of the consumer JNI bridge. Defines callbacks
// for backend operations to be called by the GMSCore via the bridge.
// Simplifies mocking in tests. Implementers of this class are expected to be a
// minimal layer of glue code that handles API callbacks for each operation.
// Any logic beyond data conversion should either live in
// `PasswordStoreAndroidBackend` or a component that is used by the java-side of
// this bridge.
class PasswordStoreAndroidBackendReceiverBridge {
 public:
  using Account = base::StrongAlias<struct SyncingAccountTag, std::string>;
  using JobId = base::StrongAlias<struct JobIdTag, int>;

  // Each bridge is created with a consumer that will be called when a job is
  // completed. In order to identify which request the response belongs to, the
  // bridge needs to pass a `JobId` back that was returned by the initial
  // request and is unique per bridge.
  class Consumer {
   public:
    virtual ~Consumer() = default;

    // Asynchronous response called with the `job_id` which was passed to the
    // corresponding call to `PasswordStoreAndroidBackendDispatcherBridge`, and
    // with the requested `passwords`. Used in response to `GetAllLogins`.
    virtual void OnCompleteWithLogins(JobId job_id,
                                      std::vector<PasswordForm> passwords) = 0;

    // Asynchronous response called with the `job_id` which was passed to the
    // corresponding call to `PasswordStoreAndroidBackendDispatcherBridge`, and
    // with the PasswordChanges. Used in response to 'AddLogin', 'UpdateLogin'
    // and `RemoveLogin`.
    virtual void OnLoginsChanged(JobId job_id, PasswordChanges changes) = 0;

    // Asynchronous response called with the `job_id` which was passed to the
    // corresponding call to `PasswordStoreAndroidBackendDispatcherBridge`.
    virtual void OnError(JobId job_id, AndroidBackendError error) = 0;
  };

  virtual ~PasswordStoreAndroidBackendReceiverBridge() = default;

  // Sets the `consumer` that is notified on job completion.
  virtual void SetConsumer(base::WeakPtr<Consumer> consumer) = 0;

  // Returns reference to the Java JNI bridge object which is Java counterpart
  // of this class.
  virtual base::android::ScopedJavaGlobalRef<jobject> GetJavaBridge() const = 0;

  // Factory function for creating the bridge. Implementation is pulled in by
  // including an implementation or by defining it explicitly in tests.
  static std::unique_ptr<PasswordStoreAndroidBackendReceiverBridge> Create(
      password_manager::IsAccountStore is_account_store);
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_RECEIVER_BRIDGE_H_
