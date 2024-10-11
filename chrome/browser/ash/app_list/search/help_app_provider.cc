// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/help_app_provider.h"

#include <memory>

#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/webui/help_app_ui/help_app_manager.h"
#include "ash/webui/help_app_ui/help_app_manager_factory.h"
#include "ash/webui/help_app_ui/search/search_handler.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/app_list/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/paint_vector_icon.h"
#include "url/gurl.h"

namespace app_list {
namespace {

constexpr size_t kMinQueryLength = 3u;
constexpr double kMinScore = 0.4;
constexpr size_t kNumRequestedResults = 5u;

// The end result of a list search. Logged once per time a list search finishes.
// Not logged if the search is canceled by a new search starting. Not logged for
// suggestion chips. These values persist to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class ListSearchResultState {
  // Search finished with no problems.
  kOk = 0,
  // Search canceled because no help app icon.
  kNoHelpAppIcon = 1,
  // Search canceled because the search backend isn't available.
  kSearchHandlerUnavailable = 2,
  kMaxValue = kSearchHandlerUnavailable,
};

// Use this when a list search finishes.
void LogListSearchResultState(ListSearchResultState state) {
  base::UmaHistogramEnumeration(
      "Apps.AppList.HelpAppProvider.ListSearchResultState", state);
}

}  // namespace

HelpAppResult::HelpAppResult(
    const float& relevance,
    Profile* profile,
    const ash::help_app::mojom::SearchResultPtr& result,
    const ui::ImageModel& icon,
    const std::u16string& query)
    : profile_(profile),
      url_path_(result->url_path_with_parameters),
      help_app_content_id_(result->id) {
  DCHECK(profile_);
  set_id(ash::kChromeUIHelpAppURL + url_path_);
  set_relevance(relevance);
  SetTitle(result->title);
  SetCategory(Category::kHelp);
  SetResultType(ResultType::kHelpApp);
  SetDisplayType(DisplayType::kList);
  SetMetricsType(ash::HELP_APP_DEFAULT);
  SetIcon(IconInfo(icon, kAppIconDimension));
  SetDetails(result->main_category);
}

HelpAppResult::~HelpAppResult() = default;

void HelpAppResult::Open(int event_flags) {
  // This is a google-internal histogram. If changing this, also change the
  // corresponding histograms file.
  base::UmaHistogramSparse("Discover.LauncherSearch.ContentLaunched",
                           base::PersistentHash(help_app_content_id_));

  // Note: event_flags is ignored, LaunchSWA doesn't need it.
  // Launch list result.
  ash::SystemAppLaunchParams params;
  params.url = GURL(ash::kChromeUIHelpAppURL + url_path_);
  params.launch_source = apps::LaunchSource::kFromAppListQuery;
  ash::LaunchSystemWebAppAsync(
      profile_, ash::SystemWebAppType::HELP, params,
      std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));
}

HelpAppProvider::HelpAppProvider(Profile* profile)
    : SearchProvider(SearchCategory::kHelp), profile_(profile) {
  CHECK(profile_);

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

HelpAppProvider::~HelpAppProvider() = default;

void HelpAppProvider::MaybeInitialize(
    ash::help_app::SearchHandler* fake_search_handler) {
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
  LoadIcon();

  // TODO(b/261867385): We manually load the icon from the local codebase as
  // the icon load from proxy is flaky. When the flakiness if solved, we can
  // safely remove this.
  icon_ = ui::ImageModel::FromVectorIcon(
      app_list::kHelpAppIcon, SK_ColorTRANSPARENT, kAppIconDimension);

  // Use fake search handler if provided in tests, or get it from
  // `help_app_manager`.
  if (fake_search_handler) {
    search_handler_ = fake_search_handler;
  } else {
    auto* help_app_manager =
        ash::help_app::HelpAppManagerFactory::GetForBrowserContext(profile_);
    CHECK(help_app_manager);
    search_handler_ = help_app_manager->search_handler();
  }

  if (!search_handler_) {
    return;
  }
  search_handler_->OnProfileDirAvailable(profile_->GetPath());
  search_handler_->Observe(
      search_results_observer_receiver_.BindNewPipeAndPassRemote());
}

void HelpAppProvider::Start(const std::u16string& query) {
  if (query.size() < kMinQueryLength) {
    // Do not do a list search for queries that are too short because the
    // results generally aren't meaningful. This isn't worth logging as a list
    // search result case because it happens frequently when entering a new
    // search query.
    return;
  }

  // Stop the search if:
  //  - the search backend isn't available (or the feature is disabled)
  //  - we don't have an icon to display with results.
  if (!search_handler_) {
    LogListSearchResultState(ListSearchResultState::kSearchHandlerUnavailable);
    // If user has started to user launcher search before the user session
    // startup tasks completed, we should honor this user action and
    // initialize the provider. It makes the help app search available
    // earlier.
    MaybeInitialize();
    return;
  } else if (icon_.IsEmpty()) {
    LogListSearchResultState(ListSearchResultState::kNoHelpAppIcon);
    // This prevents a timeout in the test, but it does not change the user
    // experience because the results were already cleared at the start.
    SearchProvider::Results search_results;
    SwapResults(&search_results);
    return;
  }

  // Start a search for list results.
  const base::TimeTicks start_time = base::TimeTicks::Now();
  last_query_ = query;

  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
  search_handler_->Search(
      query, kNumRequestedResults,
      base::BindOnce(&HelpAppProvider::OnSearchReturned,
                     weak_factory_.GetWeakPtr(), query, start_time));
}

void HelpAppProvider::StopQuery() {
  last_query_.clear();
  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
}

void HelpAppProvider::OnSearchReturned(
    const std::u16string& query,
    const base::TimeTicks& start_time,
    std::vector<ash::help_app::mojom::SearchResultPtr> sorted_results) {
  DCHECK_LE(sorted_results.size(), kNumRequestedResults);

  SearchProvider::Results search_results;
  for (const auto& result : sorted_results) {
    if (result->relevance_score < kMinScore) {
      continue;
    }

    search_results.emplace_back(std::make_unique<HelpAppResult>(
        result->relevance_score, profile_, result, icon_, last_query_));
  }

  base::UmaHistogramTimes("Apps.AppList.HelpAppProvider.QueryTime",
                          base::TimeTicks::Now() - start_time);
  LogListSearchResultState(ListSearchResultState::kOk);
  SwapResults(&search_results);
}

ash::AppListSearchResultType HelpAppProvider::ResultType() const {
  return ash::AppListSearchResultType::kHelpApp;
}

void HelpAppProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == web_app::kHelpAppId && update.ReadinessChanged() &&
      update.Readiness() == apps::Readiness::kReady) {
    LoadIcon();
  }
}

void HelpAppProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

// If the availability of search results changed, start a new search.
void HelpAppProvider::OnSearchResultAvailabilityChanged() {
  if (last_query_.empty()) {
    return;
  }
  Start(last_query_);
}

void HelpAppProvider::OnUserSessionStartUpTaskCompleted() {
  MaybeInitialize();
}

void HelpAppProvider::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    icon_ = ui::ImageModel::FromImageSkia(icon_value->uncompressed);
  }
}

void HelpAppProvider::LoadIcon() {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  proxy->LoadIcon(
      web_app::kHelpAppId, apps::IconType::kStandard,
      ash::SharedAppListConfig::instance().suggestion_chip_icon_dimension(),
      /*allow_placeholder_icon=*/false,
      base::BindOnce(&HelpAppProvider::OnLoadIcon, weak_factory_.GetWeakPtr()));
}

}  // namespace app_list
