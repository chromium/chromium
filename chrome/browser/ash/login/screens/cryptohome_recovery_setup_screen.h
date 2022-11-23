// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CRYPTOHOME_RECOVERY_SETUP_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CRYPTOHOME_RECOVERY_SETUP_SCREEN_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "components/account_id/account_id.h"

namespace ash {

class CryptohomeRecoverySetupScreenView;

// Controller for the Cryptohome recovery screen.
class CryptohomeRecoverySetupScreen : public BaseScreen {
 public:
  using TView = CryptohomeRecoverySetupScreenView;

  CryptohomeRecoverySetupScreen(
      base::WeakPtr<CryptohomeRecoverySetupScreenView> view,
      const base::RepeatingClosure& exit_callback);
  ~CryptohomeRecoverySetupScreen() override;

  CryptohomeRecoverySetupScreen(const CryptohomeRecoverySetupScreen&) = delete;
  CryptohomeRecoverySetupScreen& operator=(
      const CryptohomeRecoverySetupScreen&) = delete;

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  bool MaybeSkip(WizardContext& context) override;

 private:
  base::WeakPtr<CryptohomeRecoverySetupScreenView> view_ = nullptr;
  base::RepeatingClosure exit_callback_;
  base::WeakPtrFactory<CryptohomeRecoverySetupScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CRYPTOHOME_RECOVERY_SETUP_SCREEN_H_
