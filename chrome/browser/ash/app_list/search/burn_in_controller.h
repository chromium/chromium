// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_BURN_IN_CONTROLLER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_BURN_IN_CONTROLLER_H_

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

// Manages operations related to the burn-in period. Owned by the
// SearchController.
class BurnInController {
 public:
  using BurnInPeriodElapsedCallback = base::RepeatingCallback<void()>;

  explicit BurnInController(BurnInPeriodElapsedCallback);
  ~BurnInController();

  BurnInController(const BurnInController&) = delete;
  BurnInController& operator=(const BurnInController&) = delete;

  // Called at the beginning of a query search. Initiates the burn-in/ period.
  void Start();

  // Called to stop/interrupt a previous call to Start().
  //
  // Stops the burn-in timer, thereby preventing any now unneeded calls to
  // BurnInPeriodElapsedCallback. Currently, the only use case for this is when
  // a query search is succeeded by a zero-state search.
  void Stop();

  // Called when new result information has become available.
  //
  // Performs house-keeping related to burn-in iteration numbers for categories
  // and individual results. These are later important for sorting purposes in
  // the SearchController - see further documentation below.
  //
  // Triggers the BurnInPeriodElapsedCallback if it is the first time
  // UpdateResults() has been called since the burn-in period has elapsed.
  //
  // Returns true if results are updated before `burn_in_period_`, and false
  // otherwise.
  bool UpdateResults(ResultsMap& results,
                     CategoriesList& categories,
                     ash::AppListSearchResultType result_type);

  const base::flat_map<std::string, int>& ids_to_burn_in_iteration_for_test() {
    return ids_to_burn_in_iteration_;
  }

 private:
  // The time when Start was most recently called.
  base::Time session_start_;

  // Called when the burn-in period has elapsed.
  BurnInPeriodElapsedCallback burn_in_period_elapsed_callback_;

  // The period of time ("burn-in") to wait before publishing a first collection
  // of search results to the model updater.
  const base::TimeDelta burn_in_period_;

  // A timer for the burn-in period. During the burn-in period, results are
  // collected from search providers. Publication of results to the model
  // updater is delayed until the burn-in period has elapsed.
  base::RetainingOneShotTimer burn_in_timer_;

  // Counter for burn-in iterations. Useful for query search only.
  //
  // Zero signifies pre-burn-in state. After burn-in period has elapsed, counter
  // is incremented by one each time SetResults() is called. This burn-in
  // iteration number is used for individual results as well as overall
  // categories.
  //
  // This information is useful because it allows for:
  //
  // (1) Results and categories to be ranked by different rules depending on
  // whether the information arrived pre- or post-burn-in.
  // (2) Sorting stability across multiple post-burn-in updates.
  int burn_in_iteration_counter_ = 0;

  // Store the ID of each result we encounter in a given query, along with the
  // burn-in iteration during which it arrived. This storage is necessary
  // because:
  //
  // Some providers may return more than once, and on each successive return,
  // the previous results are swapped for new ones within SetResults(). Result
  // meta-information we wish to persist across multiple calls to SetResults
  // must therefore be stored separately.
  base::flat_map<std::string, int> ids_to_burn_in_iteration_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_BURN_IN_CONTROLLER_H_
