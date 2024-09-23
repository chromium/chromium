// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_MOCK_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_MOCK_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_

#include "chrome/browser/password_manager/android/access_loss/password_access_loss_warning_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockPasswordAccessLossWarningBridge
    : public PasswordAccessLossWarningBridge {
 public:
  MockPasswordAccessLossWarningBridge();
  MockPasswordAccessLossWarningBridge(
      const MockPasswordAccessLossWarningBridge&) = delete;
  MockPasswordAccessLossWarningBridge& operator=(
      const MockPasswordAccessLossWarningBridge&) = delete;
  ~MockPasswordAccessLossWarningBridge() override;

  MOCK_METHOD(bool,
              ShouldShowAccessLossNoticeSheet,
              (PrefService*, bool),
              (override));
  MOCK_METHOD(
      void,
      MaybeShowAccessLossNoticeSheet,
      (PrefService*,
       const gfx::NativeWindow,
       Profile*,
       bool,
       password_manager_android_util::PasswordAccessLossWarningTriggers),
      (override));
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_ACCESS_LOSS_MOCK_PASSWORD_ACCESS_LOSS_WARNING_BRIDGE_H_
