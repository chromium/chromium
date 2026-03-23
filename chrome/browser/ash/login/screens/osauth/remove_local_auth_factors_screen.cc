// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/remove_local_auth_factors_screen.h"

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/remove_local_auth_factors_screen_handler.h"

namespace ash {
namespace {

constexpr char kUserActionDoneButtonClicked[] = "done";

}  // namespace

// static
std::string RemoveLocalAuthFactorsScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kSuccess:
      return "Next";
    case Result::kError:
      return "Error";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

RemoveLocalAuthFactorsScreen::RemoveLocalAuthFactorsScreen(
    base::WeakPtr<RemoveLocalAuthFactorsScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseOSAuthSetupScreen(RemoveLocalAuthFactorsScreenView::kScreenId,
                            OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

RemoveLocalAuthFactorsScreen::~RemoveLocalAuthFactorsScreen() = default;

void RemoveLocalAuthFactorsScreen::InspectContext(UserContext* user_context) {
  if (!user_context) {
    LOG(ERROR) << "Session expired while waiting for user's decision";
    context()->osauth_error = WizardContext::OSAuthErrorKind::kFatal;
    exit_callback_.Run(Result::kError);
    return;
  }
  CHECK(user_context->HasAuthFactorsConfiguration());
}

void RemoveLocalAuthFactorsScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();
  view_->ShowRemoveLocalAuthFactorsSuccessStep();
}

void RemoveLocalAuthFactorsScreen::HideImpl() {}

void RemoveLocalAuthFactorsScreen::OnUserAction(const base::ListValue& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionDoneButtonClicked) {
    exit_callback_.Run(Result::kSuccess);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
