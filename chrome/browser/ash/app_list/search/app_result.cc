// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_result.h"

#include "base/time/time.h"

namespace app_list {

AppResult::AppResult(Profile* profile,
                     const std::string& app_id,
                     AppListControllerDelegate* controller,
                     bool is_recommendation)
    : profile_(profile),
      app_id_(app_id),
      controller_(controller) {
  SetDisplayType(ash::SearchResultDisplayType::kList);
  SetResultType(ash::AppListSearchResultType::kInstalledApp);
  SetMetricsType(ash::SEARCH_RESULT_TYPE_BOUNDARY);
  SetIsRecommendation(is_recommendation);
  SetCategory(Category::kApps);
}

AppResult::~AppResult() {
}

void AppResult::UpdateFromLastLaunchedOrInstalledTime(
    const base::Time& current_time,
    const base::Time& old_time) {
  // |current_time| can be before |old_time| in weird cases such as users
  // playing with their clocks. Handle this gracefully.
  if (current_time < old_time) {
    set_relevance(1.0);
    return;
  }

  const double weeks = (current_time - old_time) / base::Days(7);

  // Set the relevance to a value between 0 and 1. This function decays as the
  // time delta increases and reaches a value of 0.5 at 1 week.
  set_relevance(1 / (weeks + 1));
}

}  // namespace app_list
