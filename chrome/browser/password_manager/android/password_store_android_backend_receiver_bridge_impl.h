// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_RECEIVER_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_RECEIVER_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_receiver_bridge.h"

namespace password_manager {

// Native side of the JNI bridge to handle password store callbacks from Java.
// JNI code is expensive to test. Therefore, any logic beyond data conversion
// should either live in `PasswordStoreAndroidBackend` or a component that is
// used by the java-side of this bridge.
class PasswordStoreAndroidBackendReceiverBridgeImpl
    : public password_manager::PasswordStoreAndroidBackendReceiverBridge {
 public:
  PasswordStoreAndroidBackendReceiverBridgeImpl(
      password_manager::IsAccountStore is_account_store);
  PasswordStoreAndroidBackendReceiverBridgeImpl(
      PasswordStoreAndroidBackendReceiverBridgeImpl&&) = delete;
  PasswordStoreAndroidBackendReceiverBridgeImpl(
      const PasswordStoreAndroidBackendReceiverBridgeImpl&) = delete;
  PasswordStoreAndroidBackendReceiverBridgeImpl& operator=(
      PasswordStoreAndroidBackendReceiverBridgeImpl&&) = delete;
  PasswordStoreAndroidBackendReceiverBridgeImpl& operator=(
      const PasswordStoreAndroidBackendReceiverBridgeImpl&) = delete;
  ~PasswordStoreAndroidBackendReceiverBridgeImpl() override;

  base::android::ScopedJavaGlobalRef<jobject> GetJavaBridge() const override;

  // Implements consumer interface
  // Called via JNI. Called when the api call with `job_id` finished and
  // provides the resulting `passwords`.
  void OnCompleteWithLogins(
      JNIEnv* env,
      jint job_id,
      const base::android::JavaParamRef<jbyteArray>& passwords);

  // Implements consumer interface
  // Called via JNI. Called when the api call with `job_id` finished and
  // provides the resulting affiliated `passwords`.
  virtual void OnCompleteWithBrandedLogins(
      JNIEnv* env,
      jint job_id,
      const base::android::JavaParamRef<jbyteArray>& passwords);

  // Implements consumer interface
  // Called via JNI. Called when the api call with `job_id` finished and
  // provides the resulting affiliated `passwords`.
  void OnCompleteWithAffiliatedLogins(
      JNIEnv* env,
      jint job_id,
      const base::android::JavaParamRef<jbyteArray>& passwords);

  // Called via JNI. Called when the api call with `job_id` finished that could
  // have added, modified or deleted a login.
  void OnLoginChanged(JNIEnv* env, jint job_id);

  // Called via JNI. Called when the api call with `job_id` finished with
  // an exception.
  void OnError(JNIEnv* env,
               jint job_id,
               jint error_type,
               jint api_error_code,
               jboolean has_connection_result,
               jint connection_result_code);

 private:
  // Implements PasswordStoreAndroidBackendReceiverBridge interface.
  void SetConsumer(base::WeakPtr<Consumer> consumer) override;

  // Weak reference to the `Consumer` that is notified when a job completes. It
  // outlives this bridge but tasks may be posted to it.
  base::WeakPtr<Consumer> consumer_ = nullptr;

  // This object is an instance of
  // `PasswordStoreAndroidBackendReceiverBridgeImpl`, i.e. the Java counterpart
  // to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // Differentiates between a receiver bridge backing an AccountStore vs a
  // ProfileStore. Use to mark the received passwords as Profile or Account
  // passwords.
  password_manager::IsAccountStore is_account_store_;

  // All callbacks should be called on the same background thread.
  SEQUENCE_CHECKER(main_sequence_checker_);
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_RECEIVER_BRIDGE_IMPL_H_
