// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/cryptohome_recovery_setup_screen.h"

#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/recovery_factor_editor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

constexpr char kUserActionSkip[] = "skip";
constexpr char kUserActionRetry[] = "retry";

}  // namespace

// static
std::string CryptohomeRecoverySetupScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
    case Result::DONE:
      return "Done";
    case Result::SKIPPED:
      return "Skipped";
  }
}

CryptohomeRecoverySetupScreen::CryptohomeRecoverySetupScreen(
    base::WeakPtr<CryptohomeRecoverySetupScreenView> view,
    ScreenExitCallback exit_callback)
    : BaseScreen(CryptohomeRecoverySetupScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(std::move(exit_callback)),
      auth_performer_(UserDataAuthClient::Get()),
      cryptohome_pin_engine_(&auth_performer_) {}

CryptohomeRecoverySetupScreen::~CryptohomeRecoverySetupScreen() = default;

void CryptohomeRecoverySetupScreen::ShowImpl() {
  if (!view_)
    return;

  // Show UI with a spinner while we are setting up the recovery auth factor.
  view_->Show();

  SetupRecovery();
}

void CryptohomeRecoverySetupScreen::HideImpl() {}

void CryptohomeRecoverySetupScreen::OnUserAction(
    const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionSkip) {
    exit_callback_.Run(Result::SKIPPED);
    return;
  }
  if (action_id == kUserActionRetry) {
    view_->SetLoadingState();
    SetupRecovery();
    return;
  }
  BaseScreen::OnUserAction(args);
}

bool CryptohomeRecoverySetupScreen::MaybeSkip(WizardContext& wizard_context) {
  // Skip recovery setup if the user didn't opt-in.
  if (wizard_context.skip_post_login_screens_for_tests ||
      !wizard_context.recovery_setup.recovery_factor_opted_in) {
    ExitScreen(wizard_context, Result::NOT_APPLICABLE);
    return true;
  }

  return false;
}

void CryptohomeRecoverySetupScreen::SetupRecovery() {
  // Reset the weak ptr to prevent multiple calls at the same time.
  weak_ptr_factory_.InvalidateWeakPtrs();

  std::string token;
  if (ash::features::ShouldUseAuthSessionStorage()) {
    CHECK(context()->extra_factors_token.has_value());
    token = context()->extra_factors_token.value();
  } else {
    CHECK(context()->extra_factors_auth_session);

    quick_unlock::QuickUnlockStorage* quick_unlock_storage =
        quick_unlock::QuickUnlockFactory::GetForProfile(
            ProfileManager::GetActiveUserProfile());
    CHECK(quick_unlock_storage);
    token = quick_unlock_storage->CreateAuthToken(
        *context()->extra_factors_auth_session);
  }
  auto& recovery_editor = auth::GetRecoveryFactorEditor(
      quick_unlock::QuickUnlockFactory::GetDelegate());
  recovery_editor.Configure(
      token, /*enabled=*/true,
      base::BindOnce(&CryptohomeRecoverySetupScreen::OnRecoveryConfigured,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeRecoverySetupScreen::ExitScreen(
    WizardContext& wizard_context,
    CryptohomeRecoverySetupScreen::Result result) {
  // Clear the auth session if it's not needed for PIN setup.
  if (ash::features::ShouldUseAuthSessionStorage()) {
    if (wizard_context.extra_factors_token.has_value()) {
      auto& token = wizard_context.extra_factors_token.value();
      auto* storage = ash::AuthSessionStorage::Get();
      if (storage->IsValid(token) &&
          cryptohome_pin_engine_.ShouldSkipSetupBecauseOfPolicy(
              storage->Peek(token)->GetAccountId())) {
        storage->Invalidate(token, base::DoNothing());
        wizard_context.extra_factors_token = absl::nullopt;
      }
    }
  } else {
    if (wizard_context.extra_factors_auth_session != nullptr &&
        cryptohome_pin_engine_.ShouldSkipSetupBecauseOfPolicy(
            wizard_context.extra_factors_auth_session->GetAccountId())) {
      wizard_context.extra_factors_auth_session.reset();
    }
  }

  exit_callback_.Run(result);
}

void CryptohomeRecoverySetupScreen::OnRecoveryConfigured(
    auth::mojom::ConfigureResult result) {
  switch (result) {
    case auth::mojom::ConfigureResult::kSuccess:
      ExitScreen(*context(), Result::DONE);
      break;
    case auth::mojom::ConfigureResult::kInvalidTokenError:
    case auth::mojom::ConfigureResult::kFatalError:
      LOG(ERROR) << "Failed to setup recovery factor, result "
                 << static_cast<int>(result);
      view_->OnSetupFailed();
      break;
  }
}

}  // namespace ash
