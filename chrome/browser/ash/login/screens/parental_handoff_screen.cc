// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/parental_handoff_screen.h"

#include <string>

#include "chrome/browser/ash/child_accounts/family_features.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/parental_handoff_screen_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/supervised_user/core/common/features.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

constexpr char kUserActionNext[] = "next";

std::u16string GetActiveUserName() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user || !user->IsChild())
    return std::u16string();
  return user->GetDisplayName();
}

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
      !IsFamilyLinkOobeHandoffEnabled()) {
    exit_callback_.Run(Result::kSkipped);
    return true;
  }

  const Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!profile->IsChild()) {
    exit_callback_.Run(Result::kSkipped);
    return true;
  }

  return false;
}

void ParentalHandoffScreen::ShowImpl() {
  if (!view_)
    return;

  view_->Show(GetActiveUserName());
}

void ParentalHandoffScreen::HideImpl() {}

void ParentalHandoffScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNext) {
    exit_callback_.Run(Result::kDone);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
