// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_CONSOLIDATED_CONSENT_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_CONSOLIDATED_CONSENT_SCREEN_H_

#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/consolidated_consent_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockConsolidatedConsentScreen : public ConsolidatedConsentScreen {
 public:
  MockConsolidatedConsentScreen(ConsolidatedConsentScreenView* view,
                                const ScreenExitCallback& exit_callback);
  ~MockConsolidatedConsentScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(Result result);
};

class MockConsolidatedConsentScreenView : public ConsolidatedConsentScreenView {
 public:
  MockConsolidatedConsentScreenView();
  ~MockConsolidatedConsentScreenView() override;

  void Bind(ConsolidatedConsentScreen* screen) override;
  void Unbind() override;

  MOCK_METHOD(void, Show, (const ScreenConfig& config));
  MOCK_METHOD(void, Hide, ());

  MOCK_METHOD(void, MockBind, (ConsolidatedConsentScreen * screen));
  MOCK_METHOD(void, MockUnbind, ());
  MOCK_METHOD(void, SetUsageMode, (bool enabled, bool managed));
  MOCK_METHOD(void, SetBackupMode, (bool enabled, bool managed));
  MOCK_METHOD(void, SetLocationMode, (bool enabled, bool managed));
  MOCK_METHOD(void, SetIsDeviceOwner, (bool is_owner));
  MOCK_METHOD(void, SetUsageOptinOptinHidden, (bool hidden));

 private:
  ConsolidatedConsentScreen* screen_ = nullptr;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::MockConsolidatedConsentScreen;
using ::ash::MockConsolidatedConsentScreenView;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_CONSOLIDATED_CONSENT_SCREEN_H_
