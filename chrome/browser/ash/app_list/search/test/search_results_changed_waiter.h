// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_SEARCH_RESULTS_CHANGED_WAITER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_SEARCH_RESULTS_CHANGED_WAITER_H_

#include <set>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"

namespace app_list {

class SearchController;

// Helper that observes app list search controller, and waits until search
// result sets get updated for a subset of search result providers.
class SearchResultsChangedWaiter {
 public:
  // NOTE: This should be instantiated before search request starts, otherwise
  // it may miss result updates from providers that reports results
  // synchronously.
  SearchResultsChangedWaiter(
      SearchController* search_controller,
      const std::set<ash::AppListSearchResultType>& types);

  SearchResultsChangedWaiter(const SearchResultsChangedWaiter&) = delete;
  SearchResultsChangedWaiter& operator=(const SearchResultsChangedWaiter&) =
      delete;

  ~SearchResultsChangedWaiter();

  // Runs the loop until result changes for all registered types have been
  // observed.
  void Wait();

 private:
  // Callback for results changes registered with `search_controller_`.
  void OnResultsChanged(ash::AppListSearchResultType result_type);

  // Observed search controller.
  const raw_ptr<SearchController> search_controller_;

  // Set of result types that the waiter is still waiting for.
  std::set<ash::AppListSearchResultType> types_;

  // Whether the waiter is observing search controller's results changes.
  bool active_ = true;

  base::RunLoop run_loop_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_SEARCH_RESULTS_CHANGED_WAITER_H_
