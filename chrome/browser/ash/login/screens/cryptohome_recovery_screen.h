// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CRYPTOHOME_RECOVERY_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CRYPTOHOME_RECOVERY_SCREEN_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "components/account_id/account_id.h"

namespace ash {

class CryptohomeRecoveryScreenView;

// Controller for the Cryptohome recovery screen.
class CryptohomeRecoveryScreen : public BaseScreen {
 public:
  using TView = CryptohomeRecoveryScreenView;

  CryptohomeRecoveryScreen(base::WeakPtr<CryptohomeRecoveryScreenView> view,
                           const base::RepeatingClosure& exit_callback);
  ~CryptohomeRecoveryScreen() override;

  CryptohomeRecoveryScreen(const CryptohomeRecoveryScreen&) = delete;
  CryptohomeRecoveryScreen& operator=(const CryptohomeRecoveryScreen&) = delete;

  void Configure(const AccountId& account_id);

 protected:
  // BaseScreen:
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

 private:
  AccountId account_id_;

  base::WeakPtr<CryptohomeRecoveryScreenView> view_ = nullptr;

  base::RepeatingClosure exit_callback_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CRYPTOHOME_RECOVERY_SCREEN_H_
