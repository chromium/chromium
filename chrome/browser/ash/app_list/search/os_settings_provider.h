// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_OS_SETTINGS_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_OS_SETTINGS_PROVIDER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/search.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace ash::settings {
class Hierarchy;
class SearchHandler;
}  // namespace ash::settings

namespace ui {
class ImageModel;
}  // namespace ui

namespace app_list {

// Search results for OS settings.
class OsSettingsResult : public ChromeSearchResult {
 public:
  OsSettingsResult(Profile* profile,
                   const ash::settings::mojom::SearchResultPtr& result,
                   double relevance_score,
                   const ui::ImageModel& icon,
                   const std::u16string& query);
  ~OsSettingsResult() override;

  OsSettingsResult(const OsSettingsResult&) = delete;
  OsSettingsResult& operator=(const OsSettingsResult&) = delete;

  // ChromeSearchResult:
  void Open(int event_flags) override;

 private:
  raw_ptr<Profile, ExperimentalAsh> profile_;
  const std::string url_path_;
};

// Provider results for OS settings based on a search query. No results are
// provided for zero-state.
class OsSettingsProvider : public SearchProvider,
                           public apps::AppRegistryCache::Observer,
                           public ash::settings::mojom::SearchResultsObserver {
 public:
  OsSettingsProvider(Profile* profile,
                     ash::settings::SearchHandler* search_handler,
                     const ash::settings::Hierarchy* hierarchy);
  ~OsSettingsProvider() override;

  OsSettingsProvider(const OsSettingsProvider&) = delete;
  OsSettingsProvider& operator=(const OsSettingsProvider&) = delete;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // mojom::SearchResultsObserver:
  void OnSearchResultsChanged() override;

 private:
  void OnSearchReturned(
      const std::u16string& query,
      const base::TimeTicks& start_time,
      std::vector<ash::settings::mojom::SearchResultPtr> results);

  // Given a vector of results from the SearchHandler, filters them down to a
  // display-ready vector. It:
  // - returns at most |kMaxShownResults| results
  // - removes results with duplicate IDs
  // - removes results with relevance score below |min_score_|.
  // - removes results matching alternate text unless they meet extra
  //   requirements.
  //
  // The SearchHandler's vector is ranked high-to-low with this logic:
  // - compares SearchResultDefaultRank,
  // - if equal, compares relevance scores
  // - if equal, compares SearchResultType, favoring sections over subpages over
  //   settings
  // - if equal, picks arbitrarily
  //
  // So simply iterating down the vector while being careful about duplicates
  // and checking for alternate matches is enough.
  std::vector<ash::settings::mojom::SearchResultPtr> FilterResults(
      const std::u16string& query,
      const std::vector<ash::settings::mojom::SearchResultPtr>& results,
      const ash::settings::Hierarchy* hierarchy);

  void OnLoadIcon(bool is_from_constructor, apps::IconValuePtr icon_value);

  // Scoring and filtering parameters.
  bool accept_alternate_matches_ = false;
  size_t min_query_length_ = 4u;
  size_t min_query_length_for_alternates_ = 4u;
  float min_score_ = 0.4f;
  float min_score_for_alternates_ = 0.4f;

  const raw_ptr<Profile, ExperimentalAsh> profile_;
  raw_ptr<ash::settings::SearchHandler, ExperimentalAsh> search_handler_;
  raw_ptr<const ash::settings::Hierarchy, ExperimentalAsh> hierarchy_;
  ui::ImageModel icon_;

  // Last query. It is reset when view is closed.
  std::u16string last_query_;
  mojo::Receiver<ash::settings::mojom::SearchResultsObserver>
      search_results_observer_receiver_{this};

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::WeakPtrFactory<OsSettingsProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_OS_SETTINGS_PROVIDER_H_
