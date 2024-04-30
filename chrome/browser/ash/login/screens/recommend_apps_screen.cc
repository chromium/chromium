// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/recommend_apps_screen.h"

#include "ash/components/arc/arc_prefs.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service_factory.h"
#include "chrome/browser/apps/app_discovery_service/play_extras.h"
#include "chrome/browser/ash/app_list/arc/arc_fast_app_reinstall_starter.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/recommend_apps_screen_handler.h"
#include "components/user_manager/user_manager.h"

namespace ash {
namespace {

constexpr const char kUserActionSkip[] = "recommendAppsSkip";
constexpr const char kUserActionInstall[] = "recommendAppsInstall";

// Maximum number of recommended apps that we are going to show.
const int kMaxAppCount = 21;

enum class RecommendAppsScreenAction {
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. This should be kept in sync with
  // RecommendAppsScreenAction in enums.xml.
  kSkipped = 0,
  kRetried = 1,
  kSelectedNone = 2,
  kAppSelected = 3,

  kMaxValue = kAppSelected
};

void RecordUmaSelectedRecommendedPercentage(
    int selected_recommended_percentage) {
  base::UmaHistogramPercentage(
      "OOBE.RecommendApps.Screen.SelectedRecommendedPercentage",
      selected_recommended_percentage);
}

void RecordUmaUserSelectionAppCount(int app_count) {
  base::UmaHistogramExactLinear("OOBE.RecommendApps.Screen.SelectedAppCount",
                                app_count, kMaxAppCount);
}

void RecordUmaScreenAction(RecommendAppsScreenAction action) {
  base::UmaHistogramEnumeration("OOBE.RecommendApps.Screen.Action", action);
}

}  // namespace

// static
std::string RecommendAppsScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kSelected:
      return "Selected";
    case Result::kSkipped:
      return "Skipped";
    case Result::kLoadError:
      return "LoadError";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

RecommendAppsScreen::RecommendAppsScreen(
    base::WeakPtr<RecommendAppsScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(RecommendAppsScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

RecommendAppsScreen::~RecommendAppsScreen() = default;

void RecommendAppsScreen::OnSkip() {
  RecordUmaScreenAction(RecommendAppsScreenAction::kSkipped);
  exit_callback_.Run(Result::kSkipped);
}

void RecommendAppsScreen::OnInstall(base::Value::List apps) {
  CHECK_GT(recommended_app_count_, 0u);
  CHECK_GT(apps.size(), 0u);
  int selected_app_count = static_cast<int>(apps.size());
  int selected_recommended_percentage =
      100 * selected_app_count / recommended_app_count_;
  RecordUmaUserSelectionAppCount(selected_app_count);
  RecordUmaSelectedRecommendedPercentage(selected_recommended_percentage);

  RecordUmaScreenAction(RecommendAppsScreenAction::kAppSelected);
  pref_service_->SetList(arc::prefs::kArcFastAppReinstallPackages,
                         std::move(apps));

  arc::ArcFastAppReinstallStarter* fast_app_reinstall_starter =
      arc::ArcSessionManager::Get()->fast_app_resintall_starter();
  if (fast_app_reinstall_starter) {
    fast_app_reinstall_starter->OnAppsSelectionFinished();
  } else {
    LOG(ERROR)
        << "Cannot complete Fast App Reinstall flow. Starter is not available.";
  }
  exit_callback_.Run(Result::kSelected);
}

bool RecommendAppsScreen::MaybeSkip(WizardContext& context) {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  DCHECK(user_manager->IsUserLoggedIn());

  Profile* profile = ProfileManager::GetActiveUserProfile();
  if (!arc::IsArcPlayStoreEnabledForProfile(profile)) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }

  bool is_managed_account = profile->GetProfilePolicyConnector()->IsManaged();
  bool is_child_account = user_manager->IsLoggedInAsChildUser();
  if (is_managed_account || is_child_account || skip_for_testing_) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }
  return false;
}

void RecommendAppsScreen::ShowImpl() {
  if (view_)
    view_->Show();

  Profile* profile = ProfileManager::GetActiveUserProfile();
  pref_service_ = profile->GetPrefs();

  app_discovery_service_ =
      apps::AppDiscoveryServiceFactory::GetForProfile(profile);
  app_discovery_service_->GetApps(
      apps::ResultType::kRecommendedArcApps,
      base::BindOnce(&RecommendAppsScreen::OnRecommendationsDownloaded,
                     weak_factory_.GetWeakPtr()));
}

void RecommendAppsScreen::HideImpl() {}

void RecommendAppsScreen::OnRecommendationsDownloaded(
    const std::vector<apps::Result>& results,
    apps::DiscoveryError error) {
  switch (error) {
    case apps::DiscoveryError::kSuccess:
      UnpackResultAndShow(results);
      break;
    case apps::DiscoveryError::kErrorRequestFailed:
      OnLoadError();
      break;
    case apps::DiscoveryError::kErrorMalformedData:
      OnParseResponseError();
      break;
  }
}

void RecommendAppsScreen::UnpackResultAndShow(
    const std::vector<apps::Result>& results) {
  if (!view_)
    return;
  base::Value::List app_list;
  for (const auto& app_result : results) {
    base::Value::Dict app_info;
    app_info.Set("title", base::Value(app_result.GetAppTitle()));
    auto* play_extras = app_result.GetSourceExtras()->AsPlayExtras();
    app_info.Set("icon_url", base::Value(play_extras->GetIconUrl().spec()));
    app_info.Set("category", base::Value(play_extras->GetCategory()));
    app_info.Set("description", base::Value(play_extras->GetDescription()));
    app_info.Set("content_rating",
                 base::Value(play_extras->GetContentRating()));
    app_info.Set("content_rating_icon",
                 base::Value(play_extras->GetContentRatingIconUrl().spec()));
    app_info.Set("in_app_purchases",
                 base::Value(play_extras->GetHasInAppPurchases()));
    app_info.Set("was_installed",
                 base::Value(play_extras->GetWasPreviouslyInstalled()));
    app_info.Set("contains_ads", base::Value(play_extras->GetContainsAds()));
    app_info.Set("package_name", base::Value(play_extras->GetPackageName()));
    app_info.Set("optimized_for_chrome",
                 base::Value(play_extras->GetOptimizedForChrome()));
    app_list.Append(std::move(app_info));
    if (app_list.size() == kMaxAppCount - 1)
      break;
  }
  recommended_app_count_ = app_list.size();
  view_->OnLoadSuccess(base::Value(std::move(app_list)));
}

void RecommendAppsScreen::OnLoadError() {
  if (is_hidden())
    return;
  exit_callback_.Run(Result::kLoadError);
}

void RecommendAppsScreen::OnParseResponseError() {
  if (view_)
    view_->OnParseResponseError();
  exit_callback_.Run(Result::kSkipped);
}

void RecommendAppsScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionSkip) {
    OnSkip();
  } else if (action_id == kUserActionInstall) {
    CHECK_EQ(args.size(), 2u);
    OnInstall(args[1].GetList().Clone());
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
