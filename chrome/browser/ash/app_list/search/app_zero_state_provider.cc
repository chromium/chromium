// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_zero_state_provider.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/app_search_data_source.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

namespace app_list {

AppZeroStateProvider::AppZeroStateProvider(AppSearchDataSource* data_source)
    : SearchProvider(SearchCategory::kApps), data_source_(data_source) {
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
  base::TimeTicks query_start_time = base::TimeTicks::Now();
  data_source_->RefreshIfNeeded();
  SearchProvider::Results new_results = data_source_->GetRecommendations();
  UMA_HISTOGRAM_TIMES("Apps.AppList.AppSearchProvider.ZeroStateLatency",
                      base::TimeTicks::Now() - query_start_time);
  SwapResults(&new_results);
}

ash::AppListSearchResultType AppZeroStateProvider::ResultType() const {
  return ash::AppListSearchResultType::kZeroStateApp;
}

}  // namespace app_list
