// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/best_effort_task_inhibiting_policy.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"

namespace performance_manager {

namespace policies {

using performance_scenarios::LoadingScenario;
using performance_scenarios::PerformanceScenarioObserverList;
using performance_scenarios::ScenarioScope;

BestEffortTaskInhibitingPolicy::BestEffortTaskInhibitingPolicy() {
  // Validate values provided by feature params.
  CHECK_GE(period_duration_, base::Seconds(30), base::NotFatalUntil::M145);
  CHECK_GT(period_duration_, minimum_duration_without_fence_,
           base::NotFatalUntil::M145);

  last_period_start_time_ = base::TimeTicks::Now();
  AddOrRemoveFence();
}

BestEffortTaskInhibitingPolicy::~BestEffortTaskInhibitingPolicy() = default;

void BestEffortTaskInhibitingPolicy::OnPassedToGraph(
    performance_manager::Graph* graph) {
  if (auto performance_scenario_observer_list =
          PerformanceScenarioObserverList::GetForScope(
              ScenarioScope::kGlobal)) {
    performance_scenario_observer_list->AddObserver(this);
  }
}

void BestEffortTaskInhibitingPolicy::OnTakenFromGraph(
    performance_manager::Graph* graph) {
  if (auto performance_scenario_observer_list =
          PerformanceScenarioObserverList::GetForScope(
              ScenarioScope::kGlobal)) {
    performance_scenario_observer_list->RemoveObserver(this);
  }
}

bool BestEffortTaskInhibitingPolicy::IsFenceLiveForTesting() {
  return best_effort_fence_.has_value();
}

void BestEffortTaskInhibitingPolicy::OnLoadingScenarioChanged(
    performance_scenarios::ScenarioScope scope,
    performance_scenarios::LoadingScenario old_scenario,
    performance_scenarios::LoadingScenario new_scenario) {
  switch (new_scenario) {
    case LoadingScenario::kFocusedPageLoading:
    case LoadingScenario::kVisiblePageLoading:
      loading_vote_ = true;
      break;
    case LoadingScenario::kNoPageLoading:
    case LoadingScenario::kBackgroundPageLoading:
      loading_vote_ = false;
      break;
  }

  AddOrRemoveFence();
}

void BestEffortTaskInhibitingPolicy::OnInputScenarioChanged(
    performance_scenarios::ScenarioScope scope,
    performance_scenarios::InputScenario old_scenario,
    performance_scenarios::InputScenario new_scenario) {
  input_vote_ =
      (new_scenario != performance_scenarios::InputScenario::kNoInput);
  AddOrRemoveFence();
}

void BestEffortTaskInhibitingPolicy::StartTimer(base::TimeDelta delay) {
  periodic_quota_check_timer_.Start(
      FROM_HERE, delay,
      base::BindOnce(&BestEffortTaskInhibitingPolicy::AddOrRemoveFence,
                     base::Unretained(this)));
}

void BestEffortTaskInhibitingPolicy::QuotaExceeded() {
  const base::TimeTicks now = base::TimeTicks::Now();

  // Remove existing fence if any.
  best_effort_fence_.reset();
  last_fence_start_time_ = base::TimeTicks();

  no_fence_until_time_ = now + minimum_duration_without_fence_;
  cumulative_fence_time_ = base::TimeDelta();
  last_period_start_time_ = now;
  StartTimer(minimum_duration_without_fence_);
}

void BestEffortTaskInhibitingPolicy::AddOrRemoveFence() {
  const base::TimeTicks now = base::TimeTicks::Now();

  base::TimeTicks expected_end_of_period =
      last_period_start_time_ + period_duration_;
  if (now >= expected_end_of_period) {
    // If there is no fence currently the state of this class starts fresh.
    if (!best_effort_fence_.has_value()) {
      last_period_start_time_ = now;
      cumulative_fence_time_ = base::TimeDelta();
    } else {
      // If a fence was up the time needs to be billed.
      base::TimeDelta lateness = (now - expected_end_of_period);

      // Quota already exceeded. Immediately handle this situation to avoid
      // logic problems further down the line where period lengths of fence time
      // would surpass logical thresholds.
      if (lateness > (period_duration_ - minimum_duration_without_fence_)) {
        QuotaExceeded();
        return;
      }

      // A fence was active when the period-end timer should have fired. Since
      // the timer is late, we need to bill the lateness as fence time in the
      // period that is starting now. This is done by starting it at its
      // expected time and setting the fence start time to be the same,
      // effectively carrying over the 'lateness' as consumed quota.
      last_period_start_time_ = expected_end_of_period;
      last_fence_start_time_ = last_period_start_time_;
      cumulative_fence_time_ = base::TimeDelta();
    }
  }

  if (!no_fence_until_time_.is_null()) {
    // Fences are allowed again after being disabled.
    if (now >= no_fence_until_time_) {
      no_fence_until_time_ = base::TimeTicks();
      last_period_start_time_ = now;
    }

    // There could not have been a fence installed during the exclusion time.
    CHECK(!best_effort_fence_.has_value());
    return;
  }

  base::TimeDelta total_fence_time = cumulative_fence_time_;
  if (!last_fence_start_time_.is_null()) {
    total_fence_time += (now - last_fence_start_time_);
  }

  // There has already been too much fence time.
  if (total_fence_time >=
      (period_duration_ - minimum_duration_without_fence_)) {
    QuotaExceeded();
    return;
  }

  const bool fence_needed = input_vote_ || loading_vote_;
  if (fence_needed) {
    if (!best_effort_fence_.has_value()) {
      last_fence_start_time_ = now;
      best_effort_fence_.emplace();
    }

    // Next mandatory check is when quota would run out if a fence is left up.
    StartTimer(period_duration_ - minimum_duration_without_fence_ -
               total_fence_time);
    return;
  }

  if (best_effort_fence_.has_value()) {
    base::TimeDelta fence_time = now - last_fence_start_time_;
    base::UmaHistogramTimes("BestEffortTaskInhibitingPolicy.FenceLifetime",
                            fence_time);
    cumulative_fence_time_ += fence_time;
    last_fence_start_time_ = base::TimeTicks();
    best_effort_fence_.reset();
  }
}

}  // namespace policies

}  // namespace performance_manager
