// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CRYPTOHOME_RECOVERY_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CRYPTOHOME_RECOVERY_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/recovery/cryptohome_recovery_performer.h"
#include "components/account_id/account_id.h"

namespace ash {

class CryptohomeRecoveryScreenView;

// Controller for the Cryptohome recovery screen.
class CryptohomeRecoveryScreen : public BaseScreen {
 public:
  using TView = CryptohomeRecoveryScreenView;
  enum class Result {
    kSucceeded,
    kGaiaLogin,
    kManualRecovery,
    kRetry,
    kNoRecoveryFactor,
    kNotApplicable,
  };
  static std::string GetResultString(Result result);
  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  CryptohomeRecoveryScreen(base::WeakPtr<CryptohomeRecoveryScreenView> view,
                           const ScreenExitCallback& exit_callback);
  ~CryptohomeRecoveryScreen() override;

  CryptohomeRecoveryScreen(const CryptohomeRecoveryScreen&) = delete;
  CryptohomeRecoveryScreen& operator=(const CryptohomeRecoveryScreen&) = delete;

  ScreenExitCallback get_exit_callback_for_testing() { return exit_callback_; }
  void set_exit_callback_for_testing(const ScreenExitCallback& callback) {
    exit_callback_ = callback;
  }

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

 private:
  void OnGetAuthFactorsConfiguration(std::unique_ptr<UserContext> user_context,
                                     absl::optional<AuthenticationError> error);
  void OnAuthenticateWithRecovery(std::unique_ptr<UserContext> context,
                                  absl::optional<AuthenticationError> error);
  void OnRotateRecoveryFactor(std::unique_ptr<UserContext> context,
                              absl::optional<AuthenticationError> error);
  void OnReplaceContextKey(std::unique_ptr<UserContext> context,
                           absl::optional<AuthenticationError> error);

  AuthFactorEditor auth_factor_editor_;
  std::unique_ptr<CryptohomeRecoveryPerformer> recovery_performer_;

  base::WeakPtr<CryptohomeRecoveryScreenView> view_ = nullptr;

  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<CryptohomeRecoveryScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CRYPTOHOME_RECOVERY_SCREEN_H_
