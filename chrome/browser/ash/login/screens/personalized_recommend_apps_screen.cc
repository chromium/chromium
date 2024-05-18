// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/personalized_recommend_apps_screen.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_discovery_service.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_discovery_service_factory.h"
#include "chrome/browser/ash/login/oobe_apps_service/oobe_apps_types.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/personalized_recommend_apps_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionSkip[] = "skip";
constexpr const char kUserActionBack[] = "back";
constexpr const char kNoUseCasesSelectedIDName[] = "oobe_other";

}  // namespace

// static
std::string PersonalizedRecommendAppsScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kSkip:
      return "Skip";
    case Result::kBack:
      return "Back";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

PersonalizedRecommendAppsScreen::PersonalizedRecommendAppsScreen(
    base::WeakPtr<PersonalizedRecommendAppsScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(PersonalizedRecommendAppsScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

PersonalizedRecommendAppsScreen::~PersonalizedRecommendAppsScreen() = default;

bool PersonalizedRecommendAppsScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!arc::IsArcPlayStoreEnabledForProfile(profile)) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  bool is_managed_account = profile->GetProfilePolicyConnector()->IsManaged();
  bool is_child_account = user_manager->IsLoggedInAsChildUser();
  if (is_managed_account || is_child_account) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  // This screen is currently shown only to the new-to-ChromeOS users. To
  // understand which user is actually a new one we use a synced preference
  // `kOobeMarketingOptInScreenFinished` which is set only once for each user
  // in the end of the OOBE flow.
  // This logic might be replaced in the future by checking a list of potential
  // synced apps for the user, but at this moment this preference check is
  // sufficient for our needs.
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (prefs->GetBoolean(prefs::kOobeMarketingOptInScreenFinished) &&
      !ash::switches::ShouldSkipNewUserCheckForTesting()) {
    return true;
  }

  return false;
}

void PersonalizedRecommendAppsScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  raw_ptr<OobeAppsDiscoveryService> oobe_apps_discovery_service_ =
      OobeAppsDiscoveryServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  oobe_apps_discovery_service_->GetAppsAndUseCases(
      base::BindOnce(&PersonalizedRecommendAppsScreen::OnResponseReceived,
                     weak_factory_.GetWeakPtr()));

  view_->Show();
}

void PersonalizedRecommendAppsScreen::OnResponseReceived(
    const std::vector<OOBEAppDefinition>& app_infos,
    const std::vector<OOBEDeviceUseCase>& use_cases,
    AppsFetchingResult result) {
  if (result != AppsFetchingResult::kSuccess) {
    LOG(ERROR)
        << "Got an error when fetched cached data from the OOBE Apps Service";
    exit_callback_.Run(Result::kNotApplicable);
  }

  if (app_infos.empty()) {
    LOG(ERROR) << "Empty set of apps received from the server";
    exit_callback_.Run(Result::kNotApplicable);
  }

  if (use_cases.empty()) {
    LOG(ERROR) << "Empty set of use-cases received from the server";
    exit_callback_.Run(Result::kNotApplicable);
  }

  // Save locally into the screen class to retrieve data needed for the
  // installation after the user's selection.
  app_infos_ = app_infos;

  // This code performs the following steps to prepare recommended app data for
  // the WebUI:
  //
  // 1. Loads Selected Use Cases:
  //   - Retrieves the user's selected device use case IDs from their
  //      preferences.
  //    - Sorts them according to the server-defined order for consistent
  //      display and prioritization.
  //    - If no use cases are selected, inserts a special "zero use cases
  //      selected" use case as the first element.
  //
  // 2. Maps Apps to Use Cases:
  //    - Creates a map (`apps_to_use_cases`) where keys are app IDs and
  //      values are lists of corresponding use case IDs.
  //
  // 3. Creates WebUI-Ready App Data:
  //    - Generates a final map (`apps_dict`) associating use case names
  //      with their lists of recommended apps.
  //    - Filters out duplicate apps using a set (`used_apps_uuids`) to ensure
  //      each app appears only once in the results.
  //    - If an app is associated with multiple selected use cases, the
  //      server-defined order determines the primary use case for display.
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  const base::Value::List& selected_use_cases_ids =
      prefs->GetList(prefs::kOobeCategoriesSelected).Clone();

  std::vector<OOBEDeviceUseCase> selected_use_cases;

  // Gathers selected use-cases from saved IDs and sorts them according to the
  // server-side order.
  // This order determines both the display sequence to the user and the
  // preferred use-case for apps associated with multiple use-cases.
  for (const auto& selected_use_case_id : selected_use_cases_ids) {
    auto it = std::find_if(
        use_cases.cbegin(), use_cases.cend(),
        [&selected_use_case_id](const OOBEDeviceUseCase& use_case) {
          return use_case.GetID() == selected_use_case_id.GetString();
        });
    if (it != use_cases.end()) {
      selected_use_cases.push_back(*it);
    }
  }

  std::sort(selected_use_cases.begin(), selected_use_cases.end());

  // Add a special use-case ID that is used specifically for the case when no
  // device use-cases were selected on the previous screen.
  if (selected_use_cases.empty()) {
    if (use_cases.front().GetID() != kNoUseCasesSelectedIDName) {
      LOG(ERROR) << "Got: " << use_cases.front().GetID()
                 << " use-case as zero index one, expected: "
                 << kNoUseCasesSelectedIDName << ", server data is malformed"
                 << ", skipping recommend apps screen";
      exit_callback_.Run(Result::kNotApplicable);
      return;
    }
    selected_use_cases.emplace_back(use_cases.front());
  }

  std::unordered_map<std::string, std::vector<OOBEAppDefinition>>
      apps_to_use_cases;

  for (const auto& app : app_infos) {
    std::vector<std::string> tags = app.GetTags();
    // TODO: Remove this logic once server side data is finalized and updated.
    // There should be no apps with zero tags, but for now we will add all such
    // apps into a special "zero use-cases selected" group, which is displayed
    // when user skips device-use case screen.
    if (tags.empty()) {
      tags.emplace_back(use_cases.front().GetID());
    }

    for (const auto& tag : tags) {
      auto it = apps_to_use_cases.find(tag);
      if (it == apps_to_use_cases.end()) {
        apps_to_use_cases[tag] = std::vector<OOBEAppDefinition>{app};

      } else {
        it->second.emplace_back(app);
      }
    }
  }

  // We should show app only once in the whole list even if multiple use-cases
  // are attached to that app.
  // We use server-side ordering to decide in which use-case app should be shown
  // in case multiple attached use-cases were selected by the user.
  std::unordered_set<std::string> used_apps_uuids;

  // We pass a map of use_case_name` -> list of apps to the screen's WebUI.
  base::Value::Dict apps_dict;

  for (const auto& selected_use_case : selected_use_cases) {
    base::Value::List apps_list;
    DCHECK(apps_to_use_cases.find(selected_use_case.GetID()) !=
           apps_to_use_cases.end());
    for (const auto& app : apps_to_use_cases[selected_use_case.GetID()]) {
      if (used_apps_uuids.find(app.GetAppGroupUUID()) !=
          used_apps_uuids.end()) {
        continue;
      }
      used_apps_uuids.insert(app.GetAppGroupUUID());

      base::Value::Dict app_dict(
          base::Value::Dict()
              .Set("appId", base::Value(app.GetAppGroupUUID()))
              .Set("name", base::Value(app.GetName()))
              // TODO: Propagate app's subtitle when it will be available on the
              // server.
              .Set("package_name", base::Value(app.GetPackageId()->ToString()))
              .Set("icon", base::Value(app.GetIconURL()))
              .Set("selected", base::Value(false)));

      apps_list.Append(std::move(app_dict));
    }

    if (!apps_list.empty()) {
      // Map a name of use-case to the list of filetered apps.
      apps_dict.Set(selected_use_case.GetLabel(), std::move(apps_list));
    }
  }

  if (view_) {
    view_->SetCategoriesAppsMapData(std::move(apps_dict));
  }
}

void PersonalizedRecommendAppsScreen::HideImpl() {}

void PersonalizedRecommendAppsScreen::OnUserAction(
    const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionSkip) {
    exit_callback_.Run(Result::kSkip);
    return;
  }

  if (action_id == kUserActionNext) {
    CHECK_EQ(args.size(), 2u);
    // TODO(b/339789465) : the install logic of the apps.
    exit_callback_.Run(Result::kNext);
    return;
  }

  if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::kBack);
    return;
  }

  BaseScreen::OnUserAction(args);
}

}  // namespace ash
