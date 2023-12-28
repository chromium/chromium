// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_SORTING_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_SORTING_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

// Determine category display order by sorting the given list of |categories|
// in-place.
//
// Sorting criteria in descending precedence are:
// - Burn-in iteration number.
// - Score.
void SortCategories(CategoriesList& categories);

// Determine result display order by sorting the given list of |results|
// in-place. |categories| is assumed to be already sorted into category display
// order.
//
// Sorting criteria in descending precedence are:
// - Best match
// - Category
// - Burn-in iteration number
// - Display score.
void SortResults(
    std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>& results,
    const CategoriesList& categories);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_SORTING_H_
