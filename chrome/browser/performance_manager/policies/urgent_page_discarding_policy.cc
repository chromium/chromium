// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/urgent_page_discarding_policy.h"

#include <memory>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/policies/policy_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/metrics/histogram_macros.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_memory_pressure_evaluator.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/memory/pressure/system_memory_pressure_evaluator.h"
#endif

namespace performance_manager::policies {

namespace {

#if BUILDFLAG(IS_CHROMEOS)
std::optional<memory_pressure::ReclaimTarget> GetReclaimTarget() {
  std::optional<memory_pressure::ReclaimTarget> reclaim_target = std::nullopt;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* evaluator = LacrosMemoryPressureEvaluator::Get();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  auto* evaluator = ash::memory::SystemMemoryPressureEvaluator::Get();
#endif
  if (evaluator) {
    reclaim_target = evaluator->GetCachedReclaimTarget();
  }
  return reclaim_target;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace

UrgentPageDiscardingPolicy::UrgentPageDiscardingPolicy() = default;
UrgentPageDiscardingPolicy::~UrgentPageDiscardingPolicy() = default;

void UrgentPageDiscardingPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!handling_memory_pressure_notification_);
  graph->AddSystemNodeObserver(this);
  DCHECK(PageDiscardingHelper::GetFromGraph(graph))
      << "A PageDiscardingHelper instance should be registered against the "
         "graph in order to use this policy.";
}

void UrgentPageDiscardingPolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveSystemNodeObserver(this);
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
  PageDiscardingHelper::GetFromGraph(GetOwningGraph())
      ->DiscardMultiplePages(
          reclaim_target, discard_protected_pages,
          base::BindOnce(
              [](UrgentPageDiscardingPolicy* policy,
                 std::optional<base::TimeTicks> origin_time,
                 base::TimeTicks on_memory_pressure_at,
                 std::optional<base::TimeTicks> first_discarded_at) {
                DCHECK(policy->handling_memory_pressure_notification_);
                policy->handling_memory_pressure_notification_ = false;
                if (origin_time && first_discarded_at) {
                  base::TimeDelta reclaim_arrival_duration =
                      on_memory_pressure_at - *origin_time;
                  UMA_HISTOGRAM_MEDIUM_TIMES("Discarding.ReclaimArrivalLatency",
                                             reclaim_arrival_duration);
                  base::TimeDelta discard_duration =
                      *first_discarded_at - on_memory_pressure_at;
                  UMA_HISTOGRAM_MEDIUM_TIMES("Discarding.DiscardLatency",
                                             discard_duration);
                }
              },
              base::Unretained(this), origin_time, on_memory_pressure_at),
          PageDiscardingHelper::DiscardReason::URGENT);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void UrgentPageDiscardingPolicy::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel new_level) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The Memory Pressure Monitor will send notifications at regular interval,
  // |handling_memory_pressure_notification_| prevents this class from trying to
  // reply to multiple notifications at the same time.
  if (handling_memory_pressure_notification_ ||
      new_level != base::MemoryPressureListener::MemoryPressureLevel::
                       MEMORY_PRESSURE_LEVEL_CRITICAL) {
    return;
  }

  // Don't discard a page if urgent discarding is disabled. The feature state is
  // checked here instead of at policy creation time so that only clients that
  // experience memory pressure are enrolled in the experiment.
  if (!base::FeatureList::IsEnabled(
          performance_manager::features::kUrgentPageDiscarding)) {
    return;
  }

  handling_memory_pressure_notification_ = true;

#if BUILDFLAG(IS_CHROMEOS)
  base::TimeTicks on_memory_pressure_at = base::TimeTicks::Now();
  // Chrome OS memory pressure evaluator provides the memory reclaim target to
  // leave critical memory pressure. When Chrome OS is under heavy memory
  // pressure, discards multiple tabs to meet the memory reclaim target.
  content::GetUIThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(GetReclaimTarget),
      base::BindOnce(&UrgentPageDiscardingPolicy::OnReclaimTarget,
                     base::Unretained(this), on_memory_pressure_at));
#else
  PageDiscardingHelper::GetFromGraph(GetOwningGraph())
      ->DiscardAPage(
          base::BindOnce(
              [](UrgentPageDiscardingPolicy* policy,
                 std::optional<base::TimeTicks> first_discarded_at_unused) {
                DCHECK(policy->handling_memory_pressure_notification_);
                policy->handling_memory_pressure_notification_ = false;
              },
              // |PageDiscardingHelper| and this class are both GraphOwned
              // objects, their lifetime is tied to the Graph's lifetime and
              // both objects will be released sequentially while it's being
              // torn down. This ensures that the reply callback passed to
              // |DiscardAPage| won't ever run after the destruction of this
              // class and so it's safe to use Unretained.
              base::Unretained(this)),
          PageDiscardingHelper::DiscardReason::URGENT);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace performance_manager::policies
