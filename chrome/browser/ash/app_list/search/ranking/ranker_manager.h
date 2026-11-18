// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_RANKER_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_RANKER_MANAGER_H_

#include "chrome/browser/ash/app_list/search/ranking/ranker.h"

class ChromeSearchResult;

namespace user_manager {
class User;
}  // namespace user_manager

namespace app_list {

// A manager for a series of rankers. Rankers can be added via AddRanker, and
// all other methods will delegate the call to each ranker in the order they
// were added.
//
// This is the place to configure experiments or flags that change ranking
// behavior.
class RankerManager : public Ranker {
 public:
  // `user` must not be null and must outlive this instance: this
  // constructor doesn't store it itself, but passes it to the
  // RemovedResultsRanker it creates and owns for its own lifetime, and
  // that one does store a reference to it.
  explicit RankerManager(user_manager::User* user);
  ~RankerManager() override;

  RankerManager(const RankerManager&) = delete;
  RankerManager& operator=(const RankerManager&) = delete;

  // Ranker:
  void Start(const std::u16string& query,
             const CategoriesList& categories) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
  void UpdateCategoryRanks(const ResultsMap& results,
                           CategoriesList& categories,
                           ProviderType provider) override;
  void Train(const LaunchData& launch) override;
  void Remove(ChromeSearchResult* result) override;
  void OnBurnInPeriodElapsed() override;

 private:
  void AddRanker(std::unique_ptr<Ranker> ranker);

  std::vector<std::unique_ptr<Ranker>> rankers_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_RANKER_MANAGER_H_
