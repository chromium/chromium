// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_CRYPTOHOME_RECOVERY_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_CRYPTOHOME_RECOVERY_SETUP_SCREEN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/auth/cryptohome_pin_engine.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-shared.h"

namespace ash {

class CryptohomeRecoverySetupScreenView;

// Controller for the Cryptohome recovery screen.
class CryptohomeRecoverySetupScreen : public BaseScreen {
 public:
  using TView = CryptohomeRecoverySetupScreenView;
  enum class Result {
    NOT_APPLICABLE,
    DONE,
    SKIPPED,
  };
  static std::string GetResultString(Result result);
  using ScreenExitCallback = base::RepeatingCallback<void(Result)>;

  CryptohomeRecoverySetupScreen(
      base::WeakPtr<CryptohomeRecoverySetupScreenView> view,
      ScreenExitCallback exit_callback);
  ~CryptohomeRecoverySetupScreen() override;

  CryptohomeRecoverySetupScreen(const CryptohomeRecoverySetupScreen&) = delete;
  CryptohomeRecoverySetupScreen& operator=(
      const CryptohomeRecoverySetupScreen&) = delete;

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
  void SetupRecovery();
  void ExitScreen(WizardContext& wizard_context, Result result);
  void OnRecoveryConfigured(auth::mojom::ConfigureResult result);
  base::WeakPtr<CryptohomeRecoverySetupScreenView> view_ = nullptr;
  ScreenExitCallback exit_callback_;
  AuthPerformer auth_performer_;
  legacy::CryptohomePinEngine cryptohome_pin_engine_;
  base::WeakPtrFactory<CryptohomeRecoverySetupScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_OSAUTH_CRYPTOHOME_RECOVERY_SETUP_SCREEN_H_
