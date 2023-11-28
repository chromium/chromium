// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/apply_online_password_screen.h"

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/osauth/apply_online_password_screen_handler.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/password_factor_editor.h"

namespace ash {

// static
std::string ApplyOnlinePasswordScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
    case Result::kSuccess:
      return "Success";
    case Result::kError:
      return "Error";
  }
}

ApplyOnlinePasswordScreen::ApplyOnlinePasswordScreen(
    base::WeakPtr<ApplyOnlinePasswordScreenView> view,
    ScreenExitCallback exit_callback)
    : BaseOSAuthSetupScreen(ApplyOnlinePasswordScreenView::kScreenId,
                            OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(std::move(exit_callback)) {}

ApplyOnlinePasswordScreen::~ApplyOnlinePasswordScreen() = default;

void ApplyOnlinePasswordScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  view_->Show();
  InspectContextAndContinue(
      base::BindOnce(&ApplyOnlinePasswordScreen::InspectContext,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&ApplyOnlinePasswordScreen::SetOnlinePassword,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ApplyOnlinePasswordScreen::HideImpl() {
  online_password_ = absl::nullopt;
  BaseOSAuthSetupScreen::HideImpl();
}

bool ApplyOnlinePasswordScreen::MaybeSkip(WizardContext& wizard_context) {
  CHECK(features::AreLocalPasswordsEnabledForConsumers());
  return false;
}

void ApplyOnlinePasswordScreen::InspectContext(UserContext* user_context) {
  if (!user_context) {
    LOG(ERROR) << "Session expired while waiting for user's decision";
    exit_callback_.Run(Result::kSuccess);
    return;
  }
  CHECK(user_context->HasAuthFactorsConfiguration());
  auth_factors_config_ = user_context->GetAuthFactorsConfiguration();
  online_password_ = user_context->GetOnlinePassword();
}

void ApplyOnlinePasswordScreen::SetOnlinePassword() {
  if (!online_password_.has_value()) {
    context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
    exit_callback_.Run(Result::kError);
    return;
  }
  auth::mojom::PasswordFactorEditor& password_factor_editor =
      auth::GetPasswordFactorEditor(
          quick_unlock::QuickUnlockFactory::GetDelegate(),
          g_browser_process->local_state());

  password_factor_editor.SetOnlinePassword(
      GetToken(), online_password_.value().value(),
      base::BindOnce(&ApplyOnlinePasswordScreen::OnOnlinePasswordSet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ApplyOnlinePasswordScreen::OnOnlinePasswordSet(
    auth::mojom::ConfigureResult result) {
  if (result != auth::mojom::ConfigureResult::kSuccess) {
    context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
    exit_callback_.Run(Result::kError);
    LOG(ERROR) << "Could not set online password";
  } else {
    exit_callback_.Run(Result::kSuccess);
  }
}

}  // namespace ash
