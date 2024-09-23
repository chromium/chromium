// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_APPLY_ONLINE_PASSWORD_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_APPLY_ONLINE_PASSWORD_SCREEN_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/auth_types.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-shared.h"

namespace ash {

class UserContext;

class ApplyOnlinePasswordScreenView;

// Screen that sets up (creates or updates) user's Online password as
// their cryptohome Knowledge factor.
class ApplyOnlinePasswordScreen : public BaseOSAuthSetupScreen {
 public:
  using TView = ApplyOnlinePasswordScreenView;
  enum class Result {
    kNotApplicable,
    kSuccess,
    kError,
  };

  static std::string GetResultString(Result result);
  using ScreenExitCallback = base::RepeatingCallback<void(Result)>;

  ApplyOnlinePasswordScreen(base::WeakPtr<ApplyOnlinePasswordScreenView> view,
                            ScreenExitCallback exit_callback);
  ~ApplyOnlinePasswordScreen() override;

  ApplyOnlinePasswordScreen(const ApplyOnlinePasswordScreen&) = delete;
  ApplyOnlinePasswordScreen& operator=(const ApplyOnlinePasswordScreen&) =
      delete;

  ScreenExitCallback get_exit_callback_for_testing() { return exit_callback_; }
  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  bool MaybeSkip(WizardContext& context) override;

 private:
  void InspectContext(UserContext* user_context);
  void SetOnlinePassword();
  void OnOnlinePasswordSet(auth::mojom::ConfigureResult result);

  // Values obtained from UserContext in `InspectContext`
  std::optional<OnlinePassword> online_password_;
  AuthFactorsConfiguration auth_factors_config_;

  base::WeakPtr<ApplyOnlinePasswordScreenView> view_ = nullptr;
  ScreenExitCallback exit_callback_;
  base::WeakPtrFactory<ApplyOnlinePasswordScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_APPLY_ONLINE_PASSWORD_SCREEN_H_
