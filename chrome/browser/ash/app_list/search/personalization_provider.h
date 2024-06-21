// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_PERSONALIZATION_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_PERSONALIZATION_PROVIDER_H_

#include <string>
#include <vector>

#include "ash/webui/personalization_app/search/search.mojom.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/models/image_model.h"

class Profile;

namespace ash::personalization_app {

class SearchHandler;

}  // namespace ash::personalization_app

namespace app_list {

class PersonalizationResult : public ChromeSearchResult {
 public:
  PersonalizationResult(
      Profile* profile,
      const ash::personalization_app::mojom::SearchResult& result,
      const std::u16string& query,
      gfx::ImageSkia icon);

  PersonalizationResult(const PersonalizationResult&) = delete;
  PersonalizationResult& operator=(const PersonalizationResult&) = delete;

  ~PersonalizationResult() override;

  // ChromeSearchResult:
  void Open(int event_flags) override;

 private:
  const raw_ptr<Profile> profile_;
};

// Provides search results for Personalization App based on a search query. No
// results are provided for zero-state.
class PersonalizationProvider
    : public SearchProvider,
      public ::ash::personalization_app::mojom::SearchResultsObserver,
      public ::apps::AppRegistryCache::Observer,
      public session_manager::SessionManagerObserver {
 public:
  explicit PersonalizationProvider(Profile* profile);
  ~PersonalizationProvider() override;

  PersonalizationProvider(const PersonalizationProvider&) = delete;
  PersonalizationProvider& operator=(const PersonalizationProvider&) = delete;

  // Initialize the provider. It should be called when:
  //    1. User session start up tasks has completed.
  //    2. User session start up tasks has not completed, but user has start to
  //    search in launcher.
  //    3. In tests with fake search handler provided.
  void MaybeInitialize(
      ::ash::personalization_app::SearchHandler* fake_search_handler = nullptr);

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

  // ::ash::personalization_app::mojom::SearchResultsObserver:
  void OnSearchResultsChanged() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // session_manager::SessionManagerObserver:
  void OnUserSessionStartUpTaskCompleted() override;

 private:
  void OnSearchDone(
      base::TimeTicks start_time,
      std::vector<::ash::personalization_app::mojom::SearchResultPtr> results);

  void StartLoadIcon();

  void OnLoadIcon(::apps::IconValuePtr icon_value);

  const raw_ptr<Profile> profile_;
  std::u16string current_query_;
  gfx::ImageSkia icon_;
  bool has_initialized = false;

  raw_ptr<::ash::personalization_app::SearchHandler> search_handler_ = nullptr;
  mojo::Receiver<::ash::personalization_app::mojom::SearchResultsObserver>
      search_results_observer_{this};

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_manager_observation_{this};

  base::WeakPtrFactory<PersonalizationProvider> weak_ptr_factory_{this};
  base::WeakPtrFactory<PersonalizationProvider> app_service_weak_ptr_factory_{
      this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_PERSONALIZATION_PROVIDER_H_
