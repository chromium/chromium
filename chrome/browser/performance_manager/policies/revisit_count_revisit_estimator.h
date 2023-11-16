// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_COUNT_REVISIT_ESTIMATOR_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_COUNT_REVISIT_ESTIMATOR_H_

#include <map>

#include "chrome/browser/performance_manager/policies/probability_distribution.h"
#include "components/performance_manager/user_tuning/proactive_discard_evaluator.h"

namespace performance_manager {

class RevisitCountRevisitEstimator
    : public ProactiveDiscardEvaluator::RevisitProbabilityEstimator {
 public:
  RevisitCountRevisitEstimator(
      Graph* graph,
      std::map<int64_t, ProbabilityDistribution> time_to_revisit_probabilities,
      std::map<int64_t, float> revisit_probabilities);
  ~RevisitCountRevisitEstimator() override;

  float ComputeRevisitProbability(
      const TabPageDecorator::TabHandle* tab_handle) override;

 private:
  raw_ptr<Graph> graph_;
  std::map<int64_t, ProbabilityDistribution> time_to_revisit_probabilities_;
  // The probability of a tab being revisited, given no other priors, for each
  // `num_revisits`.
  // TODO(crbug.com/1469337): Use a probability distribution based on time in
  // background here as well.
  std::map<int64_t, float> revisit_probabilities_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REVISIT_COUNT_REVISIT_ESTIMATOR_H_
