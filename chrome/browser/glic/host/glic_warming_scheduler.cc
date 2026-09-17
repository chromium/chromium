// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_warming_scheduler.h"

#include <optional>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace glic {

namespace {
// Delays equal to or exceeding this threshold are treated as an explicit
// backstop to turn off warming completely (used by field trials/killswitches).
constexpr base::TimeDelta kDelayTooLong = base::Days(7);

// LINT.IfChange(GlicWarmingSchedulerSchedulingMethod)
enum class SchedulingMethod {
  kFixedDelay = 0,
  kPerformanceManagerImmediateIdle = 1,
  kPerformanceManagerObserved = 2,
  kPerformanceManagerMissingObserverListFallback = 3,
  kMaxValue = kPerformanceManagerMissingObserverListFallback,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicWarmingSchedulerSchedulingMethod)
}  // namespace

class GlicWarmingScheduler::Metrics {
 public:
  void OnScheduled(SchedulingMethod method) {
    scheduled_time_ = base::TimeTicks::Now();
    base::UmaHistogramEnumeration("Glic.WarmingScheduler.SchedulingMethod",
                                  method);
    if (method == SchedulingMethod::kPerformanceManagerImmediateIdle) {
      OnIdleReached();
    }
  }

  void OnIdleReached() {
    if (scheduled_time_.has_value()) {
      base::UmaHistogramMediumTimes("Glic.WarmingScheduler.TimeToIdle",
                                    base::TimeTicks::Now() - *scheduled_time_);
      scheduled_time_.reset();
    }
  }

  void Reset() { scheduled_time_.reset(); }

 private:
  std::optional<base::TimeTicks> scheduled_time_;
};

GlicWarmingScheduler::GlicWarmingScheduler()
    : GlicWarmingScheduler(Options()) {}

GlicWarmingScheduler::GlicWarmingScheduler(Options options)
    : performance_scenarios::MatchingScenarioObserver(options.scenario_pattern),
      options_(options),
      metrics_(std::make_unique<Metrics>()) {
  timer_.SetTaskRunner(
      content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT}));
}

GlicWarmingScheduler::~GlicWarmingScheduler() = default;

void GlicWarmingScheduler::Schedule(base::OnceClosure task) {
  Cancel();

  if (options_.delay >= kDelayTooLong) {
    return;
  }

  pending_task_ = std::move(task);

  if (options_.use_performance_manager) {
    if (ScheduleWithPerformanceManager()) {
      return;
    }
    // Performance Manager observer list was unavailable for this scope (e.g.,
    // Performance Manager is uninitialized). Fall through to schedule on the
    // delay timer as a safety fallback.
  } else {
    metrics_->OnScheduled(SchedulingMethod::kFixedDelay);
  }

  ScheduleTimer();
}

bool GlicWarmingScheduler::ScheduleWithPerformanceManager() {
  auto observer_list =
      performance_scenarios::PerformanceScenarioObserverList::GetForScope(
          options_.scope);
  if (!observer_list) {
    metrics_->OnScheduled(
        SchedulingMethod::kPerformanceManagerMissingObserverListFallback);
    return false;
  }

  if (performance_scenarios::CurrentScenariosMatch(options_.scope,
                                                   options_.scenario_pattern)) {
    // The browser is already idle. Post task to execute asynchronously
    // without re-entrancy into the caller.
    metrics_->OnScheduled(SchedulingMethod::kPerformanceManagerImmediateIdle);
    TriggerAsynchronously();
  } else {
    metrics_->OnScheduled(SchedulingMethod::kPerformanceManagerObserved);
    observation_.Observe(observer_list.get());
  }
  return true;
}

void GlicWarmingScheduler::ScheduleTimer() {
  if (options_.delay.is_zero()) {
    TriggerAsynchronously();
  } else {
    timer_.Start(
        FROM_HERE, options_.delay,
        base::BindOnce(&GlicWarmingScheduler::Trigger, base::Unretained(this)));
  }
}

void GlicWarmingScheduler::TriggerAsynchronously() {
  content::GetUIThreadTaskRunner({base::TaskPriority::BEST_EFFORT})
      ->PostTask(FROM_HERE, base::BindOnce(&GlicWarmingScheduler::Trigger,
                                           weak_ptr_factory_.GetWeakPtr()));
}

void GlicWarmingScheduler::Cancel() {
  observation_.Reset();
  timer_.Stop();
  pending_task_.Reset();
  metrics_->Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

bool GlicWarmingScheduler::IsScheduled() const {
  return !pending_task_.is_null();
}

void GlicWarmingScheduler::OnScenarioMatchChanged(
    performance_scenarios::ScenarioScope scope,
    bool matches_pattern) {
  if (matches_pattern) {
    metrics_->OnIdleReached();
    Trigger();
  }
}

void GlicWarmingScheduler::Trigger() {
  observation_.Reset();
  timer_.Stop();
  metrics_->Reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  if (pending_task_) {
    std::move(pending_task_).Run();
  }
}

}  // namespace glic
