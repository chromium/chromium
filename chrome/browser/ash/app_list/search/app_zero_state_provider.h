// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_ZERO_STATE_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_ZERO_STATE_PROVIDER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

class AppListModelUpdater;

namespace app_list {

class AppSearchDataSource;

class AppZeroStateProvider : public SearchProvider {
 public:
  AppZeroStateProvider(AppSearchDataSource* data_source,
                       AppListModelUpdater* model_updater);

  AppZeroStateProvider(const AppZeroStateProvider&) = delete;
  AppZeroStateProvider& operator=(const AppZeroStateProvider&) = delete;

  ~AppZeroStateProvider() override;

  // SearchProvider overrides:
  void StartZeroState() override;
  ash::AppListSearchResultType ResultType() const override;

 private:
  void UpdateResults();

  // Updates the zero-state app recommendations ("recent apps").
  void UpdateRecommendedResults(
      const base::flat_map<std::string, uint16_t>& id_to_app_list_index);

  AppSearchDataSource* const data_source_;
  AppListModelUpdater* const model_updater_;

  base::TimeTicks query_start_time_;

  base::WeakPtrFactory<AppZeroStateProvider> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_ZERO_STATE_PROVIDER_H_
