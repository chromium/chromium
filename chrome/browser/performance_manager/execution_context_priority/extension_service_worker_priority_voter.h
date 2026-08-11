// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXTENSION_SERVICE_WORKER_PRIORITY_VOTER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXTENSION_SERVICE_WORKER_PRIORITY_VOTER_H_

#include "components/performance_manager/public/execution_context_priority/priority_voting_system.h"
#include "components/performance_manager/public/graph/worker_node.h"

namespace performance_manager::execution_context_priority {

// Votes USER_BLOCKING for extension service workers that can block navigations
// and network requests, so that their renderer process is never left at
// background priority (which maps to EcoQoS on Windows).
//
// This is a narrowly scoped alternative to
// `performance_manager::execution_context_priority::
// ExtensionServiceWorkerVoter`, which boosts *every* extension service worker
// and was disabled by default after it regressed performance metrics
// (crbug.com/493556675). Only extensions holding the `webRequestBlocking`
// permission are boosted here: those workers synchronously gate navigations, so
// starving them stalls the browsing session (crbug.com/484218883). Note that
// MV3 only grants `webRequestBlocking` to policy-installed extensions, which
// keeps the boost limited to the enterprise force-install case that motivated
// it.
//
// This voter lives in //chrome rather than //components/performance_manager
// because deciding whether to boost requires querying extension permissions,
// and //components/performance_manager cannot depend on //extensions.
class ExtensionServiceWorkerPriorityVoter : public PriorityVoter,
                                            public WorkerNodeObserver {
 public:
  static const char kPriorityReason[];

  ExtensionServiceWorkerPriorityVoter();
  ~ExtensionServiceWorkerPriorityVoter() override;

  ExtensionServiceWorkerPriorityVoter(
      const ExtensionServiceWorkerPriorityVoter&) = delete;
  ExtensionServiceWorkerPriorityVoter& operator=(
      const ExtensionServiceWorkerPriorityVoter&) = delete;

  // PriorityVoter:
  void InitializeOnGraph(Graph* graph, VotingChannel voting_channel) override;
  void TearDownOnGraph(Graph* graph) override;

  // WorkerNodeObserver:
  void OnBeforeWorkerNodeAdded(
      const WorkerNode* worker_node,
      const ProcessNode* pending_process_node) override;
  void OnBeforeWorkerNodeRemoved(const WorkerNode* worker_node) override;

  VoterId voter_id() const { return voting_channel_.voter_id(); }

 private:
  // The voting channel where votes are submitted.
  VotingChannel voting_channel_;
};

}  // namespace performance_manager::execution_context_priority

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_EXECUTION_CONTEXT_PRIORITY_EXTENSION_SERVICE_WORKER_PRIORITY_VOTER_H_
