// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TYPES_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TYPES_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/containers/flat_map.h"

class ChromeSearchResult;

namespace app_list {

// The type of a particular result.
using ResultType = ash::AppListSearchResultType;
// The type of a search provider as a whole. This is currently just the 'main'
// ResultType returned by the provider.
using ProviderType = ash::AppListSearchResultType;
// The display type of a search result, indicating which UI section it belongs
// to.
using DisplayType = ash::SearchResultDisplayType;

// Common types used throughout result ranking.
using Results = std::vector<std::unique_ptr<ChromeSearchResult>>;
using ResultsMap = base::flat_map<ProviderType, Results>;

// All possible categories.
using Category = ash::AppListSearchResultCategory;

// A wrapper struct around a category to hold scoring information.
struct CategoryMetadata {
  Category category = Category::kUnknown;
  double score = 0.0;

  // Same purpose, meaning, and incrementing rules as the burnin_iteration
  // member of the Scoring struct above, except this member is for categories
  // rather than individual results. Additionally, -1 signifies that the
  // category has not yet been seen in the current search.
  int burnin_iteration = -1;
};

using CategoriesList = std::vector<CategoryMetadata>;

CategoriesList CreateAllCategories();

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TYPES_H_
