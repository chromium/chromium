// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/high_pmf_discard_policy.h"

#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_metrics.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if !BUILDFLAG(IS_LINUX)
#include "base/memory/memory_pressure_monitor.h"
#endif

namespace performance_manager {
namespace policies {

namespace {

// The factor that will be applied to the total amount of RAM to establish the
// PMF limit.
static constexpr base::FeatureParam<double> kRAMRatioPMFLimitFactor{
    &performance_manager::features::kHighPMFDiscardPolicy,
    "RAMRatioPMFLimitFactor", 1.5};

// The discard strategy to use.
static constexpr base::FeatureParam<int> kDiscardStrategy{
    &performance_manager::features::kHighPMFDiscardPolicy, "DiscardStrategy",
    static_cast<int>(features::DiscardStrategy::LRU)};

}  // namespace

HighPMFDiscardPolicy::HighPMFDiscardPolicy() = default;
HighPMFDiscardPolicy::~HighPMFDiscardPolicy() = default;

void HighPMFDiscardPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddSystemNodeObserver(this);
  graph_ = graph;

  metrics_interest_token_ = performance_manager::ProcessMetricsDecorator::
      RegisterInterestForProcessMetrics(graph_);

  base::SystemMemoryInfoKB mem_info = {};
  if (base::GetSystemMemoryInfo(&mem_info))
    pmf_limit_kb_ = mem_info.total * kRAMRatioPMFLimitFactor.Get();

  DCHECK(PageDiscardingHelper::GetFromGraph(graph_))
      << "A PageDiscardingHelper instance should be registered against the "
         "graph in order to use this policy.";
}

void HighPMFDiscardPolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveSystemNodeObserver(this);
  metrics_interest_token_.reset();
  graph_ = nullptr;
  pmf_limit_kb_ = kInvalidPMFLimitValue;
}

void HighPMFDiscardPolicy::OnProcessMemoryMetricsAvailable(
    const SystemNode* unused) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pmf_limit_kb_ == kInvalidPMFLimitValue)
    return;

  if (discard_attempt_in_progress_)
    return;

  int total_pmf_kb = 0;
  bool should_discard = false;
  auto process_nodes = graph_->GetAllProcessNodes();
  for (const auto* node : process_nodes) {
    total_pmf_kb += node->GetPrivateFootprintKb();
    if (total_pmf_kb >= pmf_limit_kb_) {
      should_discard = true;
      // Do not break so we can compute the total PMF and report it later.
    }
  }

  // Report metrics when the high-PMF session has ended or after 100 attempts
  // (the maximum value for the histograms).
  if ((!should_discard && discard_attempts_count_while_pmf_is_high_) ||
      discard_attempts_count_while_pmf_is_high_ == 100) {
    base::UmaHistogramCounts100("Discarding.HighPMFPolicy.DiscardAttemptsCount",
                                discard_attempts_count_while_pmf_is_high_);
    base::UmaHistogramCounts100(
        "Discarding.HighPMFPolicy.SuccessfulDiscardsCount",
        successful_discards_count_while_pmf_is_high_);
    discard_attempts_count_while_pmf_is_high_ = 0;
    successful_discards_count_while_pmf_is_high_ = 0;
  }

  // Reports the metrics for the previous intervention if necessary.
  if (intervention_details_.has_value()) {
    base::UmaHistogramBoolean("Discarding.HighPMFPolicy.DiscardSuccess",
                              intervention_details_->a_tab_has_been_discarded);

    if (intervention_details_->a_tab_has_been_discarded) {
      // Report the total PMF delta as an integer as the total PMF might have
      // increased. Negative values (increase of memory) are reported as 0 and
      // go into the underflow bucket.
      base::UmaHistogramCustomCounts(
          "Discarding.HighPMFPolicy.MemoryReclaimedKbAfterDiscardingATab",
          std::max(0, intervention_details_->total_pmf_kb_before_intervention -
                          total_pmf_kb),
          1, 2000000 /* +2GB */, 50);
    }

    intervention_details_.reset();
  }

  if (should_discard) {
    discard_attempt_in_progress_ = true;
#if !BUILDFLAG(IS_LINUX)
    // Record the memory pressure level before discarding a tab.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce([]() {
          base::UmaHistogramEnumeration(
              "Discarding.HighPMFPolicy.MemoryPressureLevel",
              base::MemoryPressureMonitor::Get()->GetCurrentPressureLevel());
        }));
#endif
    intervention_details_.emplace(
        InterventionDetails{.total_pmf_kb_before_intervention = total_pmf_kb});
    PageDiscardingHelper::GetFromGraph(graph_)->UrgentlyDiscardAPage(
        static_cast<features::DiscardStrategy>(kDiscardStrategy.Get()),
        base::BindOnce(&HighPMFDiscardPolicy::PostDiscardAttemptCallback,
                       base::Unretained(this)));
  }
}

void HighPMFDiscardPolicy::PostDiscardAttemptCallback(bool success) {
  DCHECK(discard_attempt_in_progress_);
  DCHECK(intervention_details_.has_value());
  discard_attempt_in_progress_ = false;
  intervention_details_->a_tab_has_been_discarded = success;
  ++discard_attempts_count_while_pmf_is_high_;
  if (success)
    ++successful_discards_count_while_pmf_is_high_;
}

}  // namespace policies
}  // namespace performance_manager
