// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_MEMORY_SAVER_MODE_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_MEMORY_SAVER_MODE_POLICY_H_

#include <map>
#include <memory>

#include "base/timer/timer.h"
#include "components/performance_manager/public/decorators/tab_page_decorator.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/user_tuning/prefs.h"

namespace performance_manager::policies {

// This policy is responsible for discarding tabs after they have been
// backgrounded for a certain amount of time, when Memory Saver Mode is
// enabled by the user.
class MemorySaverModePolicy : public GraphOwned,
                                 public PageNode::ObserverDefaultImpl,
                                 public TabPageObserverDefaultImpl {
 public:
  MemorySaverModePolicy();
  ~MemorySaverModePolicy() override;

  static MemorySaverModePolicy* GetInstance();

  // PageNode::ObserverDefaultImpl:
  void OnIsVisibleChanged(const PageNode* page_node) override;

  // TabPageObserverDefaultImpl:
  void OnTabAdded(TabPageDecorator::TabHandle* tab_handle) override;
  void OnBeforeTabRemoved(TabPageDecorator::TabHandle* tab_handle) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  void OnMemorySaverModeChanged(bool enabled);
  base::TimeDelta GetTimeBeforeDiscardForTesting() const;
  void SetMode(user_tuning::prefs::MemorySaverModeAggressiveness mode);

  // Returns true if Memory Saver mode is enabled, false otherwise. Useful to
  // get the state of the mode from the Performance Manager sequence.
  bool IsMemorySaverDiscardingEnabled() const;

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
  user_tuning::prefs::MemorySaverModeAggressiveness mode_ =
      user_tuning::prefs::MemorySaverModeAggressiveness::kMedium;
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_MEMORY_SAVER_MODE_POLICY_H_
