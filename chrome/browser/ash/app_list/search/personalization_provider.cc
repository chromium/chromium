// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/personalization_provider.h"

#include <string>
#include <vector>

#include "ash/constants/personalization_entry_point.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/personalization_app/search/search_handler.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "url/gurl.h"

namespace app_list {

namespace {

inline constexpr size_t kMinQueryLength = 3u;
inline constexpr size_t kNumRequestedResults = 3u;

}  // namespace

PersonalizationResult::PersonalizationResult(
    Profile* profile,
    const ash::personalization_app::mojom::SearchResult& result,
    const std::u16string& query,
    gfx::ImageSkia icon)
    : profile_(profile) {
  DCHECK(profile_);
  set_id(::ash::personalization_app::kChromeUIPersonalizationAppURL +
         result.relative_url);
  set_relevance(result.relevance_score);
  // Put personalization results into the Settings category. May change in the
  // future.
  SetCategory(Category::kSettings);
  SetTitle(result.text);
  SetResultType(ResultType::kPersonalization);
  SetDisplayType(DisplayType::kList);
  SetIcon(IconInfo(ui::ImageModel::FromImageSkia(icon), kAppIconDimension));
  SetMetricsType(::ash::SearchResultType::PERSONALIZATION);
}

PersonalizationResult::~PersonalizationResult() = default;

void PersonalizationResult::Open(int event_flags) {
  ::ash::SystemAppLaunchParams launch_params;
  launch_params.url = GURL(id());
  // Record entry point to Personalization Hub through Launcher search.
  ash::personalization_app::LogPersonalizationEntryPoint(
      ash::PersonalizationEntryPoint::kLauncherSearch);
  ash::LaunchSystemWebAppAsync(profile_, ash::SystemWebAppType::PERSONALIZATION,
                               launch_params);
}

PersonalizationProvider::PersonalizationProvider(Profile* profile)
    : SearchProvider(SearchCategory::kSettings), profile_(profile) {
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager->IsUserSessionStartUpTaskCompleted()) {
    // If user session start up task has completed, the initialization can
    // start.
    MaybeInitialize();
  } else {
    // Wait for the user session start up task completion to prioritize
    // resources for them.
    session_manager_observation_.Observe(session_manager);
  }
}

PersonalizationProvider::~PersonalizationProvider() = default;

void PersonalizationProvider::MaybeInitialize(
    ::ash::personalization_app::SearchHandler* fake_search_handler) {
  // Ensures that the provider can be initialized once only.
  if (has_initialized) {
    return;
  }
  has_initialized = true;

  // Initialization is happening, so we no longer need to wait for user session
  // start up task completion.
  session_manager_observation_.Reset();

  app_registry_cache_observer_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile_)
           ->AppRegistryCache());
  StartLoadIcon();

  // Use fake search handler if provided in tests, or get it from
  // `personalization_app_manager`.
  if (fake_search_handler) {
    search_handler_ = fake_search_handler;
  } else {
    auto* personalization_app_manager = ash::personalization_app::
        PersonalizationAppManagerFactory::GetForBrowserContext(profile_);
    CHECK(personalization_app_manager);
    search_handler_ = personalization_app_manager->search_handler();
  }
  CHECK(search_handler_);
  search_handler_->AddObserver(
      search_results_observer_.BindNewPipeAndPassRemote());
}

void PersonalizationProvider::Start(const std::u16string& query) {
  if (!search_handler_) {
    // If user has started to user launcher search before the user session
    // startup tasks completed, we should honor this user action and
    // initialize the provider. It makes the personalization search available
    // earlier.
    MaybeInitialize();
    return;
  }

  if (query.size() < kMinQueryLength) {
    return;
  }

  if (icon_.isNull()) {
    VLOG(1) << "No personalization icon for search results";
    return;
  }

  current_query_ = query;
  weak_ptr_factory_.InvalidateWeakPtrs();
  search_handler_->Search(
      query, kNumRequestedResults,
      base::BindOnce(&PersonalizationProvider::OnSearchDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     /*start_time=*/base::TimeTicks::Now()));
}

void PersonalizationProvider::StopQuery() {
  current_query_.clear();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

ash::AppListSearchResultType PersonalizationProvider::ResultType() const {
  return ash::AppListSearchResultType::kPersonalization;
}

void PersonalizationProvider::OnSearchResultsChanged() {
  if (current_query_.empty()) {
    return;
  }
  Start(current_query_);
}

void PersonalizationProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != web_app::kPersonalizationAppId) {
    return;
  }

  if (update.IconKeyChanged() ||
      (update.ReadinessChanged() &&
       update.Readiness() == apps::Readiness::kReady)) {
    StartLoadIcon();
  }
}

void PersonalizationProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void PersonalizationProvider::OnSearchDone(
    base::TimeTicks start_time,
    std::vector<::ash::personalization_app::mojom::SearchResultPtr> results) {
  SearchProvider::Results search_results;
  for (const auto& result : results) {
    DCHECK(!result.is_null());
    search_results.push_back(std::make_unique<PersonalizationResult>(
        profile_, *result, current_query_, icon_));
  }

  base::UmaHistogramTimes("Apps.AppList.PersonalizationProvider.QueryTime",
                          base::TimeTicks::Now() - start_time);

  SwapResults(&search_results);
}

void PersonalizationProvider::OnUserSessionStartUpTaskCompleted() {
  MaybeInitialize();
}

void PersonalizationProvider::StartLoadIcon() {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->LoadIcon(
      web_app::kPersonalizationAppId, apps::IconType::kStandard,
      ash::SharedAppListConfig::instance().search_list_icon_dimension(),
      /*allow_placeholder_icon=*/false,
      base::BindOnce(&PersonalizationProvider::OnLoadIcon,
                     app_service_weak_ptr_factory_.GetWeakPtr()));
}

void PersonalizationProvider::OnLoadIcon(::apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    icon_ = icon_value->uncompressed;
  }
}

}  // namespace app_list
