// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROBABILISTIC_MEMORY_SAVER_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROBABILISTIC_MEMORY_SAVER_POLICY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/user_tuning/proactive_discard_evaluator.h"

namespace performance_manager {

class ProbabilisticMemorySaverPolicy : public GraphOwned {
 public:
  using EstimatorCreationFunc = base::RepeatingCallback<std::unique_ptr<
      ProactiveDiscardEvaluator::RevisitProbabilityEstimator>(Graph*)>;
  ProbabilisticMemorySaverPolicy(
      bool simulation_mode,
      EstimatorCreationFunc estimator_creation_function = base::BindRepeating(
          &ProbabilisticMemorySaverPolicy::CreateDefaultEstimator));
  ~ProbabilisticMemorySaverPolicy() override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  static std::unique_ptr<ProactiveDiscardEvaluator::RevisitProbabilityEstimator>
  CreateDefaultEstimator(Graph* graph);
  void OnShouldDiscard(const TabPageDecorator::TabHandle* tab_handle);

  // When true, histograms are recorded as-if tabs were discarded but the
  // discard isn't triggered.
  bool is_simulation_mode_ = true;

  std::unique_ptr<ProactiveDiscardEvaluator> evaluator_;
  raw_ptr<Graph> graph_;
  EstimatorCreationFunc estimator_creation_function_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PROBABILISTIC_MEMORY_SAVER_POLICY_H_
