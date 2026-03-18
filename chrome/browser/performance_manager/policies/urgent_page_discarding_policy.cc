// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"

#include <memory>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/components/memory/pressure/system_memory_pressure_evaluator.h"
#endif

namespace performance_manager::policies {

namespace {

bool g_disabled_for_testing = false;

#if BUILDFLAG(IS_CHROMEOS)
std::optional<memory_pressure::ReclaimTarget> GetReclaimTarget() {
  std::optional<memory_pressure::ReclaimTarget> reclaim_target = std::nullopt;
  auto* evaluator = ash::memory::SystemMemoryPressureEvaluator::Get();
  if (evaluator) {
    reclaim_target = evaluator->GetCachedReclaimTarget();
  }
  return reclaim_target;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

UrgentPageDiscardingPolicy::UrgentPageDiscardingPolicy()
    : sustained_memory_pressure_timer_(
          FROM_HERE,
          base::Seconds(5),
          base::BindRepeating(
              &UrgentPageDiscardingPolicy::HandleMemoryPressureEvent,
              base::Unretained(this))) {
#if BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(
          performance_manager::features::kDiscardOnCommitLimit)) {
    monitor_ = base::AvailableMemoryMonitor::Get();
  }
#endif

  if (base::FeatureList::IsEnabled(features::kSustainedPMUrgentDiscarding)) {
    sustained_memory_pressure_evaluator_.emplace(base::BindRepeating(
        &UrgentPageDiscardingPolicy::OnSustainedMemoryPressure,
        base::Unretained(this)));
  } else {
    memory_pressure_listener_registration_.emplace(
        FROM_HERE, base::MemoryPressureListenerTag::kUrgentPageDiscardingPolicy,
        this);
  }

  if (monitor_) {
    monitor_->AddObserver(this);
  }
}
UrgentPageDiscardingPolicy::~UrgentPageDiscardingPolicy() {
  if (monitor_) {
    monitor_->RemoveObserver(this);
  }
}

void UrgentPageDiscardingPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(PageDiscardingHelper::GetFromGraph(graph))
      << "A PageDiscardingHelper instance should be registered against the "
         "graph in order to use this policy.";
}

void UrgentPageDiscardingPolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

#if BUILDFLAG(IS_CHROMEOS)
void UrgentPageDiscardingPolicy::OnReclaimTarget(
    base::TimeTicks on_memory_pressure_at,
    std::optional<memory_pressure::ReclaimTarget> reclaim_target) {
  bool discard_protected_pages = true;
  std::optional<base::TimeTicks> origin_time = std::nullopt;
  if (reclaim_target) {
    discard_protected_pages = reclaim_target->discard_protected;
    origin_time = reclaim_target->origin_time;
  }
  std::optional<base::TimeTicks> first_discarded_at =
      PageDiscardingHelper::GetFromGraph(GetOwningGraph())
          ->DiscardMultiplePages(
              reclaim_target, discard_protected_pages,
              DiscardEligibilityPolicy::DiscardReason::URGENT);

  if (origin_time && first_discarded_at) {
    base::TimeDelta reclaim_arrival_duration =
        on_memory_pressure_at - *origin_time;
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES("Discarding.ReclaimArrivalLatency",
                                          reclaim_arrival_duration);
    base::TimeDelta discard_duration =
        *first_discarded_at - on_memory_pressure_at;
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES("Discarding.DiscardLatency",
                                          discard_duration);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void UrgentPageDiscardingPolicy::DisableForTesting() {
  g_disabled_for_testing = true;
}

void UrgentPageDiscardingPolicy::OnMemoryPressure(
    base::MemoryPressureLevel new_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (new_level != base::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }

  if (monitor_) {
    // When kDiscardOnCommitLimit is enabled on Windows, this policy discards
    // pages based on the available commit space instead of memory pressure
    // signals.
    return;
  }

  HandleMemoryPressureEvent();
}

void UrgentPageDiscardingPolicy::OnAvailableMemoryUpdated(
    const base::AvailableMemoryMonitor::MemorySample& sample) {
#if BUILDFLAG(IS_WIN)
  if (sample.total_commit_bytes.is_zero()) {
    return;
  }

  double available_percent = (sample.available_commit_bytes.InBytesF() /
                              sample.total_commit_bytes.InBytesF()) *
                             100.0;
  double threshold =
      performance_manager::features::kDiscardOnCommitLimit_MinAvailablePercent
          .Get();

  if (available_percent < threshold) {
    HandleMemoryPressureEvent();
  }
#endif
}

void UrgentPageDiscardingPolicy::OnSustainedMemoryPressure(
    bool is_sustained_memory_pressure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_sustained_memory_pressure) {
    HandleMemoryPressureEvent();
    // Start the time that will continuously discard a tab while under sustained
    // memory pressure.
    sustained_memory_pressure_timer_.Reset();
  } else {
    sustained_memory_pressure_timer_.Stop();
  }
}

void UrgentPageDiscardingPolicy::HandleMemoryPressureEvent() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (g_disabled_for_testing) {
    return;
  }

  // Don't discard a page if urgent discarding is disabled. The feature state is
  // checked here instead of at policy creation time so that only clients that
  // experience memory pressure are enrolled in the experiment.
  if (!base::FeatureList::IsEnabled(
          performance_manager::features::kUrgentPageDiscarding)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  base::TimeTicks on_memory_pressure_at = base::TimeTicks::Now();
  // Chrome OS memory pressure evaluator provides the memory reclaim target to
  // leave critical memory pressure. When Chrome OS is under heavy memory
  // pressure, discards multiple tabs to meet the memory reclaim target.
  OnReclaimTarget(on_memory_pressure_at, GetReclaimTarget());
#else
  PageDiscardingHelper::GetFromGraph(GetOwningGraph())
      ->DiscardAPage(DiscardEligibilityPolicy::DiscardReason::URGENT);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace performance_manager::policies
