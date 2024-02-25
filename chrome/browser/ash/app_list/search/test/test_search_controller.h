// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_SEARCH_CONTROLLER_H_

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"

class ChromeSearchResult;

namespace app_list {

class AppSearchDataSource;
class SearchProvider;

class TestSearchController : public SearchController {
 public:
  TestSearchController();
  ~TestSearchController() override;

  TestSearchController(const TestSearchController&) = delete;
  TestSearchController& operator=(const TestSearchController&) = delete;

  Results& last_results() { return last_results_; }

  // SearchController:
  void AddProvider(std::unique_ptr<SearchProvider> provider) override;
  void StartSearch(const std::u16string& query) override;
  void ClearSearch() override;
  void StartZeroState(base::OnceClosure on_done,
                      base::TimeDelta timeout) override;
  void SetResults(ash::AppListSearchResultType result_type,
                  Results results) override;
  void WaitForZeroStateCompletionForTest(base::OnceClosure callback) override;
  void set_results_changed_callback_for_test(
      ResultsChangedCallback callback) override;

 private:
  std::unique_ptr<SearchProvider> provider_;

  Results last_results_;
  ResultsChangedCallback results_changed_callback_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_SEARCH_CONTROLLER_H_
