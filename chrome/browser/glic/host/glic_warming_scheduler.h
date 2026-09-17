// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WARMING_SCHEDULER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WARMING_SCHEDULER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"

namespace glic {

// A generalized scheduler for warming operations (e.g. backfill warming or
// cold startup warming). It supports:
// 1. Performance Manager scenario detection to trigger warming when the
//    browser becomes idle (falling back to a fixed delay timer only if
//    Performance Manager observer list is unavailable).
// 2. Fixed-delay timer mode.
class GlicWarmingScheduler
    : public performance_scenarios::MatchingScenarioObserver {
 public:
  struct Options {
    // When true, monitors Performance Manager scenarios to trigger warming
    // as soon as the browser becomes idle.
    bool use_performance_manager = false;

    // Pattern to match when `use_performance_manager` is true.
    performance_scenarios::ScenarioPattern scenario_pattern =
        performance_scenarios::kDefaultIdleScenarios;

    // Delay duration in fixed-delay mode, or fallback delay if Performance
    // Manager observer list is unavailable. If set to an excessively large
    // value (>= 7 days, defined by kDelayTooLong), warming is disabled entirely
    // and Schedule() will not run any tasks in either mode.
    base::TimeDelta delay = base::Seconds(20);

    // Scenario scope to observe (defaults to kGlobal).
    performance_scenarios::ScenarioScope scope =
        performance_scenarios::ScenarioScope::kGlobal;
  };

  GlicWarmingScheduler();
  explicit GlicWarmingScheduler(Options options);
  ~GlicWarmingScheduler() override;

  GlicWarmingScheduler(const GlicWarmingScheduler&) = delete;
  GlicWarmingScheduler& operator=(const GlicWarmingScheduler&) = delete;

  // Schedules `task` to be run when the configured conditions are met.
  // If `options.delay` is >= kDelayTooLong (7 days), warming is considered
  // disabled and no task will be scheduled. Any previously scheduled task is
  // canceled.
  void Schedule(base::OnceClosure task);

  // Cancels any currently pending warming task and stops observers/timers.
  void Cancel();

  // Returns true if a warming task is scheduled and pending execution.
  bool IsScheduled() const;

  // MatchingScenarioObserver:
  void OnScenarioMatchChanged(performance_scenarios::ScenarioScope scope,
                              bool matches_pattern) override;

  base::OneShotTimer& GetTimerForTesting() { return timer_; }
  const Options& options_for_testing() const { return options_; }

 private:
  class Metrics;

  bool ScheduleWithPerformanceManager();
  void ScheduleTimer();
  void TriggerAsynchronously();
  void Trigger();

  const Options options_;
  base::OnceClosure pending_task_;
  base::OneShotTimer timer_;
  base::ScopedObservation<
      performance_scenarios::PerformanceScenarioObserverList,
      performance_scenarios::MatchingScenarioObserver>
      observation_{this};
  std::unique_ptr<Metrics> metrics_;
  base::WeakPtrFactory<GlicWarmingScheduler> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WARMING_SCHEDULER_H_
