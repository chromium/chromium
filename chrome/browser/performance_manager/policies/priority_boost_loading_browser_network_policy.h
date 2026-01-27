// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_LOADING_BROWSER_NETWORK_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_LOADING_BROWSER_NETWORK_POLICY_H_

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace performance_manager::policies {

// This policy is used to disable priority boosting for all processes except
// for the browser process, renderers that are currently loading, and for the
// network process if at least one renderer is currently loading.
class PriorityBoostLoadingBrowserNetworkPolicy : public GraphOwned,
                                                 public ProcessNodeObserver {
 public:
  PriorityBoostLoadingBrowserNetworkPolicy();
  ~PriorityBoostLoadingBrowserNetworkPolicy() override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver:
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;

  void OnRendererStartedLoading();
  void OnRendererStoppedLoading();

 private:
  void AddLoadingObserver(const ProcessNode* process_node);
  void ApplyPolicy(const ProcessNode* process_node);

  class ProcessLoadingScenarioObserver;

  absl::flat_hash_map<const ProcessNode*,
                      std::unique_ptr<ProcessLoadingScenarioObserver>>
      scenario_observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  // A non-owning pointer to the network process node.
  raw_ptr<const ProcessNode> network_process_node_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // Tracks the number of renderer processes that are currently in a loading
  // scenario.
  int loading_renderers_count_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_LOADING_BROWSER_NETWORK_POLICY_H_
