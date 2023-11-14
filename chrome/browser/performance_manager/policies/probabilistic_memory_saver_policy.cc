// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/probabilistic_memory_saver_policy.h"

#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/probabilistic_memory_saver_sampler.h"
#include "chrome/browser/performance_manager/policies/revisit_count_revisit_estimator.h"
#include "chrome/browser/performance_manager/policies/revisit_probability_distributions.h"

namespace performance_manager {

ProbabilisticMemorySaverPolicy::ProbabilisticMemorySaverPolicy(
    bool is_simulation_mode,
    ProbabilisticMemorySaverPolicy::EstimatorCreationFunc
        estimator_creation_function)
    : is_simulation_mode_(is_simulation_mode),
      estimator_creation_function_(estimator_creation_function) {}

ProbabilisticMemorySaverPolicy::~ProbabilisticMemorySaverPolicy() = default;

void ProbabilisticMemorySaverPolicy::OnPassedToGraph(Graph* graph) {
  graph_ = graph;
  // Unretained is safe because this owns the evaluator for the latter's entire
  // lifetime.
  evaluator_ = std::make_unique<ProactiveDiscardEvaluator>(
      estimator_creation_function_.Run(graph_),
      std::make_unique<ProbabilisticMemorySaverSampler>(graph_),
      base::BindRepeating(&ProbabilisticMemorySaverPolicy::OnShouldDiscard,
                          base::Unretained(this)));
}

void ProbabilisticMemorySaverPolicy::OnTakenFromGraph(Graph* graph) {
  evaluator_.reset();
  graph_ = nullptr;
}

// static
std::unique_ptr<ProactiveDiscardEvaluator::RevisitProbabilityEstimator>
ProbabilisticMemorySaverPolicy::CreateDefaultEstimator(Graph* graph) {
  return std::make_unique<RevisitCountRevisitEstimator>(
      graph, CreatePerRevisitCountTimeToRevisitCdfs(),
      CreatePerRevisitCountRevisitProbability());
}

void ProbabilisticMemorySaverPolicy::OnShouldDiscard(
    const TabPageDecorator::TabHandle* tab_handle) {
  CHECK(graph_);
  CHECK(tab_handle);
  if (!is_simulation_mode_) {
    policies::PageDiscardingHelper::GetFromGraph(graph_)
        ->ImmediatelyDiscardSpecificPage(
            tab_handle->page_node(),
            policies::PageDiscardingHelper::DiscardReason::PROACTIVE);
  }
}

}  // namespace performance_manager
