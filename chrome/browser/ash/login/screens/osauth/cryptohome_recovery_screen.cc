// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/cryptohome_recovery_screen.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/public/cpp/reauth_reason.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/login/auth/recovery/cryptohome_recovery_performer.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config_utils.h"
#include "components/user_manager/user_manager.h"

namespace {

constexpr char kUserActionReauth[] = "reauth";

}  // namespace

namespace ash {

// static
std::string CryptohomeRecoveryScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kGaiaLogin:
      return "GaiaLogin";
    case Result::kAuthenticated:
      return "Authenticated";
    case Result::kError:
      return "Error";
    case Result::kFallbackLocal:
      return "FallbackLocal";
    case Result::kFallbackOnline:
      return "FallbackOnline";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

CryptohomeRecoveryScreen::CryptohomeRecoveryScreen(
    base::WeakPtr<CryptohomeRecoveryScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(CryptohomeRecoveryScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      auth_factor_editor_(UserDataAuthClient::Get()),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

CryptohomeRecoveryScreen::~CryptohomeRecoveryScreen() = default;

void CryptohomeRecoveryScreen::ShowImpl() {
  if (!view_)
    return;

  CHECK(context()->user_context);
  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::move(context()->user_context),
      base::BindOnce(&CryptohomeRecoveryScreen::OnGetAuthFactorsConfiguration,
                     weak_ptr_factory_.GetWeakPtr()));

  view_->Show();
}

void CryptohomeRecoveryScreen::HideImpl() {}

void CryptohomeRecoveryScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionReauth) {
    exit_callback_.Run(Result::kGaiaLogin);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void CryptohomeRecoveryScreen::OnGetAuthFactorsConfiguration(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to get auth factors configuration, code "
               << error->get_cryptohome_error();
    context()->user_context = std::move(user_context);
    context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
    exit_callback_.Run(Result::kError);
    return;
  }

  const auto& config = user_context->GetAuthFactorsConfiguration();
  bool has_online_password = false;
  bool has_local_password = false;
  if (config.HasConfiguredFactor(cryptohome::AuthFactorType::kPassword)) {
    has_online_password = auth::IsGaiaPassword(
        *config.FindFactorByType(cryptohome::AuthFactorType::kPassword));
    has_local_password = auth::IsLocalPassword(
        *config.FindFactorByType(cryptohome::AuthFactorType::kPassword));
  }

  bool is_configured =
      config.HasConfiguredFactor(cryptohome::AuthFactorType::kRecovery);

  if (is_configured) {
    if (user_context->GetReauthProofToken().empty()) {
      auto account_id = user_context->GetAccountId();
      context()->user_context = std::move(user_context);
      if (was_reauth_proof_token_missing_) {
        LOG(ERROR)
            << "Reauth proof token is still missing after the second attempt";
        context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
        exit_callback_.Run(Result::kError);
        return;
      } else {
        LOG(WARNING) << "Reauth proof token is not present";
        was_reauth_proof_token_missing_ = true;
        RecordReauthReason(account_id, ReauthReason::kCryptohomeRecovery);
        view_->ShowReauthNotification();
        return;
      }
    }
    CHECK(user_context->HasAuthFactorsConfiguration());
    if (!has_online_password && !has_local_password) {
      LOG(ERROR) << "Contuining Recovery with no passwords";
    }
    recovery_performer_ = std::make_unique<CryptohomeRecoveryPerformer>(
        UserDataAuthClient::Get(),
        g_browser_process->shared_url_loader_factory());
    recovery_performer_->AuthenticateWithRecovery(
        std::move(user_context),
        base::BindOnce(&CryptohomeRecoveryScreen::OnAuthenticateWithRecovery,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    CHECK(user_context->HasAuthFactorsConfiguration());
    bool has_smart_card =
        config.HasConfiguredFactor(cryptohome::AuthFactorType::kSmartCard);
    CHECK(!has_smart_card) << "Recovery for smart card users is not supported!";

    // Exactly one of the password should exists.
    CHECK_NE(has_online_password, has_local_password);

    context()->user_context = std::move(user_context);
    if (has_online_password) {
      exit_callback_.Run(Result::kFallbackOnline);
    } else {
      exit_callback_.Run(Result::kFallbackLocal);
    }
  }
}

void CryptohomeRecoveryScreen::OnAuthenticateWithRecovery(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to authenticate with recovery, "
               << error->ToDebugString();
    context()->user_context = std::move(user_context);
    context()->osauth_error =
        WizardContext::OSAuthErrorKind::kRecoveryAuthenticationFailed;
    exit_callback_.Run(Result::kError);
    return;
  }

  // The user just authenticated with recovery factor and therefore we want to
  // rotate the recovery id.
  auth_factor_editor_.RotateRecoveryFactor(
      std::move(user_context),
      /*ensure_fresh_recovery_id=*/true,
      base::BindOnce(&CryptohomeRecoveryScreen::OnRotateRecoveryFactor,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeRecoveryScreen::OnRotateRecoveryFactor(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to rotate recovery factor, code "
               << error->get_cryptohome_error();
    context()->extra_factors_token =
        ash::AuthSessionStorage::Get()->Store(std::move(user_context));
    context()->osauth_error =
        WizardContext::OSAuthErrorKind::kRecoveryRotationFailed;
    exit_callback_.Run(Result::kError);
    return;
  }

  // Get AuthFactorsConfiguration again, as it was cleared after
  // rotation.
  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::move(user_context),
      base::BindOnce(&CryptohomeRecoveryScreen::OnRefreshFactorsConfiguration,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeRecoveryScreen::OnRefreshFactorsConfiguration(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to get auth factors configuration, code "
               << error->get_cryptohome_error();
    context()->user_context = std::move(user_context);
    context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
    exit_callback_.Run(Result::kError);
    return;
  }
  context()->extra_factors_token =
      ash::AuthSessionStorage::Get()->Store(std::move(user_context));
  exit_callback_.Run(Result::kAuthenticated);
}

}  // namespace ash
