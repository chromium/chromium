// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/saml_confirm_password_screen.h"

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/check_passwords_against_cryptohome_helper.h"
#include "chrome/browser/ui/webui/ash/login/saml_confirm_password_handler.h"
#include "chromeos/ash/components/login/auth/public/auth_types.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"

namespace ash {

// static
std::string SamlConfirmPasswordScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kCancel:
      return "Cancel";
    case Result::kTooManyAttempts:
      return "TooManyAttempts";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

SamlConfirmPasswordScreen::SamlConfirmPasswordScreen(
    base::WeakPtr<SamlConfirmPasswordView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(SamlConfirmPasswordView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

SamlConfirmPasswordScreen::~SamlConfirmPasswordScreen() = default;

void SamlConfirmPasswordScreen::SetContextAndPasswords(
    std::unique_ptr<UserContext> user_context,
    ::login::StringList scraped_saml_passwords) {
  user_context_ = std::move(user_context);
  scraped_saml_passwords_ = std::move(scraped_saml_passwords);
  DCHECK_NE(scraped_saml_passwords_.size(), 1u);
}

void SamlConfirmPasswordScreen::TryPassword(const std::string& password) {
  Key key(password);
  key.SetLabel(kCryptohomeGaiaKeyLabel);
  if (scraped_saml_passwords_.empty() ||
      base::Contains(scraped_saml_passwords_, password)) {
    user_context_->SetKey(key);
    user_context_->SetSamlPassword(SamlPassword{password});
    user_context_->SetPasswordKey(Key(password));
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

  if (!view_)
    return;
  view_->ShowPasswordStep(/*retry=*/true);
}

void SamlConfirmPasswordScreen::ShowImpl() {
  if (!view_)
    return;
  view_->Show(user_context_->GetAccountId().GetUserEmail(),
              scraped_saml_passwords_.empty());
  if (!features::IsCheckPasswordsAgainstCryptohomeHelperEnabled() ||
      scraped_saml_passwords_.empty()) {
    ShowPasswordStep(/*retry=*/false);
    return;
  }
  check_passwords_against_cryptohome_helper_ =
      std::make_unique<CheckPasswordsAgainstCryptohomeHelper>(
          *user_context_.get(), scraped_saml_passwords_,
          base::BindOnce(&SamlConfirmPasswordScreen::ShowPasswordStep,
                         base::Unretained(this),/*retry=*/false),
          base::BindOnce(&SamlConfirmPasswordScreen::TryPassword,
                         base::Unretained(this)));
}

void SamlConfirmPasswordScreen::ShowPasswordStep(bool retry) {
  if (!view_)
    return;
  view_->ShowPasswordStep(retry);
}

void SamlConfirmPasswordScreen::HideImpl() {}

void SamlConfirmPasswordScreen::OnUserAction(const base::Value::List& args) {
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

}  // namespace ash
