// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_PASSWORD_SELECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_PASSWORD_SELECTION_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-shared.h"

namespace ash {

class UserContext;

class PasswordSelectionScreenView;

// Controller for the Password Selection Screen, which allows the user to choose
// between the local password or Gaia password setup.
class PasswordSelectionScreen : public BaseOSAuthSetupScreen {
 public:
  using TView = PasswordSelectionScreenView;
  enum class Result {
    NOT_APPLICABLE,
    BACK,
    LOCAL_PASSWORD_CHOICE,
    LOCAL_PASSWORD_FORCED,
    GAIA_PASSWORD_FALLBACK,
    GAIA_PASSWORD_CHOICE,
    GAIA_PASSWORD_ENTERPRISE,
  };

  static std::string GetResultString(Result result);
  using ScreenExitCallback = base::RepeatingCallback<void(Result)>;

  PasswordSelectionScreen(base::WeakPtr<PasswordSelectionScreenView> view,
                          ScreenExitCallback exit_callback);
  ~PasswordSelectionScreen() override;

  PasswordSelectionScreen(const PasswordSelectionScreen&) = delete;
  PasswordSelectionScreen& operator=(const PasswordSelectionScreen&) = delete;

  ScreenExitCallback get_exit_callback_for_testing() { return exit_callback_; }
  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool MaybeSkip(WizardContext& context) override;

 private:
  void InspectContext(UserContext* user_context);
  void ProcessOptions();
  void ShowPasswordChoice();

  void SetOnlinePassword();
  void OnOnlinePasswordSet(auth::mojom::ConfigureResult result);

  // Values obtained from UserContext in `InspectContext`
  bool has_online_password_ = false;
  bool is_shown_ = false;
  AuthFactorsConfiguration auth_factors_config_;

  base::WeakPtr<PasswordSelectionScreenView> view_ = nullptr;
  ScreenExitCallback exit_callback_;
  base::WeakPtrFactory<PasswordSelectionScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_PASSWORD_SELECTION_SCREEN_H_
