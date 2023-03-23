// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/burn_in_controller.h"

#include "base/containers/flat_set.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"

namespace app_list {

constexpr base::TimeDelta kBurnInPeriod = base::Milliseconds(200);

BurnInController::BurnInController(BurnInPeriodElapsedCallback callback)
    : burn_in_period_elapsed_callback_(std::move(callback)),
      burn_in_period_(kBurnInPeriod) {}

void BurnInController::Start() {
  burn_in_timer_.Start(FROM_HERE, burn_in_period_,
                       burn_in_period_elapsed_callback_);

  session_start_ = base::Time::Now();
  burn_in_iteration_counter_ = 0;
  ids_to_burn_in_iteration_.clear();
}

BurnInController::~BurnInController() {}

void BurnInController::Stop() {
  burn_in_timer_.Stop();
}

bool BurnInController::UpdateResults(ResultsMap& results,
                                     CategoriesList& categories,
                                     ash::AppListSearchResultType result_type) {
  // True if the burn-in period has elapsed.
  const bool is_post_burn_in =
      base::Time::Now() - session_start_ > burn_in_period_;
  if (is_post_burn_in) {
    ++burn_in_iteration_counter_;
  }

  // Record the burn-in iteration number for categories we are seeing for the
  // first time in this search.
  base::flat_set<Category> updated_categories;
  for (const auto& result : results[result_type]) {
    updated_categories.insert(result->category());
  }
  for (auto& category : categories) {
    const auto it = updated_categories.find(category.category);
    if (it != updated_categories.end() && category.burn_in_iteration == -1) {
      category.burn_in_iteration = burn_in_iteration_counter_;
    }
  }

  // Record-keeping for the burn-in iteration number of individual results.
  const auto it = results.find(result_type);
  DCHECK(it != results.end());

  for (const auto& result : it->second) {
    const std::string result_id = result->id();
    if (ids_to_burn_in_iteration_.find(result_id) !=
        ids_to_burn_in_iteration_.end()) {
      // Result has been seen before. Set burn_in_iteration, since the result
      // object has changed since last seen.
      result->scoring().set_burn_in_iteration(
          ids_to_burn_in_iteration_[result_id]);
    } else {
      result->scoring().set_burn_in_iteration(burn_in_iteration_counter_);
      ids_to_burn_in_iteration_[result_id] = burn_in_iteration_counter_;
    }
  }

  return is_post_burn_in;
}

}  // namespace app_list
