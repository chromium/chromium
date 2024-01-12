// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SEARCH_DATA_SOURCE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SEARCH_DATA_SOURCE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_cache.h"

class AppListControllerDelegate;
class Profile;

namespace apps {
class AppUpdate;
}  // namespace apps

namespace base {
class Clock;
}

namespace app_list {

class AppResult;

// Aggregates and tracks information about installed apps relevant to launcher
// search. It's an abstraction on top of AppService that app search providers
// can use to generate search results for different queries.
class AppSearchDataSource : public apps::AppRegistryCache::Observer {
 public:
  AppSearchDataSource(Profile* profile,
                      AppListControllerDelegate* list_controller,
                      base::Clock* clock);

  AppSearchDataSource(const AppSearchDataSource&) = delete;
  AppSearchDataSource& operator=(const AppSearchDataSource&) = delete;

  ~AppSearchDataSource() override;

  // Registers a callback to be run when the set of cached apps changes. Search
  // providers can use this to update their ppublished results when apps get
  // updated.
  // Returns a subscription which keeps `callback` registered. To unregister the
  // callback, let the subscription object go out of scope.
  base::CallbackListSubscription SubscribeToAppUpdates(
      const base::RepeatingClosure& callback);

  // Immediately update the app info cache if it's not fresh.
  void RefreshIfNeeded();

  // Returns app recommendations (zero-state search results).
  SearchProvider::Results GetRecommendations();

  // Returns app results that match `query`. It uses exact matching algorithm.
  SearchProvider::Results GetExactMatches(const std::u16string& query);

  // Returns app results that match `query`. It uses fuzzy matching algorithm.
  SearchProvider::Results GetFuzzyMatches(const std::u16string& query);

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  class AppInfo;

  std::unique_ptr<AppResult> CreateResult(const std::string& app_id,
                                          bool is_recommended);

  // Updates cached app info to match current app states in app service.
  void Refresh();

  // Schedules asynchronous `Refresh()` if one is not already scheduled.
  // Called when apps get updated. The refresh is asynchronous to avoid
  // repeatedly calling `Refresh()` during batch updates in app service.
  void ScheduleRefresh();

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  const raw_ptr<AppListControllerDelegate> list_controller_;
  const raw_ptr<base::Clock> clock_;

  base::CallbackListSubscription foreign_session_updated_subscription_;

  // The AppSearchDataSource seems like one (but not the only) good place to
  // add an App Service icon caching wrapper, because (1) the AppSearchProvider
  // destroys and creates multiple search results in a short period of time,
  // while the user is typing, so will clearly benefit from a cache, and (2)
  // there is an obvious point in time when the cache can be emptied: the user
  // will obviously stop typing (so stop triggering LoadIcon requests) when the
  // search box view closes.
  //
  // There are reasons to have more than one icon caching layer. See the
  // comments for the apps::IconCache::GarbageCollectionPolicy enum.
  apps::IconCache icon_cache_;

  std::vector<std::unique_ptr<AppInfo>> apps_;

  base::RepeatingClosureList app_updates_callback_list_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  // Weak ptr factory for `ScheduleRefresh()` tasks - used to track and easily
  // cancel scheduled tasks.
  base::WeakPtrFactory<AppSearchDataSource> refresh_apps_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SEARCH_DATA_SOURCE_H_
