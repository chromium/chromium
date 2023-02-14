// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_IMPL_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge.h"

class PasswordSyncControllerDelegateBridgeImpl
    : public PasswordSyncControllerDelegateBridge {
 public:
  PasswordSyncControllerDelegateBridgeImpl();
  PasswordSyncControllerDelegateBridgeImpl(
      const PasswordSyncControllerDelegateBridgeImpl&) = delete;
  PasswordSyncControllerDelegateBridgeImpl(
      PasswordSyncControllerDelegateBridgeImpl&&) = delete;
  PasswordSyncControllerDelegateBridgeImpl& operator=(
      const PasswordSyncControllerDelegateBridgeImpl&) = delete;
  PasswordSyncControllerDelegateBridgeImpl& operator=(
      PasswordSyncControllerDelegateBridgeImpl&&) = delete;
  ~PasswordSyncControllerDelegateBridgeImpl() override;

  // PasswordSyncControllerDelegateBridge implementation.
  void SetConsumer(base::WeakPtr<Consumer> consumer) override;
  void NotifyCredentialManagerWhenSyncing(
      const std::string& account_email) override;
  void NotifyCredentialManagerWhenNotSyncing() override;

  // Called via JNI.
  void OnCredentialManagerNotified(JNIEnv* env);

  // Called via JNI. Called when the credential manager api call finishes with
  // an exception.
  void OnCredentialManagerError(JNIEnv* env,
                                jint error_code,
                                jint api_error_code);

 private:
  // Weak reference to the `Consumer` that is notified when a job completes. It
  // is guaranteed to outlive this bridge.
  base::WeakPtr<Consumer> consumer_ = nullptr;

  // This object is an instance of PasswordSyncControllerDelegateBridge, i.e.
  // the Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_IMPL_H_
