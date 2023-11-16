// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/revisit_count_revisit_estimator.h"

#include <algorithm>

#include "components/performance_manager/public/user_tuning/tab_revisit_tracker.h"

namespace performance_manager {

RevisitCountRevisitEstimator::RevisitCountRevisitEstimator(
    Graph* graph,
    std::map<int64_t, ProbabilityDistribution> time_to_revisit_probabilities,
    std::map<int64_t, float> revisit_probabilities)
    : graph_(graph),
      time_to_revisit_probabilities_(std::move(time_to_revisit_probabilities)),
      revisit_probabilities_(std::move(revisit_probabilities)) {}

RevisitCountRevisitEstimator::~RevisitCountRevisitEstimator() = default;

float RevisitCountRevisitEstimator::ComputeRevisitProbability(
    const TabPageDecorator::TabHandle* tab_handle) {
  TabRevisitTracker* revisit_tracker =
      graph_->GetRegisteredObjectAs<TabRevisitTracker>();
  CHECK(revisit_tracker);

  TabRevisitTracker::StateBundle state =
      revisit_tracker->GetStateForTabHandle(tab_handle);

  // Cap at `kMaxNumRevisit - 1`. Because of the way the data is recorded, we
  // end up with a CDF for each num_revisit in the range [0, kMaxNumRevisit[
  auto time_to_revisit_it = time_to_revisit_probabilities_.find(
      std::min(state.num_revisits, TabRevisitTracker::kMaxNumRevisit - 1));

  if (time_to_revisit_it == time_to_revisit_probabilities_.end()) {
    return 1.0f;
  }

  auto revisit_it = revisit_probabilities_.find(
      std::min(state.num_revisits, TabRevisitTracker::kMaxNumRevisit - 1));

  if (revisit_it == revisit_probabilities_.end()) {
    return 1.0f;
  }

  absl::optional<base::TimeTicks> last_active_time =
      state.state == TabRevisitTracker::State::kBackground
          ? state.last_active_time
          : absl::nullopt;
  if (!last_active_time) {
    return 1.0f;
  }
  // The actual probability of being revisited in the interval [t0, t1] is the
  // probability of being revisited at all AND that revisit taking place between
  // t0 and t1.
  float prob_revisit_in_interval_given_revisited =
      time_to_revisit_it->second.GetProbability(base::Hours(24).InSeconds()) -
      time_to_revisit_it->second.GetProbability(
          (base::TimeTicks::Now() - *last_active_time).InSeconds());

  return prob_revisit_in_interval_given_revisited * revisit_it->second;
}

}  // namespace performance_manager
