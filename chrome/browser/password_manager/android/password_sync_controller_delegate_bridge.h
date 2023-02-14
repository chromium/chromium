// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_H_

#include <jni.h>

#include "base/memory/weak_ptr.h"

namespace password_manager {
struct AndroidBackendError;
}  // namespace password_manager

// Interface for the native side of PasswordSyncControllerDelegate JNI bridge.
// Simplifies mocking in tests.
class PasswordSyncControllerDelegateBridge {
 public:
  // Each bridge is created with a consumer that will be called when a job is
  // completed.
  class Consumer {
   public:
    virtual ~Consumer() = default;

    // Asynchronous response called when CredentialManager API calls are
    // executed successfully.
    virtual void OnCredentialManagerNotified() = 0;

    // Asynchronous response called when CredentialManager API calls fail.
    virtual void OnCredentialManagerError(
        const password_manager::AndroidBackendError& error,
        int api_error_code) = 0;
  };

  virtual ~PasswordSyncControllerDelegateBridge() = default;

  // Sets the `consumer` that is notified on job completion.
  virtual void SetConsumer(base::WeakPtr<Consumer> consumer) = 0;

  // Triggers an asynchronous request to notify credential manager of
  // the currently syncyng account. `OnCredentialManagerNotified` is called
  // when the request succeeds. `account_email` is the email of the syncing
  // account.
  virtual void NotifyCredentialManagerWhenSyncing(
      const std::string& account_email) = 0;

  // Triggers an asynchronous request to notify credential manager when
  // passwords are not synced. `OnCredentialManagerNotified` is called when the
  // request succeeds.
  virtual void NotifyCredentialManagerWhenNotSyncing() = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_H_
