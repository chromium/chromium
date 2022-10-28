// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/cryptohome_recovery_screen.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"

namespace ash {

CryptohomeRecoveryScreen::CryptohomeRecoveryScreen(
    base::WeakPtr<CryptohomeRecoveryScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(CryptohomeRecoveryScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

CryptohomeRecoveryScreen::~CryptohomeRecoveryScreen() = default;

void CryptohomeRecoveryScreen::ShowImpl() {
  if (!view_)
    return;

  view_->Show();
}

void CryptohomeRecoveryScreen::HideImpl() {}

void CryptohomeRecoveryScreen::Configure(const AccountId& account_id) {
  DCHECK(account_id.is_valid());
  account_id_ = account_id;
}

void CryptohomeRecoveryScreen::OnUserAction(const base::Value::List& args) {
  BaseScreen::OnUserAction(args);
}

}  // namespace ash
