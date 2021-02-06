// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_PMF_DISCARD_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_PMF_DISCARD_POLICY_H_

#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace performance_manager {

class Graph;

namespace policies {

// The HighPMFDiscardPolicy will discard tabs when Chrome's total PMF exceeds a
// given threshold.
class HighPMFDiscardPolicy : public GraphOwned,
                             public SystemNode::ObserverDefaultImpl {
 public:
  HighPMFDiscardPolicy();
  ~HighPMFDiscardPolicy() override;
  HighPMFDiscardPolicy(const HighPMFDiscardPolicy& other) = delete;
  HighPMFDiscardPolicy& operator=(const HighPMFDiscardPolicy&) = delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // SystemNode::ObserverDefaultImpl:
  void OnProcessMemoryMetricsAvailable(const SystemNode* system_node) override;

  void set_pmf_limit_for_testing(int pmf_limit_kb) {
    pmf_limit_kb_ = pmf_limit_kb;
  }

 private:
  // Callback called when a discard attempt has completed.
  void PostDiscardAttemptCallback(bool success);

  const int kInvalidPMFLimitValue = 0;

  int pmf_limit_kb_ = kInvalidPMFLimitValue;
  Graph* graph_ = nullptr;

  // Indicates whether or not there's a discard attempt in progress. This could
  // happen if this attempt doesn't complete between 2 calls to
  // OnProcessMemoryMetricsAvailable.
  bool discard_attempt_in_progress_ = false;

  // This will contain some details about the current intervention, or will be
  // nullopt if there's no intervention in progress.
  struct InterventionDetails {
    int total_pmf_kb_before_intervention = 0;
    bool a_tab_has_been_discarded = false;
  };
  base::Optional<InterventionDetails> intervention_details_;

  size_t discard_attempts_count_while_pmf_is_high_ = 0;
  size_t successful_discards_count_while_pmf_is_high_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_PMF_DISCARD_POLICY_H_
