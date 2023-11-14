// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/factor_setup_success_screen.h"

#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/screens/osauth/base_osauth_setup_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/osauth/factor_setup_success_screen_handler.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"

namespace ash {
namespace {

// LINT.IfChange
constexpr const char kUserActionProceed[] = "proceed";

std::string GetChangeModeString(WizardContext::AuthChangeFlow flow) {
  switch (flow) {
    case WizardContext::AuthChangeFlow::kRecovery:
      return "update";
    case WizardContext::AuthChangeFlow::kInitialSetup:
      return "set";
  }
}

std::string GetModifiedFactorsString(AuthFactorsSet factors) {
  if (factors == AuthFactorsSet({AshAuthFactor::kGaiaPassword})) {
    return "online";
  } else if (factors == AuthFactorsSet({AshAuthFactor::kLocalPassword})) {
    return "local";
  } else if (factors == AuthFactorsSet({AshAuthFactor::kCryptohomePin})) {
    return "pin";
  } else if (factors == AuthFactorsSet({AshAuthFactor::kGaiaPassword,
                                        AshAuthFactor::kCryptohomePin})) {
    return "online+pin";
  } else if (factors == AuthFactorsSet({AshAuthFactor::kLocalPassword,
                                        AshAuthFactor::kCryptohomePin})) {
    return "local+pin";
  }
  // Fallback for sanity:
  return "online";
}
// LINT.ThenChange(/chrome/browser/resources/chromeos/login/screens/osauth/factor_setup_success.js)

}  // namespace

// static
std::string FactorSetupSuccessScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
    case Result::kProceed:
      return "Proceed";
  }
}

FactorSetupSuccessScreen::FactorSetupSuccessScreen(
    base::WeakPtr<FactorSetupSuccessScreenView> view,
    ScreenExitCallback exit_callback)
    : BaseScreen(FactorSetupSuccessScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(std::move(exit_callback)) {}

FactorSetupSuccessScreen::~FactorSetupSuccessScreen() = default;

bool FactorSetupSuccessScreen::MaybeSkip(WizardContext& wizard_context) {
  if (wizard_context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }
  if (wizard_context.knowledge_factor_setup.modified_factors.Empty()) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }
  return false;
}

void FactorSetupSuccessScreen::HideImpl() {}

void FactorSetupSuccessScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  base::Value::Dict params;
  params.Set("modifiedFactors",
             GetModifiedFactorsString(
                 context()->knowledge_factor_setup.modified_factors));
  params.Set(
      "changeMode",
      GetChangeModeString(context()->knowledge_factor_setup.auth_setup_flow));
  view_->Show(std::move(params));
}

void FactorSetupSuccessScreen::OnUserAction(const base::Value::List& args) {
  CHECK_GE(args.size(), 1u);
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionProceed) {
    exit_callback_.Run(Result::kProceed);
    return;
  }
  BaseScreen::OnUserAction(args);
}

}  // namespace ash
