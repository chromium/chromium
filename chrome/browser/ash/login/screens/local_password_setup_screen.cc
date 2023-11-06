// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/local_password_setup_screen.h"

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/values.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/local_password_setup_handler.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-forward.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-shared.h"
#include "components/crash/core/app/crashpad.h"

namespace ash {
namespace {

constexpr const char kUserActionInputPassword[] = "inputPassword";
constexpr const char kUserActionBack[] = "back";
constexpr const char kUserActionDone[] = "done";

}  // namespace

// static
std::string LocalPasswordSetupScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kDone:
      return "Done";
    case Result::kBack:
      return "Back";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

LocalPasswordSetupScreen::LocalPasswordSetupScreen(
    base::WeakPtr<LocalPasswordSetupView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(LocalPasswordSetupView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

LocalPasswordSetupScreen::~LocalPasswordSetupScreen() = default;

void LocalPasswordSetupScreen::ShowImpl() {
  CHECK(!context()->skip_post_login_screens_for_tests);

  if (!view_) {
    return;
  }
  bool can_go_back = !context()->knowledge_factor_setup.local_password_forced;
  view_->Show(can_go_back, context()->knowledge_factor_setup.auth_setup_flow ==
                               WizardContext::AuthChangeFlow::kRecovery);
}

void LocalPasswordSetupScreen::HideImpl() {}

void LocalPasswordSetupScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionInputPassword) {
    CHECK_EQ(args.size(), 2u);
    const std::string& password = args[1].GetString();
    auth::mojom::PasswordFactorEditor& password_factor_editor =
        auth::GetPasswordFactorEditor(
            quick_unlock::QuickUnlockFactory::GetDelegate(),
            g_browser_process->local_state());

    if (context()->knowledge_factor_setup.auth_setup_flow ==
        WizardContext::AuthChangeFlow::kRecovery) {
      password_factor_editor.UpdateLocalPassword(
          GetToken(), password,
          base::BindOnce(&LocalPasswordSetupScreen::OnSetLocalPassword,
                         weak_factory_.GetWeakPtr()));
      return;
    }

    password_factor_editor.SetLocalPassword(
        GetToken(), password,
        base::BindOnce(&LocalPasswordSetupScreen::OnSetLocalPassword,
                       weak_factory_.GetWeakPtr()));

    return;
  } else if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::kBack);
    return;
  } else if (action_id == kUserActionDone) {
    exit_callback_.Run(Result::kDone);
    return;
  }
  BaseScreen::OnUserAction(args);
}

void LocalPasswordSetupScreen::OnUpdateLocalPassword(
    auth::mojom::ConfigureResult result) {
  if (result != auth::mojom::ConfigureResult::kSuccess) {
    view_->ShowLocalPasswordSetupFailure();
    LOG(ERROR) << "Failed to update local password, error id= "
               << static_cast<int>(result);
    exit_callback_.Run(Result::kDone);
    crash_reporter::DumpWithoutCrashing();
    return;
  }
  view_->ShowLocalPasswordSetupSuccess();
}

void LocalPasswordSetupScreen::OnSetLocalPassword(
    auth::mojom::ConfigureResult result) {
  if (result != auth::mojom::ConfigureResult::kSuccess) {
    view_->ShowLocalPasswordSetupFailure();
    LOG(ERROR) << "Failed to set local password, error id= "
               << static_cast<int>(result);
    exit_callback_.Run(Result::kDone);
    crash_reporter::DumpWithoutCrashing();
    return;
  }
  view_->ShowLocalPasswordSetupSuccess();
}

std::string LocalPasswordSetupScreen::GetToken() const {
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
