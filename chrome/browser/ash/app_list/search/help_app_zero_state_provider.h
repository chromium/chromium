// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_HELP_APP_ZERO_STATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_HELP_APP_ZERO_STATE_PROVIDER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/icon_types.h"

class Profile;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace app_list {

// Search results for the Help App (aka Explore).
class HelpAppZeroStateResult : public ChromeSearchResult {
 public:
  HelpAppZeroStateResult(Profile* profile,
                         const std::string& id,
                         DisplayType display_type,
                         const std::u16string& title,
                         const std::u16string& details,
                         const gfx::ImageSkia& icon);

  ~HelpAppZeroStateResult() override;

  HelpAppZeroStateResult(const HelpAppZeroStateResult&) = delete;
  HelpAppZeroStateResult& operator=(const HelpAppZeroStateResult&) = delete;

  // ChromeSearchResult overrides:
  void Open(int event_flags) override;

 private:
  const raw_ptr<Profile> profile_;
};

// Provides zero-state results from the Help App.
class HelpAppZeroStateProvider : public SearchProvider,
                                 public apps::AppRegistryCache::Observer,
                                 public ash::AppListNotifier::Observer {
 public:
  HelpAppZeroStateProvider(Profile* profile, ash::AppListNotifier* notifier);
  ~HelpAppZeroStateProvider() override;

  HelpAppZeroStateProvider(const HelpAppZeroStateProvider&) = delete;
  HelpAppZeroStateProvider& operator=(const HelpAppZeroStateProvider&) = delete;

  // SearchProvider:
  void StartZeroState() override;
  ash::AppListSearchResultType ResultType() const override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // ash::AppListNotifier::Observer:
  void OnImpression(ash::AppListNotifier::Location location,
                    const std::vector<ash::AppListNotifier::Result>& results,
                    const std::u16string& query) override;

 private:
  void OnLoadIcon(apps::IconValuePtr icon_value);
  void LoadIcon();

  const raw_ptr<Profile> profile_;

  gfx::ImageSkia icon_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::ScopedObservation<ash::AppListNotifier, ash::AppListNotifier::Observer>
      notifier_observer_{this};

  base::WeakPtrFactory<HelpAppZeroStateProvider> weak_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_HELP_APP_ZERO_STATE_PROVIDER_H_
