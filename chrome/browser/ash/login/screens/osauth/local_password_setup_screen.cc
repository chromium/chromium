// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/local_password_setup_screen.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/local_password_setup_handler.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-forward.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-shared.h"
#include "components/crash/core/app/crashpad.h"

namespace ash {
namespace {

constexpr const char kUserActionInputPassword[] = "inputPassword";
constexpr const char kUserActionBack[] = "back";

}  // namespace

// static
std::string LocalPasswordSetupScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kDone:
      return "Done";
    case Result::kBack:
      return "Back";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

LocalPasswordSetupScreen::LocalPasswordSetupScreen(
    base::WeakPtr<LocalPasswordSetupView> view,
    const ScreenExitCallback& exit_callback)
    : BaseOSAuthSetupScreen(LocalPasswordSetupView::kScreenId,
                            OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

LocalPasswordSetupScreen::~LocalPasswordSetupScreen() = default;

void LocalPasswordSetupScreen::ShowImpl() {
  CHECK(!context()->skip_post_login_screens_for_tests);

  if (!view_) {
    return;
  }
  EstablishKnowledgeFactorGuard(base::BindOnce(
      &LocalPasswordSetupScreen::DoShow, weak_factory_.GetWeakPtr()));
}

void LocalPasswordSetupScreen::DoShow() {
  bool can_go_back = !context()->knowledge_factor_setup.local_password_forced;
  bool is_recovery_flow = context()->knowledge_factor_setup.auth_setup_flow ==
                          WizardContext::AuthChangeFlow::kRecovery;
  view_->Show(/*can_go_back=*/can_go_back,
              /*is_recovery_flow=*/is_recovery_flow);
}

void LocalPasswordSetupScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionInputPassword) {
    CHECK_EQ(args.size(), 2u);
    const std::string& password = args[1].GetString();
    auth::mojom::PasswordFactorEditor& password_factor_editor =
        auth::GetPasswordFactorEditor(
            quick_unlock::QuickUnlockFactory::GetDelegate(),
            g_browser_process->local_state());
    switch (context()->knowledge_factor_setup.auth_setup_flow) {
      case WizardContext::AuthChangeFlow::kInitialSetup:
        password_factor_editor.SetLocalPassword(
            GetToken(), password,
            base::BindOnce(&LocalPasswordSetupScreen::OnSetLocalPassword,
                           weak_factory_.GetWeakPtr()));
        break;
      case WizardContext::AuthChangeFlow::kRecovery:
        password_factor_editor.UpdateOrSetLocalPassword(
            GetToken(), password,
            base::BindOnce(&LocalPasswordSetupScreen::OnUpdateLocalPassword,
                           weak_factory_.GetWeakPtr()));
        break;
      case WizardContext::AuthChangeFlow::kReauthentication:
        NOTREACHED_IN_MIGRATION();
    }
    return;
  } else if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::kBack);
    return;
  }
  BaseOSAuthSetupScreen::OnUserAction(args);
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
  context()->knowledge_factor_setup.modified_factors.Put(
      AshAuthFactor::kLocalPassword);
  exit_callback_.Run(Result::kDone);
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
  context()->knowledge_factor_setup.modified_factors.Put(
      AshAuthFactor::kLocalPassword);
  exit_callback_.Run(Result::kDone);
}

}  // namespace ash
