// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_BROWSER_NETWORK_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_BROWSER_NETWORK_POLICY_H_

#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"

namespace performance_manager::policies {

// Disables priority boosting for all processes except the browser and network
// processes.
class PriorityBoostBrowserNetworkPolicy : public GraphOwned,
                                          public ProcessNodeObserver {
 public:
  PriorityBoostBrowserNetworkPolicy();
  ~PriorityBoostBrowserNetworkPolicy() override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;

 private:
  void ApplyPolicy(const ProcessNode* process_node);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_BROWSER_NETWORK_POLICY_H_
