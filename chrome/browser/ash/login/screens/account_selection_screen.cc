// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/account_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/account_selection_screen_handler.h"

namespace ash {
namespace {

constexpr char kUserActionReuseAccount[] = "reuseAccountFromEnrollment";
constexpr char kUserActionSigninAgain[] = "signinAgain";

}  // namespace

// static
std::string AccountSelectionScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kGaiaFallback:
      return "GaiaFallback";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

AccountSelectionScreen::AccountSelectionScreen(
    base::WeakPtr<AccountSelectionScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(AccountSelectionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

AccountSelectionScreen::~AccountSelectionScreen() = default;

bool AccountSelectionScreen::MaybeSkip(WizardContext& context) {
  if (!features::IsOobeAddUserDuringEnrollmentEnabled() ||
      !IsUserContextComplete(&context)) {
    std::move(exit_callback_).Run(Result::kGaiaFallback);
    return true;
  }

  return false;
}

void AccountSelectionScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  CHECK(context());
  CHECK(IsUserContextComplete(context()));
  const std::string email =
      context()->timebound_user_context_holder->GetAccountId().GetUserEmail();
  view_->SetUserEmail(email);
  view_->Show();
}

void AccountSelectionScreen::HideImpl() {}

void AccountSelectionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionReuseAccount) {
    if (!MaybeLoginWithCachedCredentials()) {
      exit_callback_.Run(Result::kGaiaFallback);
    }
  } else if (action_id == kUserActionSigninAgain) {
    exit_callback_.Run(Result::kGaiaFallback);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void AccountSelectionScreen::OnCredentialsExpiredCallback() {
  if (!is_hidden()) {
    std::move(exit_callback_).Run(Result::kGaiaFallback);
  }
}

bool AccountSelectionScreen::IsUserContextComplete(
    const WizardContext* const wizard_context) const {
  if (!wizard_context) {
    return false;
  }
  const TimeboundUserContextHolder* const user_context_holder =
      wizard_context->timebound_user_context_holder.get();
  if (!user_context_holder || !user_context_holder->HasUserContext()) {
    return false;
  }
  const bool user_context_available =
      !user_context_holder->GetAccountId().empty() &&
      user_context_holder->GetPassword() &&
      !user_context_holder->GetRefreshToken().empty();
  if (!user_context_available) {
    return false;
  }

  return true;
}

bool AccountSelectionScreen::MaybeLoginWithCachedCredentials() {
  CHECK(features::IsOobeAddUserDuringEnrollmentEnabled());
  WizardContext* wizard_context = context();
  CHECK(wizard_context);
  if (!IsUserContextComplete(wizard_context)) {
    return false;
  }

  if (view_) {
    view_->ShowStepProgress();
  }

  std::unique_ptr<UserContext> user_context =
      wizard_context->timebound_user_context_holder->GetUserContext();
  wizard_context->timebound_user_context_holder.reset();
  LoginDisplayHost::default_host()->CompleteLogin(*std::move(user_context));

  return true;
}

}  // namespace ash
