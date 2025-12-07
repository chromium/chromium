// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_BEST_EFFORT_TASK_INHIBITING_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_BEST_EFFORT_TASK_INHIBITING_POLICY_H_

#include <optional>

#include "base/task/execution_fence.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"

namespace performance_manager {

namespace policies {

// Policy that disables running of best-effort tasks when the browser is
// actively being used (e.g. during page loads or user input). To prevent
// task starvation, this policy ensures that the fence is disabled for at least
// `minimum_duration_without_fence` within any `period_duration_` window.
class BestEffortTaskInhibitingPolicy
    : public GraphOwned,
      public performance_scenarios::PerformanceScenarioObserver {
 public:
  BestEffortTaskInhibitingPolicy();
  ~BestEffortTaskInhibitingPolicy() override;

  // performance_scenarios::PerformanceScenarioObserver:
  void OnLoadingScenarioChanged(
      performance_scenarios::ScenarioScope scope,
      performance_scenarios::LoadingScenario old_scenario,
      performance_scenarios::LoadingScenario new_scenario) override;
  void OnInputScenarioChanged(
      performance_scenarios::ScenarioScope scope,
      performance_scenarios::InputScenario old_scenario,
      performance_scenarios::InputScenario new_scenario) override;

  // performance_manager::GraphOwned:
  void OnPassedToGraph(performance_manager::Graph* graph) override;
  void OnTakenFromGraph(performance_manager::Graph* graph) override;

  [[nodiscard]] bool IsFenceLiveForTesting();

  base::TimeDelta period_duration() const { return period_duration_; }

  base::TimeDelta minimum_duration_without_fence() const {
    return minimum_duration_without_fence_;
  }

 private:
  // Schedule the next verification for fence time quota.
  void StartTimer(base::TimeDelta delay);

  // Installs or removes a fence depending on the current scenario provided info
  // and the quota of fence time.
  void AddOrRemoveFence();

  // Handles internal state and scheduling decision when going over quota.
  void QuotaExceeded();

  // True if there is ongoing user input that should inhibit best-effort tasks.
  bool input_vote_ = false;
  // True if there is an ongoing load that should inhibit best-effort tasks.
  bool loading_vote_ = false;

  // The timestamp when the current best-effort fence was installed. This is
  // reset when the fence is removed.
  base::TimeTicks last_fence_start_time_;

  // The start time of current best-effort fence quota evaluation period.
  base::TimeTicks last_period_start_time_;

  // The total time a best-effort fence has been active during the current
  // evaluation period. Does not include the duration of a currently active
  // fence.
  base::TimeDelta cumulative_fence_time_;

  // If non-null, this is the timestamp until which no best-effort fences can
  // be installed.
  base::TimeTicks no_fence_until_time_;

  base::OneShotTimer periodic_quota_check_timer_;
  std::optional<base::ScopedBestEffortExecutionFence> best_effort_fence_;

  // Time during which to enable running best effort tasks again when they have
  // been disabled for too long.
  base::TimeDelta minimum_duration_without_fence_ =
      features::kBestEffortTaskInhibitingMinimumAllowedTimePerPeriod.Get();

  // Time over which at least `minimum_duration_without_fence_' must elapse
  // without a fence being live.
  base::TimeDelta period_duration_ =
      features::kBestEffortTaskInhibitingPeriod.Get();
};

}  // namespace policies

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_BEST_EFFORT_TASK_INHIBITING_POLICY_H_
