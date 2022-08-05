// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_SMART_PRIVACY_PROTECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_SMART_PRIVACY_PROTECTION_SCREEN_H_

#include <string>

#include "chrome/browser/ash/login/screens/base_screen.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chrome/browser/ui/webui/chromeos/login/smart_privacy_protection_screen_handler.h"

namespace ash {

// Class that controls OOBE screen showing smart privacy protection featrure
// promotion.
class SmartPrivacyProtectionScreen : public BaseScreen {
 public:
  using TView = SmartPrivacyProtectionView;

  enum class Result {
    PROCEED_WITH_FEATURE_ON,
    PROCEED_WITH_FEATURE_OFF,
    NOT_APPLICABLE,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  SmartPrivacyProtectionScreen(base::WeakPtr<SmartPrivacyProtectionView> view,
                               const ScreenExitCallback& exit_callback);

  SmartPrivacyProtectionScreen(const SmartPrivacyProtectionScreen&) = delete;
  SmartPrivacyProtectionScreen& operator=(const SmartPrivacyProtectionScreen&) =
      delete;

  ~SmartPrivacyProtectionScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  base::WeakPtr<SmartPrivacyProtectionView> view_;

  ScreenExitCallback exit_callback_;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::SmartPrivacyProtectionScreen;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_SMART_PRIVACY_PROTECTION_SCREEN_H_
