// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_ACCOUNT_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_ACCOUNT_SELECTION_SCREEN_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

class AccountSelectionScreenView;

// This screen is shown after the manual enrollment is succesffully completed.
// It gives the user a choice to reuse the enrollment account for the device
// account or sign in again. The latter will cause the Gaia screen to be shown.
class AccountSelectionScreen : public BaseScreen {
 public:
  using TView = AccountSelectionScreenView;

  enum class Result { kGaiaFallback = 0, kNotApplicable };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  AccountSelectionScreen(base::WeakPtr<AccountSelectionScreenView> view,
                         const ScreenExitCallback& exit_callback);

  AccountSelectionScreen(const AccountSelectionScreen&) = delete;
  AccountSelectionScreen& operator=(const AccountSelectionScreen&) = delete;

  ~AccountSelectionScreen() override;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  static std::string GetResultString(Result result);

  void OnCredentialsExpiredCallback();

 private:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // Safely checks if the UserContext stored in the WizardContext contains all
  // information needed to perform the signin.
  bool IsUserContextComplete(const WizardContext* const wizard_context) const;
  bool MaybeLoginWithCachedCredentials();

  base::WeakPtr<AccountSelectionScreenView> view_;
  ScreenExitCallback exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_ACCOUNT_SELECTION_SCREEN_H_
