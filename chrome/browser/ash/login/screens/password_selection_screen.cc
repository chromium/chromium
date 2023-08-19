// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/password_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"

namespace ash {

namespace {

constexpr char kUserActionBack[] = "back";
constexpr char kUserActionLocalPassword[] = "local-password";
constexpr char kUserActionGaiaPassword[] = "gaia-password";

// Returns `true` if the active Profile is enterprise managed.
bool IsUserEnterpriseManaged() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
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
}

void PasswordSelectionScreen::HideImpl() {}

void PasswordSelectionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::BACK);
    return;
  }
  if (action_id == kUserActionLocalPassword) {
    exit_callback_.Run(Result::LOCAL_PASSWORD);
    return;
  }
  if (action_id == kUserActionGaiaPassword) {
    exit_callback_.Run(Result::GAIA_PASSWORD);
    return;
  }
  BaseScreen::OnUserAction(args);
}

bool PasswordSelectionScreen::MaybeSkip(WizardContext& wizard_context) {
  UserContext* user_context = nullptr;
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
  if (IsUserEnterpriseManaged()) {
    LOG(WARNING) << "Managed user must use Gaia password.";
    exit_callback_.Run(Result::GAIA_PASSWORD);
    return true;
  }
  return false;
}

}  // namespace ash
