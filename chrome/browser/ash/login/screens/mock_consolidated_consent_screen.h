// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_CONSOLIDATED_CONSENT_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_CONSOLIDATED_CONSENT_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/consolidated_consent_screen.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockConsolidatedConsentScreen : public ConsolidatedConsentScreen {
 public:
  MockConsolidatedConsentScreen(
      base::WeakPtr<ConsolidatedConsentScreenView> view,
      const ScreenExitCallback& exit_callback);
  ~MockConsolidatedConsentScreen() override;

  MOCK_METHOD(void, ShowImpl, ());
  MOCK_METHOD(void, HideImpl, ());

  void ExitScreen(Result result);
};

class MockConsolidatedConsentScreenView final
    : public ConsolidatedConsentScreenView {
 public:
  MockConsolidatedConsentScreenView();
  ~MockConsolidatedConsentScreenView() override;

  MOCK_METHOD(void, Show, (base::Value::Dict data));
  MOCK_METHOD(void, Hide, ());
  MOCK_METHOD(void, SetUsageMode, (bool enabled, bool managed));
  MOCK_METHOD(void, SetBackupMode, (bool enabled, bool managed));
  MOCK_METHOD(void, SetLocationMode, (bool enabled, bool managed));
  MOCK_METHOD(void, SetIsDeviceOwner, (bool is_owner));
  MOCK_METHOD(void, SetUsageOptinHidden, (bool hidden));

  base::WeakPtr<ConsolidatedConsentScreenView> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<ConsolidatedConsentScreenView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_MOCK_CONSOLIDATED_CONSENT_SCREEN_H_
