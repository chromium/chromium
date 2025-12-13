// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_DISABLER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_DISABLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace performance_manager {

// Creates and owns a ProcessLoadingScenarioObserver for each process, in order
// to disable priority boosting after an associated page has loaded.
class PriorityBoostDisabler : public GraphOwned, public ProcessNodeObserver {
 public:
  PriorityBoostDisabler();
  ~PriorityBoostDisabler() override;

  // GraphOwned overrides
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver overrides
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;

 private:
  class ProcessLoadingScenarioObserver;

  absl::flat_hash_map<const ProcessNode*,
                      std::unique_ptr<ProcessLoadingScenarioObserver>>
      scenario_observers_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_PRIORITY_BOOST_DISABLER_H_
