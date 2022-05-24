// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/recommend_apps_screen.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service_factory.h"
#include "chrome/browser/apps/app_discovery_service/play_extras.h"
#include "chrome/browser/ash/login/screens/recommend_apps/recommend_apps_fetcher.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/recommend_apps_screen_handler.h"
#include "chrome/common/chrome_features.h"
#include "components/user_manager/user_manager.h"

namespace ash {

// static
std::string RecommendAppsScreen::GetResultString(Result result) {
  switch (result) {
    case Result::SELECTED:
      return "Selected";
    case Result::SKIPPED:
      return "Skipped";
    case Result::LOAD_ERROR:
      return "LoadError";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

RecommendAppsScreen::RecommendAppsScreen(
    RecommendAppsScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(RecommendAppsScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);

  view_->Bind(this);
}

RecommendAppsScreen::~RecommendAppsScreen() {
  if (view_)
    view_->Bind(nullptr);
}

// TODO(https://crbug.com/1070917) Migrate to OnUserActionDeprecated.
void RecommendAppsScreen::OnSkip() {
  if (is_hidden())
    return;
  exit_callback_.Run(Result::SKIPPED);
}

void RecommendAppsScreen::OnInstall() {
  if (is_hidden())
    return;
  exit_callback_.Run(Result::SELECTED);
}

void RecommendAppsScreen::OnViewDestroyed(RecommendAppsScreenView* view) {
  DCHECK_EQ(view, view_);
  view_ = nullptr;
}

bool RecommendAppsScreen::MaybeSkip(WizardContext* context) {
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  DCHECK(user_manager->IsUserLoggedIn());
  bool is_managed_account = ProfileManager::GetActiveUserProfile()
                                ->GetProfilePolicyConnector()
                                ->IsManaged();
  bool is_child_account = user_manager->IsLoggedInAsChildUser();
  if (is_managed_account || is_child_account || skip_for_testing_) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }
  return false;
}

void RecommendAppsScreen::ShowImpl() {
  if (view_)
    view_->Show();

  if (features::IsOobeNewRecommendAppsEnabled() &&
      base::FeatureList::IsEnabled(::features::kAppDiscoveryForOobe)) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    app_discovery_service_ =
        apps::AppDiscoveryServiceFactory::GetForProfile(profile);
    app_discovery_service_->GetApps(
        apps::ResultType::kRecommendedArcApps,
        base::BindOnce(&RecommendAppsScreen::OnRecommendationsDownloaded,
                       weak_factory_.GetWeakPtr()));
  } else {
    recommend_apps_fetcher_ = RecommendAppsFetcher::Create(this);
    recommend_apps_fetcher_->Start();
  }
}

void RecommendAppsScreen::HideImpl() {
  view_->Hide();
}

void RecommendAppsScreen::OnLoadSuccess(base::Value app_list) {
  if (view_)
    view_->OnLoadSuccess(std::move(app_list));
}

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
  }
  view_->OnLoadSuccess(base::Value(std::move(app_list)));
}

void RecommendAppsScreen::OnLoadError() {
  if (is_hidden())
    return;
  exit_callback_.Run(Result::LOAD_ERROR);
}

void RecommendAppsScreen::OnParseResponseError() {
  if (view_)
    view_->OnParseResponseError();
}

}  // namespace ash
