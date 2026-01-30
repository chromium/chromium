// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_FOREGROUND_BROWSER_NETWORK_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_FOREGROUND_BROWSER_NETWORK_POLICY_H_

#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager::policies {

// Disables priority boosting for all processes except foreground renderers, the
// browser and network processes. Foreground for this policy means the process
// priority is greater than kBestEffort.
class PriorityBoostForegroundBrowserNetworkPolicy : public GraphOwned,
                                                    public ProcessNodeObserver {
 public:
  PriorityBoostForegroundBrowserNetworkPolicy();
  ~PriorityBoostForegroundBrowserNetworkPolicy() override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnPriorityChanged(const ProcessNode* process_node,
                         base::Process::Priority previous_value) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

 private:
  // Enable or disable boosting according to process type, as soon as the
  // process is added or becomes valid.
  void ApplyStartupPolicy(const ProcessNode* process_node);
  // Enable boosting for foreground renderers (priority is greater than
  // BestEffort). Disable it for background renderers(priority is BestEffort).
  void ApplyRendererPriorityPolicy(const ProcessNode* process_node);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_FOREGROUND_BROWSER_NETWORK_POLICY_H_
