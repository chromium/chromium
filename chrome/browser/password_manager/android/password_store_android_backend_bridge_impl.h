// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_bridge.h"

namespace password_manager {

// Native side of the JNI bridge to forward password store requests to Java.
// JNI code is expensive to test. Therefore, any logic beyond data conversion
// should either live in `PasswordStoreAndroidBackend` or a component that is
// used by the java-side of this bridge.
class PasswordStoreAndroidBackendBridgeImpl
    : public PasswordStoreAndroidBackendBridge {
 public:
  explicit PasswordStoreAndroidBackendBridgeImpl(
      const PasswordStoreAndroidBackendConsumerBridge& consumer_bridge);
  PasswordStoreAndroidBackendBridgeImpl(
      PasswordStoreAndroidBackendBridgeImpl&&) = delete;
  PasswordStoreAndroidBackendBridgeImpl(
      const PasswordStoreAndroidBackendBridgeImpl&) = delete;
  PasswordStoreAndroidBackendBridgeImpl& operator=(
      PasswordStoreAndroidBackendBridgeImpl&&) = delete;
  PasswordStoreAndroidBackendBridgeImpl& operator=(
      const PasswordStoreAndroidBackendBridgeImpl&) = delete;
  ~PasswordStoreAndroidBackendBridgeImpl() override;

 private:
  // Implements PasswordStoreAndroidBackendBridge interface.
  [[nodiscard]] JobId GetAllLogins(Account account) override;
  [[nodiscard]] JobId GetAutofillableLogins(Account account) override;
  [[nodiscard]] JobId GetLoginsForSignonRealm(const std::string& signon_realm,
                                              Account account) override;
  [[nodiscard]] JobId AddLogin(const PasswordForm& form,
                               Account account) override;
  [[nodiscard]] JobId UpdateLogin(const PasswordForm& form,
                                  Account account) override;
  [[nodiscard]] JobId RemoveLogin(const PasswordForm& form,
                                  Account account) override;

  [[nodiscard]] JobId GetNextJobId();

  void ShowErrorNotification() override;

  // This member stores the unique ID last used for an API request.
  JobId last_job_id_{0};

  // This object is an instance of PasswordStoreAndroidBackendBridgeImpl, i.e.
  // the Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_BRIDGE_IMPL_H_
