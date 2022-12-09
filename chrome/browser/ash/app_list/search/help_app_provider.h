// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_HELP_APP_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_HELP_APP_PROVIDER_H_

#include <string>
#include <vector>

#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace ash::help_app {
class SearchHandler;
}  // namespace ash::help_app

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace app_list {

// Search results for the Help App (aka Explore).
class HelpAppResult : public ChromeSearchResult {
 public:
  HelpAppResult(const float& relevance,
                Profile* profile,
                const ash::help_app::mojom::SearchResultPtr& result,
                const gfx::ImageSkia& icon,
                const std::u16string& query);

  ~HelpAppResult() override;

  HelpAppResult(const HelpAppResult&) = delete;
  HelpAppResult& operator=(const HelpAppResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

 private:
  Profile* const profile_;
  const std::string url_path_;
  const std::string help_app_content_id_;
};

// Provides results from the Help App based on the search query.
class HelpAppProvider : public SearchProvider,
                        public apps::AppRegistryCache::Observer,
                        public ash::help_app::mojom::SearchResultsObserver {
 public:
  HelpAppProvider(Profile* profile,
                  ash::help_app::SearchHandler* search_handler);
  ~HelpAppProvider() override;

  HelpAppProvider(const HelpAppProvider&) = delete;
  HelpAppProvider& operator=(const HelpAppProvider&) = delete;

  // SearchProvider:
  void Start(const std::u16string& query) override;
  void StopQuery() override;
  ash::AppListSearchResultType ResultType() const override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // mojom::SearchResultsObserver:
  void OnSearchResultAvailabilityChanged() override;

 private:
  void OnSearchReturned(
      const std::u16string& query,
      const base::TimeTicks& start_time,
      std::vector<ash::help_app::mojom::SearchResultPtr> results);
  void OnLoadIcon(apps::IconValuePtr icon_value);
  void LoadIcon();

  Profile* const profile_;

  ash::help_app::SearchHandler* search_handler_;
  apps::AppServiceProxy* app_service_proxy_;
  gfx::ImageSkia icon_;

  // Last search query. It is reset when the view is closed.
  std::u16string last_query_;
  mojo::Receiver<ash::help_app::mojom::SearchResultsObserver>
      search_results_observer_receiver_{this};

  base::WeakPtrFactory<HelpAppProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_HELP_APP_PROVIDER_H_
