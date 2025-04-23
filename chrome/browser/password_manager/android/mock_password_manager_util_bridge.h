// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_MANAGER_UTIL_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_MANAGER_UTIL_BRIDGE_H_

#include "chrome/browser/password_manager/android/password_manager_util_bridge_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPasswordManagerUtilBridge
    : public password_manager_android_util::PasswordManagerUtilBridgeInterface {
 public:
  MockPasswordManagerUtilBridge();
  ~MockPasswordManagerUtilBridge() override;

  MOCK_METHOD(bool, IsInternalBackendPresent, (), (override));
  MOCK_METHOD(bool, IsPlayStoreAppPresent, (), (override));
  MOCK_METHOD(bool, IsGooglePlayServicesUpdatable, (), (override));
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_MOCK_PASSWORD_MANAGER_UTIL_BRIDGE_H_
