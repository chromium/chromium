// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_LAUNCH_DATA_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_LAUNCH_DATA_H_

#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

// Data concerning the app launch. Used for training models.
struct LaunchData {
  LaunchData();
  ~LaunchData();

  LaunchData(LaunchData&& other);
  // ID of the item launched.
  std::string id;
  // The type of the result.
  ash::AppListSearchResultType result_type =
      ash::AppListSearchResultType::kUnknown;
  ash::AppListLaunchedFrom launched_from =
      ash::AppListLaunchedFrom::kLaunchedFromShelf;
  // The category of the result.
  Category category = Category::kUnknown;
  // The type of app launched.
  ash::AppListLaunchType launch_type = ash::AppListLaunchType::kSearchResult;
  // The index of the suggested app launched, if applicable.
  int suggestion_index = 0;
  // User's search query string, if relevant.
  std::string query;
  // The score of the suggestion.
  double score = 0.0;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_LAUNCH_DATA_H_
