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
  void ClearSearch() override;
  void StartSearch(const std::u16string& query) override;
  void StartZeroState(base::OnceClosure on_done,
                      base::TimeDelta timeout) override;
  void AppListClosing() override;
  void OpenResult(ChromeSearchResult* result, int event_flags) override;
  void InvokeResultAction(ChromeSearchResult* result,
                          ash::SearchResultActionType action) override;
  AppSearchDataSource* GetAppSearchDataSource() override;
  void AddProvider(std::unique_ptr<SearchProvider> provider) override;
  size_t ReplaceProvidersForResultTypeForTest(
      ash::AppListSearchResultType result_type,
      std::unique_ptr<SearchProvider> provider) override;
  void SetResults(const SearchProvider* provider, Results results) override;
  void Publish() override;
  ChromeSearchResult* FindSearchResult(const std::string& result_id) override;
  ChromeSearchResult* GetResultByTitleForTest(
      const std::string& title) override;
  void Train(LaunchData&& launch_data) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  std::u16string get_query() override;
  base::Time session_start() override;
  void set_results_changed_callback_for_test(
      ResultsChangedCallback callback) override;
  void disable_ranking_for_test() override;
  void WaitForZeroStateCompletionForTest(base::OnceClosure callback) override;

 private:
  std::unique_ptr<SearchProvider> provider_;

  Results last_results_;
  ResultsChangedCallback results_changed_callback_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_TEST_SEARCH_CONTROLLER_H_
