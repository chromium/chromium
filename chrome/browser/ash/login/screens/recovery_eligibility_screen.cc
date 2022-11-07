// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/recovery_eligibility_screen.h"

#include "ash/constants/ash_features.h"
#include "base/callback.h"
#include "chrome/browser/ui/webui/ash/login/recovery_eligibility_screen_handler.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"

namespace ash {

// static
std::string RecoveryEligibilityScreen::GetResultString(Result result) {
  switch (result) {
    case Result::PROCEED:
      return "Proceed";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

RecoveryEligibilityScreen::RecoveryEligibilityScreen(
    const ScreenExitCallback& exit_callback)
    : BaseScreen(RecoveryEligibilityView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      exit_callback_(exit_callback) {}

RecoveryEligibilityScreen::~RecoveryEligibilityScreen() = default;

bool RecoveryEligibilityScreen::MaybeSkip(WizardContext& wizard_context) {
  if (!features::IsUseAuthFactorsEnabled() ||
      !features::IsCryptohomeRecoverySetupEnabled()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  if (!wizard_context.extra_factors_auth_session.get()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  if (wizard_context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void RecoveryEligibilityScreen::ShowImpl() {
  UserContext* user_context = context()->extra_factors_auth_session.get();
  auto supported_factors =
      user_context->GetAuthFactorsConfiguration().get_supported_factors();
  if (supported_factors.Has(cryptohome::AuthFactorType::kRecovery)) {
    context()->ask_about_recovery_consent = true;
  }
  exit_callback_.Run(Result::PROCEED);
}

void RecoveryEligibilityScreen::HideImpl() {}

}  // namespace ash
