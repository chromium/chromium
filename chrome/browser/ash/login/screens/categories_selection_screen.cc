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

bool HasBeenSelected(std::string category_id) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  const base::Value::List& selected_categories =
      prefs->GetList(prefs::kOobeCategoriesSelected);
  for (const auto& category : selected_categories) {
    if (category.GetString() == category_id) {
      return true;
    }
  }
  return false;
}

}  // namespace

// static
std::string CategoriesSelectionScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kSkip:
      return "Skip";
    case Result::kError:
      return "Error";
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

  raw_ptr<OobeAppsDiscoveryService> oobe_apps_discovery_service_ =
      OobeAppsDiscoveryServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  oobe_apps_discovery_service_->GetAppsAndUseCases(
      base::BindOnce(&CategoriesSelectionScreen::OnResponseReceived,
                     weak_factory_.GetWeakPtr()));
}

void CategoriesSelectionScreen::HideImpl() {}

void CategoriesSelectionScreen::OnResponseReceived(
    const std::vector<OOBEAppDefinition>& appInfos,
    const std::vector<OOBEDeviceUseCase>& categories,
    AppsFetchingResult result) {
  if (result != AppsFetchingResult::kSuccess) {
    exit_callback_.Run(Result::kError);
  }

  base::Value::List categories_list;
  for (OOBEDeviceUseCase category : categories) {
    // Disregard the category with order 0, as it relates to a general,
    // non-specific use case (oobe_otheres).
    if (category.GetOrder() == 0) {
      continue;
    }
    base::Value::Dict category_dict;
    category_dict.Set("categoryId", base::Value(std::move(category.GetID())));
    category_dict.Set("title", base::Value(std::move(category.GetLabel())));
    category_dict.Set("subtitle",
                      base::Value(std::move(category.GetDescription())));
    category_dict.Set("icon", base::Value(std::move(category.GetImageURL())));
    category_dict.Set("selected", HasBeenSelected(category.GetID()));
    categories_list.Append(std::move(category_dict));
  }

  base::Value::Dict data;
  data.Set("categories", std::move(categories_list));
  if (view_) {
    view_->SetCategoriesData(std::move(data));
  }
}

void CategoriesSelectionScreen::OnSelect(base::Value::List categories) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  prefs->SetList(prefs::kOobeCategoriesSelected, std::move(categories));
}

void CategoriesSelectionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionSkip) {
    PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
    prefs->ClearPref(prefs::kOobeCategoriesSelected);
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
