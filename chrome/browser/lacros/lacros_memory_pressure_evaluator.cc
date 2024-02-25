// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_memory_pressure_evaluator.h"

#include "base/logging.h"

#include "chromeos/lacros/lacros_service.h"

namespace {
// Pointer to the LacrosMemoryPressureEvaluator used by TabManagerDelegate for
// chromeos to need to call into ScheduleEarlyCheck.
LacrosMemoryPressureEvaluator* g_lacros_evaluator = nullptr;

// We try not to re-notify on moderate too frequently, this time
// controls how frequently we will notify after our first notification.
constexpr base::TimeDelta kModerateMemoryPressureCooldownTime =
    base::Seconds(10);

}  // namespace

LacrosMemoryPressureEvaluator::LacrosMemoryPressureEvaluator(
    std::unique_ptr<memory_pressure::MemoryPressureVoter> voter)
    : memory_pressure::SystemMemoryPressureEvaluator(std::move(voter)) {
  DCHECK(g_lacros_evaluator == nullptr);
  g_lacros_evaluator = this;

  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  // Check LacrosService availability to avoid crashing
  // lacros_chrome_browsertests.
  if (!service || !service->IsAvailable<crosapi::mojom::ResourceManager>()) {
    LOG(ERROR) << "ResourceManager is not available";
    return;
  }
  service->GetRemote<crosapi::mojom::ResourceManager>()
      ->AddMemoryPressureObserver(receiver_.BindNewPipeAndPassRemote());
}

LacrosMemoryPressureEvaluator::~LacrosMemoryPressureEvaluator() {
  DCHECK(g_lacros_evaluator == this);
  g_lacros_evaluator = nullptr;
}

// static
LacrosMemoryPressureEvaluator* LacrosMemoryPressureEvaluator::Get() {
  return g_lacros_evaluator;
}

memory_pressure::ReclaimTarget
LacrosMemoryPressureEvaluator::GetCachedReclaimTarget() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cached_reclaim_target_;
}

bool LacrosMemoryPressureEvaluator::ShouldNotify(
    const base::MemoryPressureListener::MemoryPressureLevel old_vote,
    const base::MemoryPressureListener::MemoryPressureLevel new_vote) {
  switch (new_vote) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      return false;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE: {
      // Moderate memory pressure notification advises modules to free buffers
      // that are cheap to re-allocate and not immediately needed. We may be in
      // this state for quite some time. Throttle the moderate notification to
      // avoid freeing unused buffers too often. Throttling is also necessary
      // when the vote is changed from critical or none to moderate.
      return last_moderate_notification_.is_null() ||
             (base::TimeTicks::Now() > last_moderate_notification_ +
                                           kModerateMemoryPressureCooldownTime);
    }
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      return true;
  }
}

void LacrosMemoryPressureEvaluator::MemoryPressure(
    crosapi::mojom::MemoryPressurePtr pressure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::MemoryPressureListener::MemoryPressureLevel listener_level;
  if (pressure->level == crosapi::mojom::MemoryPressureLevel::kCritical) {
    listener_level =
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL;
    cached_reclaim_target_ = memory_pressure::ReclaimTarget(
        pressure->reclaim_target_kb, pressure->signal_origin);
  } else if (pressure->level ==
             crosapi::mojom::MemoryPressureLevel::kModerate) {
    listener_level =
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
    cached_reclaim_target_ = memory_pressure::ReclaimTarget();
  } else {
    listener_level = base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE;
    cached_reclaim_target_ = memory_pressure::ReclaimTarget();
  }

  bool notify = ShouldNotify(current_vote(), listener_level);
  if (notify &&
      current_vote() ==
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE)
    last_moderate_notification_ = base::TimeTicks::Now();

  SetCurrentVote(listener_level);
  SendCurrentVote(notify);
}
