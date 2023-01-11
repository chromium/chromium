// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/cryptohome_recovery_screen.h"

#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/reauth_stats.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_screen_handler.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"

namespace {

constexpr char kUserActionDone[] = "done";
constexpr char kUserActionRetry[] = "retry";
constexpr char kUserActionEnterOldPassword[] = "enter-old-password";
constexpr char kUserActionReauth[] = "reauth";

}  // namespace

namespace ash {

// static
std::string CryptohomeRecoveryScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kSucceeded:
      return "Succeeded";
    case Result::kGaiaLogin:
      return "GaiaLogin";
    case Result::kManualRecovery:
      return "ManualRecovery";
    case Result::kRetry:
      return "Retry";
    case Result::kNoRecoveryFactor:
      return "NoRecoveryFactor";
  }
}

CryptohomeRecoveryScreen::CryptohomeRecoveryScreen(
    base::WeakPtr<CryptohomeRecoveryScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(CryptohomeRecoveryScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
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
  if (action_id == kUserActionDone) {
    exit_callback_.Run(Result::kSucceeded);
  } else if (action_id == kUserActionRetry) {
    // TODO(b/257073746): We probably want to differentiate between retry with
    // or without login.
    RecordReauthReason(context()->user_context->GetAccountId(),
                       ReauthReason::kCryptohomeRecovery);
    exit_callback_.Run(Result::kRetry);
  } else if (action_id == kUserActionEnterOldPassword) {
    exit_callback_.Run(Result::kManualRecovery);
  } else if (action_id == kUserActionReauth) {
    exit_callback_.Run(Result::kGaiaLogin);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void CryptohomeRecoveryScreen::OnGetAuthFactorsConfiguration(
    std::unique_ptr<UserContext> user_context,
    absl::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to get auth factors configuration, code "
               << error->get_cryptohome_code();
    context()->user_context = std::move(user_context);
    view_->OnRecoveryFailed();
    return;
  }

  const auto& config = user_context->GetAuthFactorsConfiguration();
  bool is_configured =
      config.HasConfiguredFactor(cryptohome::AuthFactorType::kRecovery);
  if (is_configured) {
    if (user_context->GetReauthProofToken().empty()) {
      RecordReauthReason(user_context->GetAccountId(),
                         ReauthReason::kCryptohomeRecovery);
      context()->user_context = std::move(user_context);
      view_->ShowReauthNotification();
      return;
    }
    recovery_performer_ = std::make_unique<CryptohomeRecoveryPerformer>(
        UserDataAuthClient::Get(),
        g_browser_process->shared_url_loader_factory());
    recovery_performer_->AuthenticateWithRecovery(
        std::move(user_context),
        base::BindOnce(&CryptohomeRecoveryScreen::OnAuthenticateWithRecovery,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    context()->user_context = std::move(user_context);
    exit_callback_.Run(Result::kNoRecoveryFactor);
  }
}

void CryptohomeRecoveryScreen::OnAuthenticateWithRecovery(
    std::unique_ptr<UserContext> user_context,
    absl::optional<AuthenticationError> error) {
  if (error.has_value()) {
    LOG(ERROR) << "Failed to authenticate with recovery, code "
               << static_cast<int>(
                      error->get_cryptohome_recovery_server_error());
    context()->user_context = std::move(user_context);
    view_->OnRecoveryFailed();
    return;
  }

  std::string key_label;
  auto* password_factor =
      user_context->GetAuthFactorsData().FindOnlinePasswordFactor();
  DCHECK(password_factor);
  key_label = password_factor->ref().label().value();

  if (!user_context->HasReplacementKey()) {
    // Assume that there was an attempt to use the key, so it is was already
    // hashed.
    DCHECK(user_context->GetKey()->GetKeyType() !=
           Key::KEY_TYPE_PASSWORD_PLAIN);
    // Make sure that the key has correct label.
    user_context->GetKey()->SetLabel(key_label);
    user_context->SaveKeyForReplacement();
  }

  auth_factor_editor_.ReplaceContextKey(
      std::move(user_context),
      base::BindOnce(&CryptohomeRecoveryScreen::OnReplaceContextKey,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeRecoveryScreen::OnReplaceContextKey(
    std::unique_ptr<UserContext> user_context,
    absl::optional<AuthenticationError> error) {
  context()->user_context = std::move(user_context);
  if (error.has_value()) {
    LOG(ERROR) << "Failed to replace context key, code "
               << error->get_cryptohome_code();
    view_->OnRecoveryFailed();
    return;
  }
  view_->OnRecoverySucceeded();
}

}  // namespace ash
