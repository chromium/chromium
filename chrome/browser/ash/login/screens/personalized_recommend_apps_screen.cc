// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/personalized_recommend_apps_screen.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "ash/components/arc/arc_prefs.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_service/app_install/app_install_service.h"
#include "chrome/browser/apps/app_service/app_install/app_install_types.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_fast_app_reinstall_starter.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
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
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/package_id.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionSkip[] = "skip";
constexpr const char kUserActionBack[] = "back";
constexpr const char kUserActionLoaded[] = "loaded";
constexpr const char kNoUseCasesSelectedIDName[] = "oobe_none";

// Current max amount of apps we should get from the server.
constexpr const int kMaxAppCount = 100;

void WebAppInstallCallback(const std::string& package_id, bool success) {
  if (success) {
    LOG(WARNING) << "Web application '" << package_id
                 << "' installed successfully";
  } else {
    LOG(WARNING) << "Web application '" << package_id
                 << "' installation failed";
  }
}

void RecordUmaLoadingTime(base::TimeDelta delta) {
  base::UmaHistogramCustomTimes("OOBE.PersonalizedAppsScreen.LoadingTime",
                                delta, base::Milliseconds(1), base::Seconds(60),
                                50);
}

void RecordUmaSelectedAppsTotalCount(int selected_apps_total_count) {
  base::UmaHistogramCounts100(
      "OOBE.PersonalizedAppsScreen.SelectedAppsTotalCount",
      selected_apps_total_count);
}

void RecordUmaSelectedAppsTotalPercentage(int selected_apps_total_percentage) {
  base::UmaHistogramPercentage(
      "OOBE.PersonalizedAppsScreen.SelectedAppsTotalPercentage",
      selected_apps_total_percentage);
}

void RecordUmaSelectedAppsCount(int selected_apps_count,
                                apps::PackageType type) {
  if (type == apps::PackageType::kArc) {
    base::UmaHistogramCounts100(
        "OOBE.PersonalizedAppsScreen.SelectedAppsCount.ARC",
        selected_apps_count);
  } else if (type == apps::PackageType::kWeb) {
    base::UmaHistogramCounts100(
        "OOBE.PersonalizedAppsScreen.SelectedAppsCount.Web",
        selected_apps_count);
  }
}

void RecordUmaSelectedAppsPercentage(int selected_apps_percentage,
                                     apps::PackageType type) {
  if (type == apps::PackageType::kArc) {
    base::UmaHistogramPercentage(
        "OOBE.PersonalizedAppsScreen.SelectedAppsPercentage.ARC",
        selected_apps_percentage);
  } else if (type == apps::PackageType::kWeb) {
    base::UmaHistogramPercentage(
        "OOBE.PersonalizedAppsScreen.SelectedAppsPercentage.Web",
        selected_apps_percentage);
  }
}

void RecordUmaAppID(int app_order_id) {
  // Order is a zero-based index, so we can use ExactLinear histogram for now.
  base::UmaHistogramExactLinear("OOBE.PersonalizedAppsScreen.SelectedAppIDs",
                                app_order_id, kMaxAppCount);
}

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
    case Result::kDataMalformed:
      return "DataMalformed";
    case Result::kError:
      return "Error";
    case Result::kTimeout:
      return "Timeout";
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

  if (!features::IsOobePersonalizedOnboardingEnabled()) {
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
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  return false;
}

void PersonalizedRecommendAppsScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  view_->Show();

  timeout_overview_timer_ = std::make_unique<base::OneShotTimer>();
  timeout_overview_timer_->Start(
      FROM_HERE, delay_exit_timeout_, this,
      &PersonalizedRecommendAppsScreen::ExitScreenTimeout);

  loading_start_time_ = base::TimeTicks::Now();

  raw_ptr<OobeAppsDiscoveryService> oobe_apps_discovery_service_ =
      OobeAppsDiscoveryServiceFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
  oobe_apps_discovery_service_->GetAppsAndUseCases(
      base::BindOnce(&PersonalizedRecommendAppsScreen::OnResponseReceived,
                     weak_factory_.GetWeakPtr()));
}

void PersonalizedRecommendAppsScreen::OnResponseReceived(
    const std::vector<OOBEAppDefinition>& app_infos,
    const std::vector<OOBEDeviceUseCase>& use_cases,
    AppsFetchingResult result) {
  if (result != AppsFetchingResult::kSuccess) {
    LOG(ERROR)
        << "Got an error when fetched cached data from the OOBE Apps Service";
    exit_callback_.Run(Result::kError);
    return;
  }

  if (app_infos.empty()) {
    LOG(ERROR) << "Empty set of apps received from the server";
    exit_callback_.Run(Result::kDataMalformed);
    return;
  }

  if (use_cases.empty()) {
    LOG(ERROR) << "Empty set of use-cases received from the server";
    exit_callback_.Run(Result::kDataMalformed);
    return;
  }

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
  //    - Creates a map (`use_cases_to_apps`) where keys are app IDs and
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
      exit_callback_.Run(Result::kDataMalformed);
      return;
    }
    selected_use_cases.emplace_back(use_cases.front());
  }

  std::unordered_map<std::string, std::vector<OOBEAppDefinition>>
      use_cases_to_apps;

  app_package_id_to_order_.clear();

  for (const auto& app : app_infos) {
    if (app.GetPackageId() != std::nullopt) {
      app_package_id_to_order_[app.GetPackageId()->ToString()] = app.GetOrder();
    }

    std::vector<std::string> tags = app.GetTags();
    for (const auto& tag : tags) {
      auto it = use_cases_to_apps.find(tag);
      if (it == use_cases_to_apps.end()) {
        use_cases_to_apps[tag] = std::vector<OOBEAppDefinition>{app};

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

  base::Value::List apps_by_use_cases_list;

  // Reset it on each filtering so it's not persisted between screen
  // back/next flow.
  filtered_apps_count_by_type_.clear();

  for (const auto& selected_use_case : selected_use_cases) {
    base::Value::List apps_list;
    // Handle case when server-side provided use-case that doesn't have any apps
    // attached to it.
    if (use_cases_to_apps.find(selected_use_case.GetID()) ==
        use_cases_to_apps.end()) {
      LOG(ERROR) << "No applications related to the "
                 << selected_use_case.GetID()
                 << " use-case found, check server-side data";
      continue;
    }
    for (const auto& app : use_cases_to_apps[selected_use_case.GetID()]) {
      if (used_apps_uuids.find(app.GetAppGroupUUID()) !=
          used_apps_uuids.end()) {
        continue;
      }
      used_apps_uuids.insert(app.GetAppGroupUUID());

      filtered_apps_count_by_type_[app.GetPlatform()] += 1;

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
      // Create data that we pass to the WebUI.
      apps_by_use_cases_list.Append(
          base::Value::Dict()
              .Set("name", selected_use_case.GetLabel())
              .Set("apps", std::move(apps_list)));
    }
  }

  if (apps_by_use_cases_list.empty()) {
    LOG(ERROR) << "No apps found after filtering, skipping the screen";
    exit_callback_.Run(Result::kDataMalformed);
    return;
  }

  delay_set_apps_timer_ = std::make_unique<base::OneShotTimer>();

  delay_set_apps_timer_->Start(
      FROM_HERE, delay_set_apps_step_,
      base::BindOnce(&PersonalizedRecommendAppsScreen::SetAppsAndUseCasesData,
                     weak_factory_.GetWeakPtr(),
                     std::move(apps_by_use_cases_list)));
}

void PersonalizedRecommendAppsScreen::HideImpl() {}

void PersonalizedRecommendAppsScreen::OnInstall(
    base::Value::List selected_apps_package_ids) {
  base::Value::List selected_arc_apps;
  std::vector<apps::PackageId> selected_web_apps;

  int filtered_arc_apps_count =
      filtered_apps_count_by_type_[apps::PackageType::kArc];
  int filtered_web_apps_count =
      filtered_apps_count_by_type_[apps::PackageType::kWeb];
  int filtered_apps_total_count =
      filtered_arc_apps_count + filtered_web_apps_count;

  int selected_apps_total_count =
      static_cast<int>(selected_apps_package_ids.size());
  RecordUmaSelectedAppsTotalCount(selected_apps_total_count);

  RecordUmaSelectedAppsTotalPercentage(
      (filtered_apps_total_count > 0)
          ? (100 * selected_apps_total_count / filtered_apps_total_count)
          : 0);

  // We need to separate ARC and Web apps because they are installed
  // differently.
  // TODO(b/341309803): Unify installation logic by using AppInstallService for
  // all cases when available.
  for (const auto& selected_app_package_id : selected_apps_package_ids) {
    auto it =
        app_package_id_to_order_.find(selected_app_package_id.GetString());
    if (it != app_package_id_to_order_.end()) {
      RecordUmaAppID(it->second);
    }

    std::optional<apps::PackageId> package_id =
        apps::PackageId::FromString(selected_app_package_id.GetString());
    if (!package_id.has_value()) {
      LOG(ERROR) << "Can't create PackageId from " << selected_app_package_id;
      continue;
    }
    if (package_id->package_type() == apps::PackageType::kArc) {
      selected_arc_apps.Append(package_id->identifier());
    } else {
      selected_web_apps.emplace_back(std::move(*package_id));
    }
  }

  int selected_arc_apps_count = selected_arc_apps.size();
  RecordUmaSelectedAppsCount(selected_arc_apps_count, apps::PackageType::kArc);

  RecordUmaSelectedAppsPercentage(
      (filtered_arc_apps_count > 0)
          ? (100 * selected_arc_apps_count / filtered_arc_apps_count)
          : 0,
      apps::PackageType::kArc);

  int selected_web_apps_count = selected_web_apps.size();
  RecordUmaSelectedAppsCount(selected_web_apps_count, apps::PackageType::kWeb);

  RecordUmaSelectedAppsPercentage(
      (filtered_web_apps_count > 0)
          ? (100 * selected_web_apps_count / filtered_web_apps_count)
          : 0,
      apps::PackageType::kWeb);

  Profile* profile = ProfileManager::GetActiveUserProfile();

  // ARC apps installation logic based on the `ArcFastAppReinstallStarter`.
  if (selected_arc_apps.size() > 0) {
    PrefService* prefs = profile->GetPrefs();
    prefs->SetList(arc::prefs::kArcFastAppReinstallPackages,
                   std::move(selected_arc_apps));

    arc::ArcFastAppReinstallStarter* fast_app_reinstall_starter =
        arc::ArcSessionManager::Get()->fast_app_resintall_starter();
    if (fast_app_reinstall_starter) {
      fast_app_reinstall_starter->OnAppsSelectionFinished();
    } else {
      LOG(ERROR) << "Cannot complete Fast App Reinstall flow. Starter is not "
                    "available.";
    }
  }

  // Web apps installation logic based on the `AppInstallService`.
  if (selected_web_apps.size() > 0) {
    apps::AppInstallService& install_service =
        apps::AppServiceProxyFactory::GetForProfile(profile)
            ->AppInstallService();

    for (const auto& selected_web_app : selected_web_apps) {
      install_service.InstallAppHeadless(
          apps::AppInstallSurface::kOobeAppRecommendations, selected_web_app,
          base::BindOnce(&WebAppInstallCallback, selected_web_app.ToString()));
    }
  }
}

void PersonalizedRecommendAppsScreen::ExitScreenTimeout() {
  exit_callback_.Run(Result::kTimeout);
}

void PersonalizedRecommendAppsScreen::ShowOverviewStep() {
  RecordUmaLoadingTime(base::TimeTicks::Now() - loading_start_time_);
  if (view_) {
    view_->SetOverviewStep();
  }
}

void PersonalizedRecommendAppsScreen::SetAppsAndUseCasesData(
    base::Value::List apps_by_use_cases_list) {
  if (view_) {
    view_->SetAppsAndUseCasesData(std::move(apps_by_use_cases_list));
  }
}

void PersonalizedRecommendAppsScreen::OnUserAction(
    const base::Value::List& args) {
  if (is_hidden()) {
    return;
  }
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionLoaded) {
    timeout_overview_timer_.reset();
    delay_overview_timer_ = std::make_unique<base::OneShotTimer>();
    delay_overview_timer_->Start(
        FROM_HERE, delay_overview_step_, this,
        &PersonalizedRecommendAppsScreen::ShowOverviewStep);
    return;
  }

  if (action_id == kUserActionSkip) {
    exit_callback_.Run(Result::kSkip);
    return;
  }

  if (action_id == kUserActionNext) {
    CHECK_EQ(args.size(), 2u);
    OnInstall(args[1].GetList().Clone());
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
