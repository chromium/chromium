// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_DISPATCHER_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_DISPATCHER_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/password_manager/android/password_store_android_backend_dispatcher_bridge.h"

namespace password_manager {

// Native side of the JNI bridge to forward password store requests to Java.
// JNI code is expensive to test. Therefore, any logic beyond data conversion
// should either live in `PasswordStoreAndroidBackend` or a component that is
// used by the java-side of this bridge.
// Class could be instantiated and deleted on any thread. All methods should be
// called on a single threaded sequence bound to a single background thread.
// This class instance itself could be created and destroyed on any thread.
// Thread affinity come from the JNIEnv which could only be used from a single
// physical thread where JNIEnv was created.
class PasswordStoreAndroidBackendDispatcherBridgeImpl
    : public PasswordStoreAndroidBackendDispatcherBridge {
 public:
  PasswordStoreAndroidBackendDispatcherBridgeImpl();
  PasswordStoreAndroidBackendDispatcherBridgeImpl(
      PasswordStoreAndroidBackendDispatcherBridgeImpl&&) = delete;
  PasswordStoreAndroidBackendDispatcherBridgeImpl(
      const PasswordStoreAndroidBackendDispatcherBridgeImpl&) = delete;
  PasswordStoreAndroidBackendDispatcherBridgeImpl& operator=(
      PasswordStoreAndroidBackendDispatcherBridgeImpl&&) = delete;
  PasswordStoreAndroidBackendDispatcherBridgeImpl& operator=(
      const PasswordStoreAndroidBackendDispatcherBridgeImpl&) = delete;
  ~PasswordStoreAndroidBackendDispatcherBridgeImpl() override;

  void Init(
      base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) override;

 private:
  // Implements PasswordStoreAndroidBackendDispatcherBridge interface.
  void GetAllLogins(JobId job_id, std::string account) override;
  void GetAllLoginsWithBrandingInfo(JobId job_id, std::string account) override;
  void GetAutofillableLogins(JobId job_id, std::string account) override;
  void GetLoginsForSignonRealm(JobId job_id,
                               const std::string& signon_realm,
                               std::string account) override;
  void GetAffiliatedLoginsForSignonRealm(JobId job_id,
                                         const std::string& signon_realm,
                                         std::string account) override;
  void AddLogin(JobId job_id,
                const password_manager::PasswordForm& form,
                std::string account) override;
  void UpdateLogin(JobId job_id,
                   const password_manager::PasswordForm& form,
                   std::string account) override;
  void RemoveLogin(JobId job_id,
                   const password_manager::PasswordForm& form,
                   std::string account) override;

  // This member stores the unique ID last used for an API request.
  JobId last_job_id_{0};

  // This object is an instance of
  // `PasswordStoreAndroidBackendDispatcherBridgeImpl`, i.e. the Java
  // counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_
      GUARDED_BY_CONTEXT(thread_checker_);

  // All operations should be called on the same background thread.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_STORE_ANDROID_BACKEND_DISPATCHER_BRIDGE_IMPL_H_
