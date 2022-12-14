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

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/ranking/launch_data.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace ash {
enum class AppListSearchResultType;
}

namespace base {
class Time;
class TimeDelta;
}

namespace app_list {

class AppSearchDataSource;
class SearchProvider;

// Controller that collects query from given SearchBoxModel, dispatches it
// to all search providers, then invokes the mixer to mix and to publish the
// results to the given SearchResults UI model.
class SearchController {
 public:
  using ResultsChangedCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType)>;

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
        const std::vector<const ChromeSearchResult*>& results) {}
  };

  virtual ~SearchController() = default;

  virtual void StartSearch(const std::u16string& query) = 0;
  virtual void ClearSearch() = 0;
  virtual void StartZeroState(base::OnceClosure on_done,
                              base::TimeDelta timeout) = 0;

  virtual void AppListClosing() = 0;

  virtual void OpenResult(ChromeSearchResult* result, int event_flags) = 0;
  virtual void InvokeResultAction(ChromeSearchResult* result,
                                  ash::SearchResultActionType action) = 0;

  // Returns AppSearchDataSource instance that should be used with app search
  // providers.
  virtual AppSearchDataSource* GetAppSearchDataSource() = 0;

  // Takes ownership of |provider|.
  virtual void AddProvider(std::unique_ptr<SearchProvider> provider) = 0;

  // Removes, and deletes registered search providers that provide results for
  // `result_type` and adds a new "test" provider.
  // No-op if no providers for `result_type` were previously registered.
  // Expects that `provider` provides results for `result_type`.
  // Returns number of providers removed from the provider list.
  virtual size_t ReplaceProvidersForResultTypeForTest(
      ash::AppListSearchResultType result_type,
      std::unique_ptr<SearchProvider> provider) = 0;

  // Update the controller with the given results. Used only if the categorical
  // search feature flag is enabled.
  virtual void SetResults(const SearchProvider* provider, Results results) = 0;
  // Publishes results to ash.
  virtual void Publish() = 0;

  virtual ChromeSearchResult* FindSearchResult(
      const std::string& result_id) = 0;
  virtual ChromeSearchResult* GetResultByTitleForTest(
      const std::string& title) = 0;

  // Sends training signal to each |providers_|
  virtual void Train(LaunchData&& launch_data) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  virtual std::u16string get_query() = 0;

  virtual base::Time session_start() = 0;

  virtual void set_results_changed_callback_for_test(
      ResultsChangedCallback callback) = 0;

  virtual void disable_ranking_for_test() = 0;

  // Registers a callback to be run when zero state search returns (either due
  // to all zero state providers returning results, or a timeout). The callback
  // will run immediately if there is no pending zero state search callback.
  virtual void WaitForZeroStateCompletionForTest(
      base::OnceClosure callback) = 0;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
