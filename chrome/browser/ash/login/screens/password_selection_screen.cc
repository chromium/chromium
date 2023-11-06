// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/password_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/password_factor_editor.h"

namespace ash {

namespace {

constexpr char kUserActionBack[] = "back";
constexpr char kUserActionLocalPassword[] = "local-password";
constexpr char kUserActionGaiaPassword[] = "gaia-password";

// Returns `true` if the active Profile is enterprise managed.
bool IsUserEnterpriseManaged() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  return profile->GetProfilePolicyConnector()->IsManaged() &&
         !profile->IsChild();
}

}  // namespace

// static
std::string PasswordSelectionScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
    case Result::BACK:
      return "Back";
    case Result::LOCAL_PASSWORD:
      return "LocalPassword";
    case Result::GAIA_PASSWORD:
      return "GaiaPassword";
  }
}

PasswordSelectionScreen::PasswordSelectionScreen(
    base::WeakPtr<PasswordSelectionScreenView> view,
    ScreenExitCallback exit_callback)
    : BaseScreen(PasswordSelectionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(std::move(exit_callback)) {}

PasswordSelectionScreen::~PasswordSelectionScreen() = default;

void PasswordSelectionScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  view_->Show();
  CheckPasswordPresence();
}

void PasswordSelectionScreen::HideImpl() {
  online_password_ = absl::nullopt;
}

void PasswordSelectionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::BACK);
    return;
  }
  if (action_id == kUserActionLocalPassword) {
    LOG(WARNING) << "Choice : Local password";
    context()->knowledge_factor_setup.local_password_forced = false;
    exit_callback_.Run(Result::LOCAL_PASSWORD);
    return;
  }
  if (action_id == kUserActionGaiaPassword) {
    LOG(WARNING) << "Choice : Online password";
    SetGaiaPassword();
    return;
  }
  BaseScreen::OnUserAction(args);
}

bool PasswordSelectionScreen::MaybeSkip(WizardContext& wizard_context) {
  CHECK(features::AreLocalPasswordsEnabledForConsumers());
  // We do not check context.skip_post_login_screens_for_tests here,
  // as we need to set up GAIA password anyway.
  const UserContext* user_context = nullptr;
  if (ash::features::ShouldUseAuthSessionStorage()) {
    CHECK(wizard_context.extra_factors_token.has_value());
    auto* storage = ash::AuthSessionStorage::Get();
    auto& token = wizard_context.extra_factors_token.value();
    CHECK(storage->IsValid(token));
    user_context = storage->Peek(token);
  } else {
    user_context = wizard_context.extra_factors_auth_session.get();
  }

  CHECK(user_context);
  CHECK(user_context->HasAuthFactorsConfiguration());
  if (user_context->GetAuthFactorsConfiguration().HasConfiguredFactor(
          cryptohome::AuthFactorType::kPassword)) {
    LOG(WARNING) << "User already has a password configured.";
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void PasswordSelectionScreen::CheckPasswordPresence() {
  if (ash::features::ShouldUseAuthSessionStorage()) {
    CHECK(context()->extra_factors_token.has_value());
    auto* storage = ash::AuthSessionStorage::Get();
    auto& token = context()->extra_factors_token.value();
    CHECK(storage->IsValid(token));
    storage->BorrowAsync(
        FROM_HERE, token,
        base::BindOnce(
            &PasswordSelectionScreen::CheckPasswordPresenceWithContext,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    CheckPasswordPresenceWithContext(
        std::move(context()->extra_factors_auth_session));
  }
}

void PasswordSelectionScreen::CheckPasswordPresenceWithContext(
    std::unique_ptr<UserContext> user_context) {
  if (!user_context) {
    // Session have expired
    LOG(ERROR) << "Session expired while waiting for user's decision";
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return;
  }

  // Store value so that we can later use it without obtaining context.
  online_password_ = user_context->GetOnlinePassword();
  bool has_online_password = online_password_.has_value();

  bool has_password_in_cryptohme =
      user_context->GetAuthFactorsConfiguration().HasConfiguredFactor(
          cryptohome::AuthFactorType::kPassword);

  if (ash::features::ShouldUseAuthSessionStorage()) {
    CHECK(context()->extra_factors_token.has_value());
    auto* storage = ash::AuthSessionStorage::Get();
    auto& token = context()->extra_factors_token.value();
    storage->Return(token, std::move(user_context));
  } else {
    context()->extra_factors_auth_session = std::move(user_context);
  }

  if (context()->skip_post_login_screens_for_tests) {
    LOG(WARNING) << "Skipping post-login screens, assuming that user has "
                    "online password";
    CHECK(has_online_password)
        << "Skipping post-login screens requires non-empty GAIA password";
    SetGaiaPassword();
    return;
  }

  if (has_password_in_cryptohme) {
    LOG(WARNING) << "User already has a password configured.";
    exit_callback_.Run(Result::GAIA_PASSWORD);
    return;
  }

  if (IsUserEnterpriseManaged()) {
    LOG(WARNING) << "Managed user must use online password.";
    CHECK(has_online_password)
        << "Managed users should have online password by this point";
    SetGaiaPassword();
    return;
  }

  if (!has_online_password) {
    LOG(WARNING)
        << "User does not have online password, forcing local password";
    context()->knowledge_factor_setup.local_password_forced = true;
    exit_callback_.Run(Result::LOCAL_PASSWORD);
    return;
  }
  view_->ShowPasswordChoice();
}

void PasswordSelectionScreen::SetGaiaPassword() {
  CHECK(online_password_.has_value());
  view_->ShowProgress();

  auth::mojom::PasswordFactorEditor& password_factor_editor =
      auth::GetPasswordFactorEditor(
          quick_unlock::QuickUnlockFactory::GetDelegate(),
          g_browser_process->local_state());

  password_factor_editor.SetOnlinePassword(
      GetToken(), online_password_.value().value(),
      base::BindOnce(&PasswordSelectionScreen::OnOnlinePasswordSet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PasswordSelectionScreen::OnOnlinePasswordSet(
    auth::mojom::ConfigureResult result) {
  if (result != auth::mojom::ConfigureResult::kSuccess) {
    // TODO(b/291808449): Error handling.
    LOG(ERROR) << "Could not set online password";
  } else {
    exit_callback_.Run(Result::GAIA_PASSWORD);
  }
  return;
}

std::string PasswordSelectionScreen::GetToken() const {
  if (ash::features::ShouldUseAuthSessionStorage()) {
    CHECK(context()->extra_factors_token.has_value());
    return context()->extra_factors_token.value();
  } else {
    CHECK(context()->extra_factors_auth_session);

    quick_unlock::QuickUnlockStorage* quick_unlock_storage =
        quick_unlock::QuickUnlockFactory::GetForProfile(
            ProfileManager::GetActiveUserProfile());
    CHECK(quick_unlock_storage);
    return quick_unlock_storage->CreateAuthToken(
        *context()->extra_factors_auth_session);
  }
}

}  // namespace ash
