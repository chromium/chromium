// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_RANKER_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_RANKER_MANAGER_H_

#include <memory>
#include <vector>

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace app_list {

// A test ranker manager that circumvents all result rankings, and hardcodes
// category ranking.
class TestRankerManager : public RankerManager {
 public:
  explicit TestRankerManager(Profile* profile);
  ~TestRankerManager() override;

  TestRankerManager(const TestRankerManager&) = delete;
  TestRankerManager& operator=(const TestRankerManager&) = delete;

  void SetCategoryRanks(
      base::flat_map<ash::AppListSearchResultCategory, double> category_ranks);

  void SetBestMatchString(const std::u16string& best_match_string);

  // Ranker:
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;

  // Ranker:
  void UpdateCategoryRanks(const ResultsMap& results,
                           CategoriesList& categories,
                           ProviderType provider) override;

  // Ranker:
  void Start(const std::u16string& query,
             const CategoriesList& categories) override {}
  void Train(const LaunchData& launch) override;
  void Remove(ChromeSearchResult* result) override {}

  // Return true if |Train()| has been called. Otherwise, return false;
  bool did_train() const { return did_train_; }

 private:
  base::flat_map<Category, double> category_ranks_;
  std::u16string best_match_string_;
  bool did_train_ = false;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_RANKER_MANAGER_H_
