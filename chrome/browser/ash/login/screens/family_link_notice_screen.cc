// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/family_link_notice_screen.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/family_link_notice_screen_handler.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {
namespace {

constexpr char kUserActionContinue[] = "continue";

}  // namespace

std::string FamilyLinkNoticeScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kDone:
      return "Done";
    case Result::kSkipped:
      return BaseScreen::kNotApplicable;
  }
}

FamilyLinkNoticeScreen::FamilyLinkNoticeScreen(
    base::WeakPtr<FamilyLinkNoticeView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(FamilyLinkNoticeView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

FamilyLinkNoticeScreen::~FamilyLinkNoticeScreen() = default;

bool FamilyLinkNoticeScreen::MaybeSkip(WizardContext& context) {
  if (!context.skip_post_login_screens_for_tests && context.sign_in_as_child &&
      !ProfileManager::GetActiveUserProfile()->IsChild()) {
    return false;
  }
  exit_callback_.Run(Result::kSkipped);
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

void FamilyLinkNoticeScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionContinue) {
    exit_callback_.Run(Result::kDone);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
