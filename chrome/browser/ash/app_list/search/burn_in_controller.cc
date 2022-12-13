// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/burn_in_controller.h"

#include "base/containers/flat_set.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

namespace app_list {

constexpr base::TimeDelta kBurnInPeriod = base::Milliseconds(200);

BurnInController::BurnInController(BurnInPeriodElapsedCallback callback)
    : burnin_period_elapsed_callback_(std::move(callback)),
      burnin_period_(kBurnInPeriod) {}

bool BurnInController::is_post_burnin() {
  return base::Time::Now() - session_start_ > burnin_period_;
}

void BurnInController::Start() {
  burnin_timer_.Start(FROM_HERE, burnin_period_,
                      burnin_period_elapsed_callback_);

  session_start_ = base::Time::Now();
  burnin_iteration_counter_ = 0;
  ids_to_burnin_iteration_.clear();
}

BurnInController::~BurnInController() {}

void BurnInController::Stop() {
  burnin_timer_.Stop();
}

void BurnInController::UpdateResults(ResultsMap& results,
                                     CategoriesList& categories,
                                     ash::AppListSearchResultType result_type) {
  if (is_post_burnin())
    ++burnin_iteration_counter_;

  // Record the burn-in iteration number for categories we are seeing for the
  // first time in this search.
  base::flat_set<Category> updated_categories;
  for (const auto& result : results[result_type])
    updated_categories.insert(result->category());
  for (auto& category : categories) {
    const auto it = updated_categories.find(category.category);
    if (it != updated_categories.end() && category.burnin_iteration == -1)
      category.burnin_iteration = burnin_iteration_counter_;
  }

  // Record-keeping for the burn-in iteration number of individual results.
  const auto it = results.find(result_type);
  DCHECK(it != results.end());

  for (const auto& result : it->second) {
    const std::string result_id = result->id();
    if (ids_to_burnin_iteration_.find(result_id) !=
        ids_to_burnin_iteration_.end()) {
      // Result has been seen before. Set burnin_iteration, since the result
      // object has changed since last seen.
      result->scoring().burnin_iteration = ids_to_burnin_iteration_[result_id];
    } else {
      result->scoring().burnin_iteration = burnin_iteration_counter_;
      ids_to_burnin_iteration_[result_id] = burnin_iteration_counter_;
    }
  }
}

}  // namespace app_list
