// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/enter_old_password_screen.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/enter_old_password_screen_handler.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/error_util.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {
namespace {

constexpr const char kUserActionForgotOldPassword[] = "forgot";
constexpr const char kUserActionSubmitOldPassword[] = "submit";

}  // namespace

// static
std::string EnterOldPasswordScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kForgotOldPassword:
      return "ForgotOldPassword";
    case Result::kCryptohomeError:
      return "CryptohomeError";
    case Result::kAuthenticated:
      return "Authenticated";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

EnterOldPasswordScreen::EnterOldPasswordScreen(
    base::WeakPtr<EnterOldPasswordScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseOSAuthSetupScreen(EnterOldPasswordScreenView::kScreenId,
                            OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback),
      auth_performer_(
          std::make_unique<AuthPerformer>(UserDataAuthClient::Get())) {}

EnterOldPasswordScreen::~EnterOldPasswordScreen() = default;

void EnterOldPasswordScreen::ShowImpl() {
  view_->Show();
}

void EnterOldPasswordScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionForgotOldPassword) {
    exit_callback_.Run(Result::kForgotOldPassword);
    return;
  }

  if (action_id == kUserActionSubmitOldPassword) {
    const std::string& old_password = args[1].GetString();
    AttemptAuthentication(old_password);
    return;
  }

  BaseOSAuthSetupScreen::OnUserAction(args);
}

void EnterOldPasswordScreen::AttemptAuthentication(
    const std::string& old_password) {
  DCHECK(!context()->user_context->GetAuthSessionId().empty());
  auto* factor =
      context()->user_context->GetAuthFactorsConfiguration().FindFactorByType(
          cryptohome::AuthFactorType::kPassword);
  DCHECK(factor);
  auth_performer_->AuthenticateWithPassword(
      factor->ref().label().value(), old_password,
      std::move(context()->user_context),
      base::BindOnce(&EnterOldPasswordScreen::OnPasswordAuthentication,
                     weak_factory_.GetWeakPtr()));
}

void EnterOldPasswordScreen::OnPasswordAuthentication(
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    context()->user_context = std::move(user_context);
    if (cryptohome::ErrorMatches(
            error->get_cryptohome_error(),
            user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED)) {
      view_->ShowWrongPasswordError();
      return;
    }
    context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
    exit_callback_.Run(Result::kCryptohomeError);
    return;
  }

  AuthProofToken token =
      AuthSessionStorage::Get()->Store(std::move(user_context));
  context()->extra_factors_token.emplace(token);

  exit_callback_.Run(Result::kAuthenticated);
}

}  // namespace ash
