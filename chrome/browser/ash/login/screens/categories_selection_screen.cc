// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/categories_selection_screen.h"

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/categories_selection_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionSkip[] = "skip";

}  // namespace

// static
std::string CategoriesSelectionScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kSkip:
      return "Skip";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

CategoriesSelectionScreen::CategoriesSelectionScreen(
    base::WeakPtr<CategoriesSelectionScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(CategoriesSelectionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

CategoriesSelectionScreen::~CategoriesSelectionScreen() = default;

bool CategoriesSelectionScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  Profile* profile = ProfileManager::GetActiveUserProfile();

  bool is_managed_account = profile->GetProfilePolicyConnector()->IsManaged();
  bool is_child_account = user_manager->IsLoggedInAsChildUser();
  if (is_managed_account || is_child_account) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void CategoriesSelectionScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  view_->Show();
  // TODO(b/337674429) query the endpoint to retrieve list of categories
  //  and set it in the UI.
}

void CategoriesSelectionScreen::HideImpl() {}

void CategoriesSelectionScreen::OnSelect(base::Value::List categories) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetList(prefs::kOobeCategoriesSelected, std::move(categories));
}

void CategoriesSelectionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionSkip) {
    exit_callback_.Run(Result::kSkip);
    return;
  }

  if (action_id == kUserActionNext) {
    CHECK_EQ(args.size(), 2u);
    OnSelect(args[1].GetList().Clone());
    exit_callback_.Run(Result::kNext);
    return;
  }

  BaseScreen::OnUserAction(args);
}

}  // namespace ash
