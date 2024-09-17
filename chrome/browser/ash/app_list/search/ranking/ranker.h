// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_RANKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_RANKER_H_

#include "chrome/browser/ash/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ash/app_list/search/types.h"

#include <string>

namespace app_list {

// Interface for all kinds of rankers. Primarily owned and called by
// SearchController.
class Ranker {
 public:
  Ranker() = default;
  virtual ~Ranker() = default;

  Ranker(const Ranker&) = delete;
  Ranker& operator=(const Ranker&) = delete;

  // Called each time a new search 'session' begins, eg. when the user opens the
  // launcher or changes the query.
  virtual void Start(const std::u16string& query,
                     const CategoriesList& categories);

  // Ranks search results. Should return a vector of scores that is the same
  // length as |results|.
  virtual std::vector<double> GetResultRanks(const ResultsMap& results,
                                             ProviderType provider);

  // Ranks search results. Implementations should modify the scoring structs of
  // |results|, but not modify the ordering of the vector itself.
  virtual void UpdateResultRanks(ResultsMap& results, ProviderType provider);

  // Ranks categories. Should return a vector of scores that is the same
  // length as |categories|.
  virtual std::vector<double> GetCategoryRanks(const ResultsMap& results,
                                               const CategoriesList& categories,
                                               ProviderType provider);

  // Ranks categories. Implementations should modify the scoring members of
  // structs in |categories|, but not modify the ordering of the vector itself.
  virtual void UpdateCategoryRanks(const ResultsMap& results,
                                   CategoriesList& categories,
                                   ProviderType provider);

  // Called each time a user launches a result.
  virtual void Train(const LaunchData& launch);

  // Called each time a user removes a result.
  virtual void Remove(ChromeSearchResult* result);

  // Called via callback within SearchControllerImplNew when the burn-in period
  // has elapsed and before the at-burn-in publish occurs.
  virtual void OnBurnInPeriodElapsed();
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_RANKER_H_
