// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/cryptohome_recovery_setup_screen.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/pin_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-shared.h"

namespace ash {

namespace {

constexpr char kUserActionSkip[] = "skip";
constexpr char kUserActionRetry[] = "retry";

}  // namespace

// static
std::string CryptohomeRecoverySetupScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
    case Result::DONE:
      return "Done";
    case Result::SKIPPED:
      return "Skipped";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
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
  if (!wizard_context.recovery_setup.recovery_factor_opted_in) {
    ExitScreen(wizard_context, Result::NOT_APPLICABLE);
    return true;
  }

  return false;
}

void CryptohomeRecoverySetupScreen::SetupRecovery() {
  // Reset the weak ptr to prevent multiple calls at the same time.
  weak_ptr_factory_.InvalidateWeakPtrs();

  std::string token;
  CHECK(context()->extra_factors_token.has_value());
  token = context()->extra_factors_token.value();
  auto& recovery_editor = auth::GetRecoveryFactorEditor(
      quick_unlock::QuickUnlockFactory::GetDelegate(),
      g_browser_process->local_state());
  recovery_editor.Configure(
      token, /*enabled=*/true,
      base::BindOnce(&CryptohomeRecoverySetupScreen::OnRecoveryConfigured,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeRecoverySetupScreen::ExitScreen(
    WizardContext& wizard_context,
    CryptohomeRecoverySetupScreen::Result result) {
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
