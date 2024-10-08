// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/app_list/search/app_search_data_source.h"

#include <algorithm>
#include <set>
#include <utility>

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/search/app_result.h"
#include "chrome/browser/ash/app_list/search/app_service_app_result.h"
#include "chrome/browser/ash/extensions/gfx_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"
#include "chromeos/ash/components/string_matching/tokenized_string_match.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"

using ::ash::string_matching::FuzzyTokenizedStringMatch;
using ::ash::string_matching::TokenizedString;
using ::ash::string_matching::TokenizedStringMatch;

namespace app_list {

namespace {

constexpr double kEps = 1e-5;

// The minimum capacity we reserve in the Apps container which will be filled
// with extensions and ARC apps, to avoid successive reallocation.
constexpr size_t kMinimumReservedAppsContainerCapacity = 60U;

// Parameters for FuzzyTokenizedStringMatch.
constexpr bool kUseWeightedRatio = false;
constexpr double kRelevanceThreshold = 0.64;

// Default recommended apps in descending order of priority.
constexpr const char* const ranked_default_app_ids[] = {
    web_app::kOsSettingsAppId, web_app::kHelpAppId, arc::kPlayStoreAppId,
    web_app::kCanvasAppId, web_app::kCameraAppId};

// Flag to enable/disable diacritics stripping
constexpr bool kStripDiacritics = true;

// A selection of apps are designated as default recommended apps, and these are
// ranked in a priority order. Determine the rank of the app corresponding to
// |app_id|.
//
// Returns:
//    The priority rank 0, 1, ... if the app is a default app.
//    -1 if the app is not a default app.
int GetDefaultAppRank(const std::string app_id) {
  for (size_t i = 0; i < std::size(ranked_default_app_ids); ++i) {
    if (app_id == ranked_default_app_ids[i]) {
      return i;
    }
  }
  return -1;
}

// Adds |app_result| to |results| only in case no duplicate apps were already
// added. Duplicate means the same app but for different domain, Chrome and
// Android.
void MaybeAddResult(SearchProvider::Results* results,
                    std::unique_ptr<AppResult> app_result,
                    std::set<std::string>* seen_or_filtered_apps) {
  if (seen_or_filtered_apps->count(app_result->app_id())) {
    return;
  }

  seen_or_filtered_apps->insert(app_result->app_id());

  std::unordered_set<std::string> duplicate_app_ids;
  if (!extensions::util::GetEquivalentInstalledArcApps(
          app_result->profile(), app_result->app_id(), &duplicate_app_ids)) {
    results->emplace_back(std::move(app_result));
    return;
  }

  for (const auto& duplicate_app_id : duplicate_app_ids) {
    if (seen_or_filtered_apps->count(duplicate_app_id)) {
      return;
    }
  }

  results->emplace_back(std::move(app_result));

  // Add duplicate ids in order to filter them if they appear down the
  // list.
  seen_or_filtered_apps->insert(duplicate_app_ids.begin(),
                                duplicate_app_ids.end());
}

// Linearly maps |score| to the range [min, max].
// |score| is assumed to be within [0.0, 1.0]; if it's greater than 1.0
// then max is returned; if it's less than 0.0, then min is returned.
double ReRange(const double score, const double min, const double max) {
  if (score >= 1.0) {
    return max;
  }
  if (score <= 0.0) {
    return min;
  }

  return min + score * (max - min);
}

// Gets the last activity time for an app. Returns the last launch time if it's
// set, and install time  for non-internal apps otherwise.
base::Time GetAppLastActivityTime(const apps::AppUpdate& update) {
  base::Time last_launch_time = update.LastLaunchTime();
  if (!last_launch_time.is_null()) {
    return last_launch_time;
  }

  if (!update.InstalledInternally()) {
    return update.InstallTime();
  }

  return base::Time();
}

}  // namespace

class AppSearchDataSource::AppInfo {
 public:
  AppInfo(const std::string& id,
          const std::string& name,
          const base::Time& last_activity_time)
      : id_(id),
        name_(base::UTF8ToUTF16(name)),
        last_activity_time_(last_activity_time) {}

  AppInfo(const AppInfo&) = delete;
  AppInfo& operator=(const AppInfo&) = delete;

  ~AppInfo() = default;

  struct CompareByLastActivityTimeAndThenAppId {
    bool operator()(const std::unique_ptr<AppInfo>& app1,
                    const std::unique_ptr<AppInfo>& app2) {
      // Sort decreasing by last activity time, then increasing by App ID.
      base::Time t1 = app1->last_activity_time();
      base::Time t2 = app2->last_activity_time();
      if (t1 != t2) {
        return t1 > t2;
      }
      return app1->id_ < app2->id_;
    }
  };

  TokenizedString* GetTokenizedIndexedName() {
    // Tokenizing a string is expensive. Don't pay the price for it at
    // construction of every App, but rather, only when needed (i.e. when the
    // query is not empty and cache the result.
    if (!tokenized_indexed_name_) {
      tokenized_indexed_name_ = std::make_unique<TokenizedString>(name_);
    }
    return tokenized_indexed_name_.get();
  }

  bool MatchSearchableTextExactly(const TokenizedString& query) {
    if (searchable_text_.empty()) {
      return false;
    }

    EnsureTokenizedIndexedSearchableText();

    TokenizedStringMatch match;
    for (const auto& curr_text : tokenized_indexed_searchable_text_) {
      if (match.Calculate(query, *curr_text) > 0) {
        return true;
      }
    }

    return false;
  }

  bool FuzzyMatchSearchableText(const TokenizedString& query) {
    if (searchable_text_.empty()) {
      return false;
    }

    EnsureTokenizedIndexedSearchableText();

    FuzzyTokenizedStringMatch match;
    for (const auto& curr_text : tokenized_indexed_searchable_text_) {
      if (match.Relevance(query, *curr_text, kUseWeightedRatio,
                          kStripDiacritics) >= kRelevanceThreshold) {
        return true;
      }
    }
    return false;
  }

  const std::string& id() const { return id_; }
  const std::u16string& name() const { return name_; }

  bool recommendable() const { return recommendable_; }
  void set_recommendable(bool recommendable) { recommendable_ = recommendable; }

  bool searchable() const { return searchable_; }
  void set_searchable(bool searchable) { searchable_ = searchable; }

  base::Time last_activity_time() const { return last_activity_time_; }

  void AddSearchableText(const std::u16string& searchable_text) {
    DCHECK(tokenized_indexed_searchable_text_.empty());
    searchable_text_.push_back(searchable_text);
  }

 private:
  void EnsureTokenizedIndexedSearchableText() {
    if (!tokenized_indexed_searchable_text_.empty()) {
      return;
    }

    for (const std::u16string& curr_text : searchable_text_) {
      tokenized_indexed_searchable_text_.push_back(
          std::make_unique<TokenizedString>(curr_text));
    }
  }

  const std::string id_;
  const std::u16string name_;
  const base::Time last_activity_time_;
  bool recommendable_ = true;
  bool searchable_ = true;
  std::vector<std::u16string> searchable_text_;
  std::unique_ptr<TokenizedString> tokenized_indexed_name_;
  std::vector<std::unique_ptr<TokenizedString>>
      tokenized_indexed_searchable_text_;
};

AppSearchDataSource::AppSearchDataSource(
    Profile* profile,
    AppListControllerDelegate* list_controller,
    base::Clock* clock)
    : profile_(profile),
      list_controller_(list_controller),
      clock_(clock),
      icon_cache_(apps::AppServiceProxyFactory::GetForProfile(profile)
                      ->app_icon_loader(),
                  apps::IconCache::GarbageCollectionPolicy::kExplicit) {
  app_registry_cache_observer_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile)
           ->AppRegistryCache());
}

AppSearchDataSource::~AppSearchDataSource() = default;

base::CallbackListSubscription AppSearchDataSource::SubscribeToAppUpdates(
    const base::RepeatingClosure& callback) {
  return app_updates_callback_list_.Add(callback);
}

void AppSearchDataSource::RefreshIfNeeded() {
  if (!apps_.empty() && !refresh_apps_factory_.HasWeakPtrs()) {
    return;
  }

  Refresh();
}

SearchProvider::Results AppSearchDataSource::GetRecommendations() {
  SearchProvider::Results recommendations;
  std::set<std::string> handled_results;

  const size_t apps_size = apps_.size();
  recommendations.reserve(apps_size);

  for (auto& app : apps_) {
    // Skip apps which cannot be shown as a suggested app.
    if (!app->recommendable()) {
      continue;
    }

    std::u16string title = app->name();
    std::unique_ptr<AppResult> result = CreateResult(app->id(), true);
    result->SetTitle(title);

    const int default_rank = GetDefaultAppRank(app->id());
    const base::Time time = app->last_activity_time();

    // Set app->relevance based on the following criteria. Scores are set
    // within the range [0, 0.66], allowing the SearchResultRanker some
    // headroom to set higher rankings without having to re-range these
    // scores.
    if (!time.is_null()) {
      // Case 1: if it has last activity time or install time, set the
      // relevance in [0.34, 0.66] based on the time.
      result->UpdateFromLastLaunchedOrInstalledTime(clock_->Now(), time);
      result->set_relevance(ReRange(result->relevance(), 0.34, 0.66));
    } else if (default_rank != -1) {
      // Case 2: if it's a default recommended app, set the relevance in
      // (0.33, 0.34) based on a hard-coded ordering.
      const double relevance = 0.34 - (kEps * (default_rank + 1));
      DCHECK(0.33 < relevance && relevance < 0.34);
      result->set_relevance(relevance);
    } else {
      // Case 3: otherwise set the relevance as 0.0;
      result->set_relevance(0.0);
    }

    result->SetDisplayType(ChromeSearchResult::DisplayType::kRecentApps);

    MaybeAddResult(&recommendations, std::move(result), &handled_results);
  }

  return recommendations;
}

SearchProvider::Results AppSearchDataSource::GetExactMatches(
    const std::u16string& query) {
  SearchProvider::Results matches;
  const size_t apps_size = apps_.size();
  matches.reserve(apps_size);
  std::set<std::string> handled_results;

  const TokenizedString query_terms(query);

  for (auto& app : apps_) {
    if (!app->searchable()) {
      continue;
    }

    TokenizedString* indexed_name = app->GetTokenizedIndexedName();
    TokenizedStringMatch match;
    double relevance = match.Calculate(query_terms, *indexed_name);

    // N.B. Exact matches should be shown even if the threshold isn't reached,
    // e.g. due to a localized name being particularly short.
    if (relevance <= 0 && !app->MatchSearchableTextExactly(query_terms)) {
      continue;
    }

    std::unique_ptr<AppResult> result = CreateResult(app->id(), false);

    // Update result from match.
    result->SetTitle(indexed_name->text());
    result->set_relevance(relevance);

    MaybeAddResult(&matches, std::move(result), &handled_results);
  }

  return matches;
}

SearchProvider::Results AppSearchDataSource::GetFuzzyMatches(
    const std::u16string& query) {
  SearchProvider::Results matches;
  const size_t apps_size = apps_.size();
  matches.reserve(apps_size);
  std::set<std::string> handled_results;

  const TokenizedString query_terms(query);

  for (auto& app : apps_) {
    if (!app->searchable()) {
      continue;
    }

    TokenizedString* indexed_name = app->GetTokenizedIndexedName();
    FuzzyTokenizedStringMatch match;
    const double relevance = match.Relevance(
        query_terms, *indexed_name, kUseWeightedRatio, kStripDiacritics);
    if (relevance < kRelevanceThreshold &&
        !app->FuzzyMatchSearchableText(query_terms)) {
      continue;
    }
    std::unique_ptr<AppResult> result = CreateResult(app->id(), false);

    // Update result from match.
    result->SetTitle(indexed_name->text());
    result->set_relevance(relevance);

    MaybeAddResult(&matches, std::move(result), &handled_results);
  }

  return matches;
}

std::unique_ptr<AppResult> AppSearchDataSource::CreateResult(
    const std::string& app_id,
    bool is_recommended) {
  return std::make_unique<AppServiceAppResult>(
      profile_, app_id, list_controller_, is_recommended, &icon_cache_);
}

void AppSearchDataSource::Refresh() {
  refresh_apps_factory_.InvalidateWeakPtrs();

  apps_.clear();
  apps_.reserve(kMinimumReservedAppsContainerCapacity);

  apps::AppServiceProxyFactory::GetForProfile(profile_)
      ->AppRegistryCache()
      .ForEachApp([this](const apps::AppUpdate& update) {
        if (!apps_util::IsInstalled(update.Readiness()) ||
            (!update.ShowInSearch().value_or(false) &&
             !(update.Recommendable().value_or(false) &&
               update.AppType() == apps::AppType::kBuiltIn))) {
          return;
        }

        // TODO(crbug.com/40569217): add the "can load in incognito" concept to
        // the App Service and use it here, similar to ExtensionDataSource.
        const std::string name = update.Name();

        apps_.emplace_back(std::make_unique<AppInfo>(
            update.AppId(), name, GetAppLastActivityTime(update)));
        // TODO(crbug.com/1364452): Test that non-recommendable apps are not
        // shown in the Recent Apps section.
        apps_.back()->set_recommendable(
            update.Recommendable().value_or(false) &&
            !update.Paused().value_or(false) &&
            !apps_util::IsDisabled(update.Readiness()) &&
            update.ShowInLauncher());
        apps_.back()->set_searchable(update.Searchable().value_or(false));

        for (const std::string& term : update.AdditionalSearchTerms()) {
          apps_.back()->AddSearchableText(base::UTF8ToUTF16(term));
        }
      });

  // Presort app based on last activity time in order to be able to remove
  // duplicates from results. We break ties by App ID, which is arbitrary, but
  // deterministic.
  std::sort(apps_.begin(), apps_.end(),
            AppInfo::CompareByLastActivityTimeAndThenAppId());

  app_updates_callback_list_.Notify();
}

void AppSearchDataSource::OnAppUpdate(const apps::AppUpdate& update) {
  if (!apps_util::IsInstalled(update.Readiness()) || update.IconKeyChanged()) {
    icon_cache_.RemoveIcon(update.AppId());
  }

  if (update.Readiness() == apps::Readiness::kReady) {
    ScheduleRefresh();
  } else {
    if (update.ReadinessChanged()) {
      Refresh();
    }
  }
}

void AppSearchDataSource::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void AppSearchDataSource::ScheduleRefresh() {
  if (refresh_apps_factory_.HasWeakPtrs()) {
    return;
  }

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AppSearchDataSource::Refresh,
                                refresh_apps_factory_.GetWeakPtr()));
}

}  // namespace app_list
