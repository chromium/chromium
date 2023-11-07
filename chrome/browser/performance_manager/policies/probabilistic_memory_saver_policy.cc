// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/probabilistic_memory_saver_policy.h"

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/probabilistic_memory_saver_sampler.h"
#include "chrome/browser/performance_manager/policies/revisit_count_revisit_estimator.h"

namespace performance_manager {

ProbabilisticMemorySaverPolicy::ProbabilisticMemorySaverPolicy(
    ProbabilisticMemorySaverPolicy::EstimatorCreationFunc
        estimator_creation_function)
    : estimator_creation_function_(estimator_creation_function) {}

ProbabilisticMemorySaverPolicy::~ProbabilisticMemorySaverPolicy() = default;

void ProbabilisticMemorySaverPolicy::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  // Unretained is safe because this owns the evaluator for the latter's entire
  // lifetime.
  evaluator_ = std::make_unique<ProactiveDiscardEvaluator>(
      estimator_creation_function_.Run(),
      std::make_unique<ProbabilisticMemorySaverSampler>(graph),
      base::BindRepeating(&ProbabilisticMemorySaverPolicy::OnShouldDiscard,
                          base::Unretained(this)));
}

void ProbabilisticMemorySaverPolicy::OnTakenFromGraph(Graph* graph) {
  evaluator_.reset();
  graph_ = nullptr;
}

// static
std::unique_ptr<ProactiveDiscardEvaluator::RevisitProbabilityEstimator>
ProbabilisticMemorySaverPolicy::CreateDefaultEstimator() {
  return std::make_unique<RevisitCountRevisitEstimator>();
}

void ProbabilisticMemorySaverPolicy::OnShouldDiscard(
    const TabPageDecorator::TabHandle* tab_handle) {
  CHECK(graph_);
  CHECK(tab_handle);
  policies::PageDiscardingHelper::GetFromGraph(graph_)
      ->ImmediatelyDiscardSpecificPage(
          tab_handle->page_node(),
          policies::PageDiscardingHelper::DiscardReason::PROACTIVE);
}

}  // namespace performance_manager
