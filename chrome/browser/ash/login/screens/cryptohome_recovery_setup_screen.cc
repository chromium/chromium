// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/cryptohome_recovery_setup_screen.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"

namespace ash {

CryptohomeRecoverySetupScreen::CryptohomeRecoverySetupScreen(
    base::WeakPtr<CryptohomeRecoverySetupScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(CryptohomeRecoverySetupScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

CryptohomeRecoverySetupScreen::~CryptohomeRecoverySetupScreen() = default;

void CryptohomeRecoverySetupScreen::ShowImpl() {
  if (!view_)
    return;

  // Show UI with a spinner while we are setting up the recovery auth factor.
  view_->Show();

  // TODO(b/239420684): Setup the recovery auth factor.
}

void CryptohomeRecoverySetupScreen::HideImpl() {}

void CryptohomeRecoverySetupScreen::OnUserAction(
    const base::Value::List& args) {
  BaseScreen::OnUserAction(args);
}

bool CryptohomeRecoverySetupScreen::MaybeSkip(WizardContext& wizard_context) {
  // Skip recovery setup if the user didn't opt-in.
  if (!wizard_context.recovery_factor_opted_in) {
    exit_callback_.Run();
    return true;
  }

  return false;
}

}  // namespace ash
