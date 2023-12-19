// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_EFFICIENCY_MODE_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_EFFICIENCY_MODE_POLICY_H_

#include <map>
#include <memory>

#include "base/timer/timer.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager::policies {

// This policy is responsible for discarding tabs after they have been
// backgrounded for a certain amount of time, when High Efficiency Mode is
// enabled by the user.
class HighEfficiencyModePolicy : public GraphOwned,
                                 public PageNode::ObserverDefaultImpl,
                                 public TabPageObserverDefaultImpl {
 public:
  enum class MemorySaverMode {
    kUserSpecified = 0,  // The user has selected the time value
    kConservative,
    kMedium,
    kAggressive,
    kMaxValue = kAggressive,
  };

  HighEfficiencyModePolicy();
  ~HighEfficiencyModePolicy() override;

  static HighEfficiencyModePolicy* GetInstance();

  // PageNode::ObserverDefaultImpl:
  void OnIsVisibleChanged(const PageNode* page_node) override;

  // TabPageObserverDefaultImpl:
  void OnTabAdded(TabPageDecorator::TabHandle* tab_handle) override;
  void OnBeforeTabRemoved(TabPageDecorator::TabHandle* tab_handle) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  void OnHighEfficiencyModeChanged(bool enabled);
  base::TimeDelta GetTimeBeforeDiscardForTesting() const;
  void SetTimeBeforeDiscard(base::TimeDelta time_before_discard);

  // Returns true if High Efficiency mode is enabled, false otherwise. Useful to
  // get the state of the mode from the Performance Manager sequence.
  bool IsHighEfficiencyDiscardingEnabled() const;

 private:
  void StartAllDiscardTimers();
  void StartDiscardTimerIfEnabled(const TabPageDecorator::TabHandle* tab_handle,
                                  base::TimeDelta time_before_discard);
  void RemoveActiveTimer(const TabPageDecorator::TabHandle* tab_handle);
  void DiscardPageTimerCallback(const TabPageDecorator::TabHandle* tab_handle,
                                base::LiveTicks posted_at,
                                base::TimeDelta requested_time_before_discard);

  base::TimeDelta GetTimeBeforeDiscardForCurrentMode() const;
  int GetMaxNumRevisitsForCurrentMode() const;

  bool high_efficiency_mode_enabled_ = false;

  std::map<const TabPageDecorator::TabHandle*, base::OneShotTimer>
      active_discard_timers_;
  base::TimeDelta time_before_discard_;

  raw_ptr<Graph> graph_ = nullptr;
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_HIGH_EFFICIENCY_MODE_POLICY_H_
