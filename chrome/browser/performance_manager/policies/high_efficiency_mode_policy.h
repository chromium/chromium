// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_EFFICIENCY_MODE_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_EFFICIENCY_MODE_POLICY_H_

#include <map>
#include <memory>

#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager::policies {

// This policy is responsible for discarding tabs after they have been
// backgrounded for a certain amount of time, when High Efficiency Mode is
// enabled by the user.
class HighEfficiencyModePolicy : public GraphOwned,
                                 public PageNode::ObserverDefaultImpl {
 public:
  HighEfficiencyModePolicy();
  ~HighEfficiencyModePolicy() override;

  static HighEfficiencyModePolicy* GetInstance();

  // PageNode::ObserverDefaultImpl:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnTypeChanged(const PageNode* page_node,
                     PageType previous_type) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  void OnHighEfficiencyModeChanged(bool enabled);

  // Returns true if High Efficiency mode is enabled, false otherwise. Useful to
  // get the state of the mode from the Performance Manager sequence.
  bool IsHighEfficiencyDiscardingEnabled() const;

 private:
  void StartDiscardTimerIfEnabled(const PageNode* page_node,
                                  base::TimeDelta time_before_discard);
  void RemoveActiveTimer(const PageNode* page_node);
  void DiscardPageTimerCallback(const PageNode* page_node);

  bool high_efficiency_mode_enabled_ = false;

  std::map<const PageNode*, base::OneShotTimer> active_discard_timers_;
  const base::TimeDelta time_before_discard_;

  raw_ptr<Graph> graph_ = nullptr;
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_EFFICIENCY_MODE_POLICY_H_
