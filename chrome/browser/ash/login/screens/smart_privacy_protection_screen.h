// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_SMART_PRIVACY_PROTECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_SMART_PRIVACY_PROTECTION_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"

namespace ash {

class SmartPrivacyProtectionView;

// Class that controls OOBE screen showing smart privacy protection featrure
// promotion.
class SmartPrivacyProtectionScreen : public BaseScreen {
 public:
  using TView = SmartPrivacyProtectionView;

  enum class Result {
    kProceedWithFeatureOn,
    kProceedWithFeatureOff,
    kNotApplicable,
  };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;
  SmartPrivacyProtectionScreen(base::WeakPtr<SmartPrivacyProtectionView> view,
                               const ScreenExitCallback& exit_callback);

  SmartPrivacyProtectionScreen(const SmartPrivacyProtectionScreen&) = delete;
  SmartPrivacyProtectionScreen& operator=(const SmartPrivacyProtectionScreen&) =
      delete;

  ~SmartPrivacyProtectionScreen() override;

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

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

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_SMART_PRIVACY_PROTECTION_SCREEN_H_
