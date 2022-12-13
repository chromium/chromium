// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/search/burn_in_controller.h"
#include "chrome/browser/ash/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class ChromeSearchResult;
class Profile;

namespace ash {
class AppListNotifier;
enum class AppListSearchResultType;
}  // namespace ash

namespace app_list {

class SearchMetricsManager;
class SearchSessionMetricsManager;
class SearchProvider;

namespace test {
class SearchControllerImplTest;
}

class SearchControllerImpl : public SearchController {
 public:
  using ResultsChangedCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType)>;

  SearchControllerImpl(AppListModelUpdater* model_updater,
                       AppListControllerDelegate* list_controller,
                       ash::AppListNotifier* notifier,
                       Profile* profile);
  ~SearchControllerImpl() override;

  SearchControllerImpl(const SearchControllerImpl&) = delete;
  SearchControllerImpl& operator=(const SearchControllerImpl&) = delete;

  // SearchController:
  void StartSearch(const std::u16string& query) override;
  void ClearSearch() override;
  void StartZeroState(base::OnceClosure on_done,
                      base::TimeDelta timeout) override;
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
  void AppListClosing() override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void set_results_changed_callback_for_test(
      ResultsChangedCallback callback) override;
  std::u16string get_query() override;
  base::Time session_start() override;
  void disable_ranking_for_test() override;
  void WaitForZeroStateCompletionForTest(base::OnceClosure callback) override;

  void set_ranker_manager_for_test(
      std::unique_ptr<RankerManager> ranker_manager) {
    ranker_manager_ = std::move(ranker_manager);
  }

 private:
  friend class test::SearchControllerImplTest;

  // Rank the results of |provider_type|.
  void Rank(ash::AppListSearchResultType provider_type);

  void SetSearchResults(const SearchProvider* provider);

  void SetZeroStateResults(const SearchProvider* provider);

  void OnZeroStateTimedOut();

  void OnBurnInPeriodElapsed();

  void OnResultsChangedWithType(ash::AppListSearchResultType result_type);

  Profile* profile_;
  std::unique_ptr<BurnInController> burnin_controller_;

  // The query associated with the most recent search.
  std::u16string last_query_;

  // How many search providers should block zero-state until they return
  // results.
  int total_zero_state_blockers_ = 0;

  // How many zero-state blocking providers have returned for this search.
  int returned_zero_state_blockers_ = 0;

  // A timer to trigger a Publish at the end of the timeout period passed to
  // StartZeroState.
  base::OneShotTimer zero_state_timeout_;

  // Callbacks to run when initial set of zero state results is published.
  // Non empty list indicates that results should be published when zero state
  // times out.
  base::OnceClosureList on_zero_state_done_;

  // The time when StartSearch was most recently called.
  base::Time session_start_;

  // The ID of the most recently launched app. This is used for app list launch
  // recording.
  std::string last_launched_app_id_;

  // Top-level result ranker.
  std::unique_ptr<RankerManager> ranker_manager_;

  bool disable_ranking_for_test_ = false;

  // Storage for all search results for the current query.
  ResultsMap results_;

  // Storage for category scores for the current query.
  CategoriesList categories_;

  // If set, called when results set by a provider change. Only set by tests.
  ResultsChangedCallback results_changed_callback_for_test_;

  std::unique_ptr<SearchMetricsManager> metrics_manager_;
  std::unique_ptr<SearchSessionMetricsManager> session_metrics_manager_;
  std::unique_ptr<AppSearchDataSource> app_search_data_source_;
  using Providers = std::vector<std::unique_ptr<SearchProvider>>;
  Providers providers_;
  AppListModelUpdater* const model_updater_;
  AppListControllerDelegate* const list_controller_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_CONTROLLER_IMPL_H_
