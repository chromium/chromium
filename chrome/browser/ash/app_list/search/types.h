// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TYPES_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TYPES_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/containers/flat_map.h"
#include "chrome/browser/profiles/profile.h"

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

// All possible categories for the launcher search results. It's used to
// indicate the category of the result shown to the user in launcher.
using Category = ash::AppListSearchResultCategory;

// A wrapper struct around a category to hold scoring information.
struct CategoryMetadata {
  Category category = Category::kUnknown;
  double score = 0.0;

  // Same purpose, meaning, and incrementing rules as the burn_in_iteration
  // member of the Scoring struct, except this member is for categories rather
  // than individual results. Additionally, -1 signifies that the category has
  // not yet been seen in the current search.
  int burn_in_iteration = -1;
};

using CategoriesList = std::vector<CategoryMetadata>;

CategoriesList CreateAllCategories();

// The control category of launcher search providers. It's used to indicate
// whether or not we want to show the results of a particular control category
// to the user in launcher.
using ControlCategory = ash::AppListSearchControlCategory;

// The help function to check if the control category is enabled.
bool IsControlCategoryEnabled(const Profile* profile,
                              const ControlCategory control_category);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TYPES_H_
