// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/parental_handoff_screen.h"

#include <string>

#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_features.h"
#include "chrome/browser/ui/webui/chromeos/login/parental_handoff_screen_handler.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

constexpr char kUserActionNext[] = "next";

base::string16 GetActiveUserName() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user || !user->IsChild())
    return base::string16();
  return user->GetDisplayName();
}

}  // namespace

// static
std::string ParentalHandoffScreen::GetResultString(
    ParentalHandoffScreen::Result result) {
  switch (result) {
    case ParentalHandoffScreen::Result::DONE:
      return "Done";
    case ParentalHandoffScreen::Result::SKIPPED:
      return BaseScreen::kNotApplicable;
  }
}

ParentalHandoffScreen::ParentalHandoffScreen(
    ParentalHandoffScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ParentalHandoffScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

ParentalHandoffScreen::~ParentalHandoffScreen() {
  if (view_)
    view_->Unbind();
}

void ParentalHandoffScreen::OnViewDestroyed(ParentalHandoffScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool ParentalHandoffScreen::MaybeSkip(WizardContext* context) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile->IsChild() && supervised_users::IsEduCoexistenceFlowV2Enabled()) {
    return false;
  }

  exit_callback_.Run(Result::SKIPPED);
  return true;
}

void ParentalHandoffScreen::ShowImpl() {
  if (!view_)
    return;

  base::string16 user_name = GetActiveUserName();

  view_->Show(l10n_util::GetStringFUTF16(
                  IDS_LOGIN_PARENTAL_HANDOFF_SCREEN_TITLE, user_name),
              l10n_util::GetStringFUTF16(
                  IDS_LOGIN_PARENTAL_HANDOFF_SCREEN_SUBTITLE, user_name));
}
void ParentalHandoffScreen::HideImpl() {}

void ParentalHandoffScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionNext) {
    exit_callback_.Run(Result::DONE);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

}  // namespace chromeos
