// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/saml_confirm_password_screen.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/not_fatal_until.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/saml_confirm_password_handler.h"
#include "chromeos/ash/components/login/auth/public/auth_types.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "chromeos/ash/components/osauth/public/auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/login/base_screen_handler_utils.h"

namespace ash {
namespace {

void StoreSamlPassword(const std::string& password, UserContext& user_context) {
  Key key(password);
  key.SetLabel(kCryptohomeGaiaKeyLabel);
  user_context.SetKey(key);
  user_context.SetSamlPassword(SamlPassword{password});
  user_context.SetPasswordKey(Key(password));
}

bool HasAllowedLocalAuthFactors(const AccountId& account_id) {
  AuthPolicyConnector* connector = AuthPolicyConnector::Get();
  if (!connector) {
    return false;
  }
  auto auth_factors_set = connector->AllowedLocalAuthFactors(account_id);
  return auth_factors_set.has_value() && !auth_factors_set->empty();
}

const UserContext* GetUserContext(const WizardContext& wizard_context) {
  if (wizard_context.user_context != nullptr) {
    return wizard_context.user_context.get();
  }

  CHECK(wizard_context.extra_factors_token.has_value())
      << "Extra factors token is required for SAML confirm password flow for "
         "new users";
  return AuthSessionStorage::Get()->Peek(
      wizard_context.extra_factors_token.value());
}

}  // namespace

SamlContext::SamlContext(std::vector<std::string> scraped_saml_passwords,
                         AccountId account_id)
    : scraped_saml_passwords(std::move(scraped_saml_passwords)),
      account_id(account_id) {}

SamlContext::~SamlContext() = default;

// static
std::string SamlConfirmPasswordScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kSuccess:
      return "Success";
    case Result::kCancel:
      return "Cancel";
    case Result::kTooManyAttempts:
      return "TooManyAttempts";
    case Result::kNotApplicable:
      return kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

SamlConfirmPasswordScreen::SamlConfirmPasswordScreen(
    base::WeakPtr<SamlConfirmPasswordView> view,
    const ScreenExitCallback& exit_callback)
    : BaseOSAuthSetupScreen(SamlConfirmPasswordView::kScreenId,
                            OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

SamlConfirmPasswordScreen::~SamlConfirmPasswordScreen() = default;

bool SamlConfirmPasswordScreen::MaybeSkip(WizardContext& context) {
  if (!features::IsManagedLocalPinAndPasswordEnabled()) {
    return false;
  }
  bool isInitialSetup = context.knowledge_factor_setup.auth_setup_flow ==
                        WizardContext::AuthChangeFlow::kInitialSetup;
  // Safe to use `GetUserContext` to get the context via
  // `AuthSessionStorage::Peek`, as it is called within `MaybeSkip` between
  // screens.
  bool hasAllowedLocalAuthFactors =
      HasAllowedLocalAuthFactors(GetUserContext(context)->GetAccountId());
  if (isInitialSetup && hasAllowedLocalAuthFactors) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void SamlConfirmPasswordScreen::SetContextAndPasswords(
    std::unique_ptr<UserContext> user_context,
    ::login::StringList scraped_saml_passwords) {
  user_context_ = std::move(user_context);
  scraped_saml_passwords_ = std::move(scraped_saml_passwords);
  DCHECK_NE(scraped_saml_passwords_.size(), 1u);
}

void SamlConfirmPasswordScreen::ObtainContextAndStoreSamlPassword(
    const std::string password) {
  if (context()->user_context != nullptr) {
    StoreSamlPassword(password, *context()->user_context);
    exit_callback_.Run(Result::kSuccess);
  } else {
    auto set_password_callback = base::BindOnce(
        &SamlConfirmPasswordScreen::SetPasswordAndReturnContextWithExitSuccess,
        weak_factory_.GetWeakPtr(), password);
    AuthSessionStorage::Get()->BorrowAsync(
        FROM_HERE, context()->extra_factors_token.value(),
        std::move(set_password_callback));
  }
}

void SamlConfirmPasswordScreen::TryPassword(const std::string& password) {
  if (features::IsManagedLocalPinAndPasswordEnabled()) {
    const ::login::StringList scraped_saml_passwords =
        saml_context_->scraped_saml_passwords;
    if (scraped_saml_passwords.empty() ||
        std::ranges::contains(scraped_saml_passwords, password)) {
      ObtainContextAndStoreSamlPassword(password);
      return;
    }

    if (++attempt_count_ >= 2) {
      ResetSecretsAndExit();
      return;
    }
  } else {
    if (scraped_saml_passwords_.empty() ||
        std::ranges::contains(scraped_saml_passwords_, password)) {
      StoreSamlPassword(password, *user_context_.get());

      LoginDisplayHost::default_host()->CompleteLogin(*user_context_);

      user_context_.reset();
      scraped_saml_passwords_.clear();
      return;
    }

    if (++attempt_count_ >= 2) {
      user_context_.reset();
      scraped_saml_passwords_.clear();
      exit_callback_.Run(Result::kTooManyAttempts);
      return;
    }
  }

  if (!view_)
    return;
  view_->ShowPasswordStep(/*retry=*/true);
}

void SamlConfirmPasswordScreen::ShowImpl() {
  if (!view_)
    return;
  if (features::IsManagedLocalPinAndPasswordEnabled()) {
    InspectContextAndContinue(
        base::BindOnce(&SamlConfirmPasswordScreen::InspectContextAndShowImpl,
                       weak_factory_.GetWeakPtr()),
        base::DoNothing());
  } else {
    ShowImplInternal(user_context_->GetAccountId(), scraped_saml_passwords_);
  }
}
void SamlConfirmPasswordScreen::InspectContextAndShowImpl(
    UserContext* user_context) {
  CHECK(features::IsManagedLocalPinAndPasswordEnabled());
  // Fallback to `UserContext` from the wizard controller, in case it isn't
  // available in the callback. This can happen when this screen is shown before
  // the user is mounted and has a valid AuthSession.
  UserContext* user_context_with_fallback =
      user_context != nullptr ? user_context : context()->user_context.get();

  CHECK(user_context_with_fallback)
      << "User Context is missing, cannot continue.";

  saml_context_ = std::make_unique<SamlContext>(
      user_context_with_fallback->GetScrapedSamlPasswords(),
      user_context_with_fallback->GetAccountId());
  ShowImplInternal(saml_context_->account_id,
                   saml_context_->scraped_saml_passwords);
}

void SamlConfirmPasswordScreen::ShowImplInternal(
    AccountId account_id,
    std::vector<std::string> scraped_saml_passwords) {
  view_->Show(account_id.GetUserEmail(), scraped_saml_passwords.empty());
  ShowPasswordStep(/*retry=*/false);
}

void SamlConfirmPasswordScreen::ShowPasswordStep(bool retry) {
  if (!view_)
    return;
  view_->ShowPasswordStep(retry);
}

void SamlConfirmPasswordScreen::HideImpl() {
  BaseOSAuthSetupScreen::HideImpl();
}

void SamlConfirmPasswordScreen::OnUserAction(const base::ListValue& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == "inputPassword") {
    CHECK_EQ(args.size(), 2u);
    TryPassword(args[1].GetString());
    return;
  }

  if (action_id == "cancel") {
    exit_callback_.Run(Result::kCancel);
    return;
  }

  BaseScreen::OnUserAction(args);
}

void SamlConfirmPasswordScreen::SetPasswordAndReturnContextWithExitSuccess(
    const std::string password,
    std::unique_ptr<UserContext> user_context) {
  if (!user_context) {
    NOTREACHED(base::NotFatalUntil::M160) << "UserContext was not present.";
    return;
  }
  CHECK(context()->extra_factors_token.has_value())
      << "Extra factors token is required for SAML confirm password flow";
  StoreSamlPassword(password, *user_context);

  AuthSessionStorage::Get()->Return(context()->extra_factors_token.value(),
                                    std::move(user_context));
  // UserContext is only borrowed and returned when we actually set
  // the password.
  exit_callback_.Run(Result::kSuccess);
}

void SamlConfirmPasswordScreen::ResetSecretsAndExit() {
  saml_context_.reset();
  WizardContext* wizard_context = context();
  if (!wizard_context->user_context) {
    // Do nothing, as the auth session storage and `UserContext#ClearSecrets`
    // will take care of clearing the context and secrets.
    exit_callback_.Run(Result::kTooManyAttempts);
    return;
  }
  // The scraped passwords will automatically be reset when we reset the user
  // context.
  wizard_context->user_context.reset();
  exit_callback_.Run(Result::kTooManyAttempts);
}

}  // namespace ash
