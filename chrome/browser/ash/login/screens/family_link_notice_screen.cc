// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/family_link_notice_screen.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/family_link_notice_screen_handler.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {
constexpr char kUserActionContinue[] = "continue";
}  // namespace

namespace chromeos {

std::string FamilyLinkNoticeScreen::GetResultString(Result result) {
  switch (result) {
    case Result::DONE:
      return "Done";
    case Result::SKIPPED:
      return BaseScreen::kNotApplicable;
  }
}

FamilyLinkNoticeScreen::FamilyLinkNoticeScreen(
    FamilyLinkNoticeView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(FamilyLinkNoticeView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

FamilyLinkNoticeScreen::~FamilyLinkNoticeScreen() {
  if (view_)
    view_->Unbind();
}

void FamilyLinkNoticeScreen::OnViewDestroyed(FamilyLinkNoticeView* view) {
  if (view_ == view)
    view_ = nullptr;
}

bool FamilyLinkNoticeScreen::MaybeSkip(WizardContext* context) {
  if (features::IsChildSpecificSigninEnabled() && context->sign_in_as_child &&
      !ProfileManager::GetActiveUserProfile()->IsChild()) {
    return false;
  }
  exit_callback_.Run(Result::SKIPPED);
  return true;
}

void FamilyLinkNoticeScreen::ShowImpl() {
  if (!view_)
    return;
  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (profile->GetProfilePolicyConnector()->IsManaged() &&
      !profile->IsChild()) {
    std::string display_email =
        user_manager::UserManager::Get()->GetActiveUser()->GetDisplayEmail();
    view_->SetDisplayEmail(display_email);
    view_->SetDomain(gaia::ExtractDomainName(display_email));
  } else {
    view_->SetIsNewGaiaAccount(context()->is_child_gaia_account_new);
  }
  view_->Show();
}

void FamilyLinkNoticeScreen::HideImpl() {}

void FamilyLinkNoticeScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionContinue) {
    exit_callback_.Run(Result::DONE);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

}  // namespace chromeos
