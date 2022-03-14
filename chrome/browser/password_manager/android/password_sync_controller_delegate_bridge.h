// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_H_

#include <jni.h>

// Interface for the native side of PasswordSyncControllerDelegate JNI bridge.
// Simplifies mocking in tests.
class PasswordSyncControllerDelegateBridge {
 public:
  virtual ~PasswordSyncControllerDelegateBridge() = default;

  // Triggers an asynchronous request to notify credential manager of
  // the currently syncyng account. `OnCredentialManagerNotified` is called
  // when the request succeeds.
  virtual void NotifyCredentialManagerWhenSyncing() = 0;

  // Triggers an asynchronous request to notify credential manager when
  // passwords are not synced. `OnCredentialManagerNotified` is called when the
  // request succeeds.
  virtual void NotifyCredentialManagerWhenNotSyncing() = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_H_
