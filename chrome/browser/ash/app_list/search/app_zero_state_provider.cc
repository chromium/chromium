// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_zero_state_provider.h"

#include <string>
#include <utility>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/search/app_search_data_source.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

namespace app_list {

AppZeroStateProvider::AppZeroStateProvider(AppSearchDataSource* data_source,
                                           AppListModelUpdater* model_updater)
    : data_source_(data_source), model_updater_(model_updater) {
  // NOTE: Unlike AppSearchProvider, AppZeroStateProvider does not have to
  // update search model when app status, or other app information changes. The
  // recent apps UI implementation updates app representations independently of
  // search model, using app list model directly. The UI only uses search model
  // to determine preferred app display order - updating search model may change
  // order of apps, which would be undesirable UI behavior (it could be
  // perceived as pop-in after app list has been shown).
  // If the UI behavior changes, the decision not to update search model for
  // recent apps whenever app service state changes should be reevaluated.
}

AppZeroStateProvider::~AppZeroStateProvider() = default;

void AppZeroStateProvider::StartZeroState() {
  data_source_->RefreshIfNeeded();
  UpdateResults();
}

ash::AppListSearchResultType AppZeroStateProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateApp;
}

void AppZeroStateProvider::UpdateRecommendedResults(
    const base::flat_map<std::string, uint16_t>& id_to_app_list_index) {
  SearchProvider::Results new_results =
      data_source_->GetRecommendations(id_to_app_list_index);
  UMA_HISTOGRAM_TIMES("Apps.AppList.AppSearchProvider.ZeroStateLatency",
                      base::TimeTicks::Now() - query_start_time_);

  SwapResults(&new_results);
}

void AppZeroStateProvider::UpdateResults() {
  // Get the map of app ids to their position in the app list, and then
  // update results.
  // Unretained is safe because the callback gets called synchronously.
  model_updater_->GetIdToAppListIndexMap(base::BindOnce(
      &AppZeroStateProvider::UpdateRecommendedResults, base::Unretained(this)));
}

}  // namespace app_list
