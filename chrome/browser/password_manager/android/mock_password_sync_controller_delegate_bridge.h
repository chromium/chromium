// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_H_

#include "chrome/browser/password_manager/android/password_sync_controller_delegate_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPasswordSyncControllerDelegateBridge
    : public PasswordSyncControllerDelegateBridge {
 public:
  MockPasswordSyncControllerDelegateBridge();
  ~MockPasswordSyncControllerDelegateBridge() override;
  MOCK_METHOD(void, SetConsumer, (base::WeakPtr<Consumer>), (override));
  MOCK_METHOD(void,
              NotifyCredentialManagerWhenSyncing,
              (const std::string&),
              (override));
  MOCK_METHOD(void, NotifyCredentialManagerWhenNotSyncing, (), (override));
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_SYNC_CONTROLLER_DELEGATE_BRIDGE_H_
