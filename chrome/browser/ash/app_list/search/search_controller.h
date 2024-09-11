// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/search/app_discovery_metrics_manager.h"
#include "chrome/browser/ash/app_list/search/burn_in_controller.h"
#include "chrome/browser/ash/app_list/search/common/keyword_util.h"
#include "chrome/browser/ash/app_list/search/federated_metrics_manager.h"
#include "chrome/browser/ash/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"
#include "chrome/browser/ash/app_list/search/search_file_scanner.h"
#include "chrome/browser/ash/app_list/search/types.h"

class AppListControllerDelegate;
class AppListModelUpdater;
class ChromeSearchResult;
class Profile;

namespace ash {
class AppListNotifier;

namespace federated {
class FederatedServiceController;
}  // namespace federated

}  // namespace ash

namespace app_list {

class AppSearchDataSource;
class SearchMetricsManager;
class SearchSessionMetricsManager;
class SearchProvider;
class SearchEngine;
class SparkyEventRewriter;

// Long queries will be truncated down to this length.
constexpr int kMaxAllowedQueryLength = 500;

// A controller that collects queries from the AppListClient, dispatches them to
// search providers, then ranks and publishes the results to the AppListModel.
// Many methods are virtual for testing.
class SearchController {
 public:
  using ResultsChangedCallback = base::RepeatingCallback<void(ResultType)>;

  SearchController(AppListModelUpdater* model_updater,
                   AppListControllerDelegate* list_controller,
                   ash::AppListNotifier* notifier,
                   Profile* profile,
                   ash::federated::FederatedServiceController*
                       federated_service_controller_);
  virtual ~SearchController();

  SearchController(const SearchController&) = delete;
  SearchController& operator=(const SearchController&) = delete;

  class Observer : public base::CheckedObserver {
   public:
    // Called whenever results are added to the launcher, as a result of
    // zero-state or from a user query. This will be called multiple times per
    // query because launcher results arrive incrementally.
    //
    // Observers should not store the ChromeSearchResult* pointers or post them
    // to another sequence because they may be invalidated.
    virtual void OnResultsAdded(
        const std::u16string& query,
        const std::vector<KeywordInfo>& extracted_keyword_info,
        const std::vector<const ChromeSearchResult*>& results) {}
  };

  // Initializes required members of the SearchController. Must be called at
  // construction time.
  // This is separate from the constructor itself so that it can be omitted for
  // the TestSearchController mock.
  void Initialize();

  // Returns the search categories that are available for users to choose if
  // they want to have the results in the categories displayed in launcher
  // search.
  std::vector<ash::AppListSearchControlCategory> GetToggleableCategories()
      const;

  // Takes ownership of |provider|.
  virtual void AddProvider(std::unique_ptr<SearchProvider> provider);

  virtual void StartSearch(const std::u16string& query);
  virtual void ClearSearch();

  virtual void StartZeroState(base::OnceClosure on_done,
                              base::TimeDelta timeout);

  // Callback made when app list view is open or closed. |is_visible| should be
  // true when the view is open.
  void AppListViewChanging(bool is_visible);

  void OpenResult(ChromeSearchResult* result, int event_flags);
  void InvokeResultAction(ChromeSearchResult* result,
                          ash::SearchResultActionType action);

  // Update the controller with the given results.
  virtual void SetResults(ResultType result_type, Results results);

  // Publishes results to ash.
  void Publish();

  // Sends training signal to each of |providers_|.
  void Train(LaunchData&& launch_data);

  // Returns the AppSearchDataSource instance that should be used with app
  // search providers.
  AppSearchDataSource* GetAppSearchDataSource();

  ChromeSearchResult* FindSearchResult(const std::string& result_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void OnDefaultSearchIsGoogleSet(bool is_google);

  std::u16string get_query();

  base::Time session_start();

  // Test methods.

  // Removes, and deletes registered search providers that provide results for
  // `result_type` and adds a new "test" provider.
  // No-op if no providers for `result_type` were previously registered.
  // Expects that `provider` provides results for `result_type`.
  // Returns number of providers removed from the provider list.
  size_t ReplaceProvidersForResultTypeForTest(
      ResultType result_type,
      std::unique_ptr<SearchProvider> provider);

  ChromeSearchResult* GetResultByTitleForTest(const std::string& title);

  virtual void WaitForZeroStateCompletionForTest(base::OnceClosure callback);

  virtual void set_results_changed_callback_for_test(
      ResultsChangedCallback callback);

  void disable_ranking_for_test();

  void set_ranker_manager_for_test(
      std::unique_ptr<RankerManager> ranker_manager) {
    ranker_manager_ = std::move(ranker_manager);
  }

  BurnInController* burn_in_controller_for_test() {
    return burn_in_controller_.get();
  }

  const CategoriesList& categories_for_test() { return categories_; }

 private:
  // Rank the results of |provider_type|.
  void Rank(ResultType provider_type);

  void SetSearchResults(ResultType result_type);

  void SetZeroStateResults(ResultType result_type);

  void OnZeroStateTimedOut();

  void OnBurnInPeriodElapsed();

  void OnResultsChangedWithType(ResultType result_type);

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

  // Storage for all search results for the current query.
  ResultsMap results_;

  // Storage for category scores for the current query.
  CategoriesList categories_;

  bool disable_ranking_for_test_ = false;

  std::vector<ControlCategory> toggleable_categories_;

  // If set, called when results set by a provider change. Only set by tests.
  ResultsChangedCallback results_changed_callback_for_test_;

  const raw_ptr<Profile> profile_;

  std::unique_ptr<BurnInController> burn_in_controller_;
  std::unique_ptr<RankerManager> ranker_manager_;

  std::unique_ptr<SearchMetricsManager> metrics_manager_;
  std::unique_ptr<SearchSessionMetricsManager> session_metrics_manager_;
  std::unique_ptr<federated::FederatedMetricsManager>
      federated_metrics_manager_;
  std::unique_ptr<AppDiscoveryMetricsManager> app_discovery_metrics_manager_;

  std::unique_ptr<AppSearchDataSource> app_search_data_source_;

  // TODO(b/315709613):Temporary before it is moved to a new service.
  std::unique_ptr<SearchEngine> search_engine_;

  std::unique_ptr<SearchFileScanner> search_file_scanner_;
  std::unique_ptr<SparkyEventRewriter> sparky_event_rewriter_;

  const raw_ptr<AppListModelUpdater> model_updater_;
  const raw_ptr<AppListControllerDelegate> list_controller_;
  const raw_ptr<ash::AppListNotifier> notifier_;
  const raw_ptr<ash::federated::FederatedServiceController>
      federated_service_controller_;

  base::ObserverList<Observer> observer_list_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
