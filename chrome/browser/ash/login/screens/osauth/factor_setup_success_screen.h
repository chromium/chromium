// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_FACTOR_SETUP_SUCCESS_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_FACTOR_SETUP_SUCCESS_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"

namespace ash {

class FactorSetupSuccessScreenView;
class WizardContext;

// Screen to inform the user that their authentication factors were
// successfully updated during prior operations.
class FactorSetupSuccessScreen : public BaseOSAuthSetupScreen {
 public:
  using TView = FactorSetupSuccessScreenView;
  enum class Result {
    kNotApplicable,
    kProceed,
    kTimedOut,
  };

  static std::string GetResultString(Result result);
  using ScreenExitCallback = base::RepeatingCallback<void(Result)>;

  FactorSetupSuccessScreen(base::WeakPtr<FactorSetupSuccessScreenView> view,
                           ScreenExitCallback exit_callback);
  ~FactorSetupSuccessScreen() override;

  FactorSetupSuccessScreen(const FactorSetupSuccessScreen&) = delete;
  FactorSetupSuccessScreen& operator=(const FactorSetupSuccessScreen&) = delete;

 private:
  // BaseOauthSetupScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  void InspectContext(UserContext* user_context);
  void DoShow();
  void OnTimeout();

  std::unique_ptr<base::OneShotTimer> expiration_timer_;
  base::WeakPtr<FactorSetupSuccessScreenView> view_ = nullptr;
  ScreenExitCallback exit_callback_;
  base::WeakPtrFactory<FactorSetupSuccessScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_FACTOR_SETUP_SUCCESS_SCREEN_H_
