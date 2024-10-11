// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/os_settings_provider.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/constants/web_app_id_constants.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/ash/app_list/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/ash/settings/search/hierarchy.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_handler.h"
#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager.h"
#include "chrome/browser/ui/webui/ash/settings/services/settings_manager/os_settings_manager_factory.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"

namespace app_list {
namespace {

using SettingsResultPtr = ::ash::settings::mojom::SearchResultPtr;
using SettingsResultType = ::ash::settings::mojom::SearchResultType;
using Setting = chromeos::settings::mojom::Setting;
using Subpage = chromeos::settings::mojom::Subpage;
using Section = chromeos::settings::mojom::Section;

constexpr char kOsSettingsResultPrefix[] = "os-settings://";

constexpr size_t kNumRequestedResults = 5u;

// Various states of the OsSettingsProvider. These values persist to logs.
// Entries should not be renumbered and numeric values should never be reused.
enum class Status {
  kOk = 0,
  // No longer used.
  // kAppServiceUnavailable = 1,
  kNoSettingsIcon = 2,
  kSearchHandlerUnavailable = 3,
  kHierarchyEmpty = 4,
  kNoHierarchy = 5,
  kSettingsAppNotReady = 6,
  kNoAppServiceProxy = 7,
  kMaxValue = kNoAppServiceProxy,
};

void LogStatus(Status status) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.OsSettingsProvider.Error", status);
}

// Various icon-related states at different branches of the OsSettingsProvider.
// These values persist to logs. Entries should not be renumbered and numeric
// values should never be reused.
//
// TODO(b/261867385) this histogram is to investigate the bug that settings
// search results may not appear in launcher search due to the lack of icon. It
// can be removed once the associated bug is resolved.
enum class IconLoadStatus {
  // Construction
  kNoAppServiceProxy = 0,
  kBindOnLoadIconFromConstructor = 1,
  // On App Update
  kBindOnLoadIconFromOnAppUpdate = 2,
  kReadinessUnknown = 3,
  kIconKeyNotChanged = 4,
  // On Load Icon (from Constructor)
  kOkFromConstructor = 5,
  kNoValueFromConstructor = 6,
  kNotStandardFromConstructor = 7,
  // On Load Icon (from OnAppUpdate)
  kOkFromOnAppUpdate = 8,
  kNoValueFromOnAppUpdate = 9,
  kNotStandardFromOnAppUpdate = 10,
  // On App Registry Cache Will Be Destroyed
  kIconExistOnDestroyed = 11,
  kIconNotExistOnDestroyed = 12,
  // On App Update
  kOnAppUpdateGetCalled = 13,
  kMaxValue = kOnAppUpdateGetCalled,
};

void LogIconLoadStatus(IconLoadStatus icon_load_status) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.OsSettingsProvider.IconLoadStatus",
                            icon_load_status);
}

bool ContainsBetterAncestor(Subpage subpage,
                            const double score,
                            const ash::settings::Hierarchy* hierarchy,
                            const base::flat_map<Subpage, double>& subpages,
                            const base::flat_map<Section, double>& sections) {
  // Returns whether or not a higher-scoring ancestor subpage or section of
  // |subpage| is present within |subpages| or |sections|.
  const auto& metadata = hierarchy->GetSubpageMetadata(subpage);

  // Check parent subpage if one exists.
  if (metadata.parent_subpage) {
    const auto it = subpages.find(metadata.parent_subpage);
    if ((it != subpages.end() && it->second >= score) ||
        ContainsBetterAncestor(metadata.parent_subpage.value(), score,
                               hierarchy, subpages, sections)) {
      return true;
    }
  }

  // Check section.
  const auto it = sections.find(metadata.section);
  return it != sections.end() && it->second >= score;
}

bool ContainsBetterAncestor(Setting setting,
                            const double score,
                            const ash::settings::Hierarchy* hierarchy,
                            const base::flat_map<Subpage, double>& subpages,
                            const base::flat_map<Section, double>& sections) {
  // Returns whether or not a higher-scoring ancestor subpage or section of
  // |setting| is present within |subpages| or |sections|.
  const auto& metadata = hierarchy->GetSettingMetadata(setting);

  // Check primary subpage only. Alternate subpages aren't used enough for the
  // check to be worthwhile.
  if (metadata.primary.subpage) {
    const auto parent_subpage = metadata.primary.subpage.value();
    const auto it = subpages.find(parent_subpage);
    if ((it != subpages.end() && it->second >= score) ||
        ContainsBetterAncestor(parent_subpage, score, hierarchy, subpages,
                               sections)) {
      return true;
    }
  }

  // Check section.
  const auto it = sections.find(metadata.primary.section);
  return it != sections.end() && it->second >= score;
}

}  // namespace

OsSettingsResult::OsSettingsResult(Profile* profile,
                                   const SettingsResultPtr& result,
                                   const double relevance_score,
                                   const ui::ImageModel& icon,
                                   const std::u16string& query)
    : profile_(profile), url_path_(result->url_path_with_parameters) {
  set_id(kOsSettingsResultPrefix + url_path_);
  SetCategory(Category::kSettings);
  set_relevance(relevance_score);
  SetTitle(result->canonical_text);
  SetResultType(ResultType::kOsSettings);
  SetDisplayType(DisplayType::kList);
  SetMetricsType(ash::OS_SETTINGS);
  SetIcon(IconInfo(icon, kAppIconDimension));

  // If the result is not a top-level section, set the display text with
  // information about the result's 'parent' category. This is the last element
  // of |result->settings_page_hierarchy|, which is localized and ready for
  // display. Some subpages have the same name as their section (namely,
  // bluetooth), in which case we should leave the details blank.
  const auto& hierarchy = result->settings_page_hierarchy;
  if (hierarchy.empty()) {
    LogStatus(Status::kHierarchyEmpty);
  } else if (result->type != SettingsResultType::kSection) {
    SetDetails(hierarchy.back());
  }

  // Manually build the accessible name for the search result, in a way that
  // parallels the regular accessible names set by
  // SearchResultBaseView::ComputeAccessibleName.
  std::u16string accessible_name = title();
  if (!details().empty()) {
    accessible_name += u", ";
    accessible_name += details();
  }
  accessible_name += u", ";
  // The first element in the settings hierarchy is always the top-level
  // localized name of the Settings app.
  accessible_name += hierarchy[0];
  SetAccessibleName(accessible_name);
}

OsSettingsResult::~OsSettingsResult() = default;

void OsSettingsResult::Open(int event_flags) {
  chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(profile_,
                                                               url_path_);
}

OsSettingsProvider::OsSettingsProvider(Profile* profile)
    : SearchProvider(SearchCategory::kSettings), profile_(profile) {
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

OsSettingsProvider::~OsSettingsProvider() = default;

void OsSettingsProvider::MaybeInitialize(
    ash::settings::SearchHandler* fake_search_handler,
    const ash::settings::Hierarchy* fake_hierarchy) {
  // Ensures that the provider can be initialized once only.
  if (has_initialized) {
    return;
  }
  has_initialized = true;

  // Initialization is happening, so we no longer need to wait for user session
  // start up task completion.
  session_manager_observation_.Reset();

  // Use fake search handler and hierarchy if provided in tests, or get it from
  // `os_settings_manager`.
  if (fake_search_handler) {
    search_handler_ = fake_search_handler;
    hierarchy_ = fake_hierarchy;
  } else {
    auto* os_settings_manager =
        ash::settings::OsSettingsManagerFactory::GetForProfile(profile_);
    auto* app_service_proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile_);
    if (!os_settings_manager || !app_service_proxy) {
      return;
    }
    search_handler_ = os_settings_manager->search_handler();
    hierarchy_ = os_settings_manager->hierarchy();
  }

  // `search_handler_` can be nullptr in the case that the new OS settings
  // search chrome flag is disabled. If it is, we should effectively disable the
  // search provider.
  if (!search_handler_) {
    LogStatus(Status::kSearchHandlerUnavailable);
    return;
  }

  if (!hierarchy_) {
    LogStatus(Status::kNoHierarchy);
  }

  search_handler_->Observe(
      search_results_observer_receiver_.BindNewPipeAndPassRemote());

  // TODO(b/261867385): We manually load the icon from the local codebase as
  // the icon load from proxy is flaky. When the flakiness if solved, we can
  // safely remove this.
  icon_ = ui::ImageModel::FromVectorIcon(
      app_list::kOsSettingsIcon, SK_ColorTRANSPARENT, kAppIconDimension);

  app_registry_cache_observer_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile_)
           ->AppRegistryCache());

  // TODO(b/261867385): `LoadIcon()` from constructor is removed as it never
  // succeeds and the icon is only updated from "OnAppUpdate()" according to
  // the UMA metrics. We can either remove this comments if this issue is
  // confirmed, or revert the remove if this issue is solved.
  LogIconLoadStatus(IconLoadStatus::kBindOnLoadIconFromConstructor);
}

void OsSettingsProvider::Start(const std::u16string& query) {
  // Disable the provider if:
  //  - the search backend isn't available
  //  - the settings app isn't ready
  //  - we don't have an icon to display with results.
  if (!search_handler_) {
    // If user has started to user launcher search before the user session
    // startup tasks completed, we should honor this user action and
    // initialize the provider. It makes the os setting search available
    // earlier.
    MaybeInitialize();
    return;
  } else if (icon_.IsEmpty()) {
    LogStatus(Status::kNoSettingsIcon);
    return;
  }

  // Do not return results for queries that are too short, as the results
  // generally aren't meaningful. Note this provider never provides zero-state
  // results.
  if (query.size() < min_query_length_) {
    return;
  }

  const base::TimeTicks start_time = base::TimeTicks::Now();
  last_query_ = query;

  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
  search_handler_->Search(
      query, kNumRequestedResults,
      ash::settings::mojom::ParentResultBehavior::kDoNotIncludeParentResults,
      base::BindOnce(&OsSettingsProvider::OnSearchReturned,
                     weak_factory_.GetWeakPtr(), query, start_time));
}

void OsSettingsProvider::StopQuery() {
  last_query_.clear();
  // Invalidate weak pointers to cancel existing searches.
  weak_factory_.InvalidateWeakPtrs();
}

ash::AppListSearchResultType OsSettingsProvider::ResultType() const {
  return ash::AppListSearchResultType::kOsSettings;
}

void OsSettingsProvider::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() != web_app::kOsSettingsAppId) {
    return;
  }

  LogIconLoadStatus(IconLoadStatus::kOnAppUpdateGetCalled);

  // TODO(crbug.com/40125676): We previously disabled this search provider until
  // the app service signalled that the settings app is ready. But this signal
  // is flaky, so sometimes search provider was permanently disabled. Once the
  // signal is reliable, we should re-add the check.

  // Request the Settings app icon when either the readiness or the icon has
  // changed.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);
  if (update.ReadinessChanged() || update.IconKeyChanged()) {
    proxy->LoadIcon(web_app::kOsSettingsAppId, apps::IconType::kStandard,
                    kAppIconDimension,
                    /*allow_placeholder_icon=*/false,
                    base::BindOnce(&OsSettingsProvider::OnLoadIcon,
                                   weak_factory_.GetWeakPtr(),
                                   /*is_from_constructor=*/false));
    LogIconLoadStatus(IconLoadStatus::kBindOnLoadIconFromOnAppUpdate);
  } else {
    if (!update.ReadinessChanged()) {
      LogIconLoadStatus(IconLoadStatus::kReadinessUnknown);
    }
    if (!update.IconKeyChanged()) {
      LogIconLoadStatus(IconLoadStatus::kIconKeyNotChanged);
    }
  }
}

void OsSettingsProvider::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
  if (icon_.IsEmpty()) {
    LogIconLoadStatus(IconLoadStatus::kIconNotExistOnDestroyed);
  } else {
    LogIconLoadStatus(IconLoadStatus::kIconExistOnDestroyed);
  }
}

void OsSettingsProvider::OnSearchResultsChanged() {
  if (last_query_.empty()) {
    return;
  }

  Start(last_query_);
}

void OsSettingsProvider::OnUserSessionStartUpTaskCompleted() {
  MaybeInitialize();
}

void OsSettingsProvider::OnSearchReturned(
    const std::u16string& query,
    const base::TimeTicks& start_time,
    std::vector<SettingsResultPtr> sorted_results) {
  DCHECK_LE(sorted_results.size(), kNumRequestedResults);

  SearchProvider::Results search_results;

  for (const auto& result : FilterResults(query, sorted_results, hierarchy_)) {
    search_results.emplace_back(std::make_unique<OsSettingsResult>(
        profile_, result, result->relevance_score, icon_, last_query_));
  }

  UMA_HISTOGRAM_TIMES("Apps.AppList.OsSettingsProvider.QueryTime",
                      base::TimeTicks::Now() - start_time);
  // Log the OS setting search has been successfully proceeded.
  LogStatus(Status::kOk);
  SwapResults(&search_results);
}

std::vector<SettingsResultPtr> OsSettingsProvider::FilterResults(
    const std::u16string& query,
    const std::vector<SettingsResultPtr>& results,
    const ash::settings::Hierarchy* hierarchy) {
  base::flat_set<std::string> seen_urls;
  base::flat_map<Subpage, double> seen_subpages;
  base::flat_map<Section, double> seen_sections;
  std::vector<SettingsResultPtr> clean_results;

  for (const SettingsResultPtr& result : results) {
    // Filter results below the score threshold.
    if (result->relevance_score < min_score_) {
      continue;
    }

    // Check if query matched alternate text for the result. If so, only allow
    // results meeting extra requirements. Perform this check before checking
    // for duplicates to ensure a rejected alternate result doesn't preclude a
    // canonical result with a lower score from being shown.
    if (result->text != result->canonical_text &&
        (!accept_alternate_matches_ ||
         query.size() < min_query_length_for_alternates_ ||
         result->relevance_score < min_score_for_alternates_)) {
      continue;
    }

    // Check if URL has been seen.
    const std::string url = result->url_path_with_parameters;
    const auto it = seen_urls.find(url);
    if (it != seen_urls.end()) {
      continue;
    }

    seen_urls.insert(url);
    clean_results.push_back(result.Clone());
    if (result->type == SettingsResultType::kSubpage) {
      seen_subpages.insert(
          std::make_pair(result->id->get_subpage(), result->relevance_score));
    }
    if (result->type == SettingsResultType::kSection) {
      seen_sections.insert(
          std::make_pair(result->id->get_section(), result->relevance_score));
    }
  }

  // Iterate through the clean results a second time. Remove subpage or setting
  // results that have a higher-scoring ancestor subpage or section also present
  // in the results.
  for (size_t i = 0; i < clean_results.size(); ++i) {
    const auto& result = clean_results[i];
    if ((result->type == SettingsResultType::kSubpage &&
         ContainsBetterAncestor(result->id->get_subpage(),
                                result->relevance_score, hierarchy_,
                                seen_subpages, seen_sections)) ||
        (result->type == SettingsResultType::kSetting &&
         ContainsBetterAncestor(result->id->get_setting(),
                                result->relevance_score, hierarchy_,
                                seen_subpages, seen_sections))) {
      clean_results.erase(clean_results.begin() + i);
      --i;
    }
  }

  return clean_results;
}

void OsSettingsProvider::OnLoadIcon(bool is_from_constructor,
                                    apps::IconValuePtr icon_value) {
  if (icon_value && icon_value->icon_type == apps::IconType::kStandard) {
    icon_ = ui::ImageModel::FromImageSkia(icon_value->uncompressed);
    LogIconLoadStatus(is_from_constructor ? IconLoadStatus::kOkFromConstructor
                                          : IconLoadStatus::kOkFromOnAppUpdate);
  } else if (!icon_value) {
    LogIconLoadStatus(is_from_constructor
                          ? IconLoadStatus::kNoValueFromConstructor
                          : IconLoadStatus::kNoValueFromOnAppUpdate);
  } else {
    LogIconLoadStatus(is_from_constructor
                          ? IconLoadStatus::kNotStandardFromConstructor
                          : IconLoadStatus::kNotStandardFromOnAppUpdate);
  }
}

}  // namespace app_list
