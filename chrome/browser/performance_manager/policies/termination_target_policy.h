// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_TERMINATION_TARGET_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_TERMINATION_TARGET_POLICY_H_

#include "chrome/browser/performance_manager/mechanisms/termination_target_setter.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace performance_manager {

// This policy provides to partition_alloc the handle of a process to terminate
// on commit failure, before retrying the commit. In theory, the commit is much
// more likely to succeed after this, and this allows trading browser crashes
// (very bad) for unimportant renderer crashes (less bad).
//
// Implementation notes: When a page's visibility changes, a page is removed, a
// process is removed or new per-process memory metrics are available, this
// policy looks at all renderer processes to select the best "termination
// target" and provides it to partition_alloc. In order of preference, the
// termination target has:
// 1. A private memory footprint above 100MB (70th percentile), to maximize
//    chances that the termination will free enough commit space for the commit
//    retry to succeed.
// 2. Does not host non-discardable pages. For example, this reduces the chances
//    of ending a video call or loosing form entries.
// 3. Was the least recently visible.
// For simplicity of implementation, we do not observe when #2 changes (we only
// observe changes to #1 and #3). That implementation choice could be
// re-evaluated if we make "discardable" a property of `PageNode` in the future.
// For now, we rely on the fact that new per-process memory metrics are
// available every 2 minutes (and thus a change of #2 is taken into account
// within 2 minutes) for the termination target to be "mostly correct".
//
// This class is built on all platforms to facilitate development, but since a
// "commit failure" only exists on Windows, it is only instantiated on Windows
// in production.
class TerminationTargetPolicy
    : public PageNodeObserver,
      public ProcessNodeObserver,
      public SystemNodeObserver,
      public GraphOwnedAndRegistered<TerminationTargetPolicy> {
 public:
  explicit TerminationTargetPolicy(
      std::unique_ptr<TerminationTargetSetter> termination_target_setter =
          std::make_unique<TerminationTargetSetter>());
  ~TerminationTargetPolicy() override;

  // GraphOwnedAndRegistered:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNodeObserver:
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnPageNodeRemoved(const PageNode* page_node) override;

  // ProcessNodeObserver:
  void OnBeforeProcessNodeRemoved(const ProcessNode* process_node) override;
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;

  // SystemNodeObserver:
  void OnProcessMemoryMetricsAvailable(const SystemNode* system_node) override;

 private:
  // Determines the best termination target
  void UpdateTerminationTarget(
      const ProcessNode* process_being_deleted = nullptr);

  // The process currently set as a termination target in partition_alloc.
  raw_ptr<const ProcessNode> current_termination_target_ = nullptr;

  // The mechanism to set a termination target. In production, this calls
  // `partition_alloc::SetProcessToTerminateOnCommitFailure`. In tests, it can
  // be mocked.
  std::unique_ptr<TerminationTargetSetter> termination_target_setter_;
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_TERMINATION_TARGET_POLICY_H_
