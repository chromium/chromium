// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/parental_handoff_screen.h"

#include <string>

#include "base/check_deref.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/parental_handoff_screen_handler.h"
#include "components/supervised_user/core/common/features.h"
#include "components/user_manager/user.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr char kUserActionNext[] = "next";

}  // namespace

// static
std::string ParentalHandoffScreen::GetResultString(
    ParentalHandoffScreen::Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case ParentalHandoffScreen::Result::kDone:
      return "Done";
    case ParentalHandoffScreen::Result::kSkipped:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

ParentalHandoffScreen::ParentalHandoffScreen(
    base::WeakPtr<ParentalHandoffScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ParentalHandoffScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

ParentalHandoffScreen::~ParentalHandoffScreen() = default;

bool ParentalHandoffScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests ||
      !CHECK_DEREF(context.onboarding_user.get()).IsChild()) {
    exit_callback_.Run(Result::kSkipped);
    return true;
  }

  return false;
}

void ParentalHandoffScreen::ShowImpl() {
  if (!view_)
    return;

  const user_manager::User& user =
      CHECK_DEREF(context()->onboarding_user.get());
  view_->Show(user.IsChild() ? user.GetDisplayName() : std::u16string());
}

void ParentalHandoffScreen::HideImpl() {}

void ParentalHandoffScreen::OnUserAction(const base::ListValue& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNext) {
    exit_callback_.Run(Result::kDone);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
