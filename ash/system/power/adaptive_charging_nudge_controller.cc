// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_nudge_controller.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/power/adaptive_charging_nudge.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

// Delay time before the nudge should appear.
constexpr base::TimeDelta kNudgeDelayTime = base::Seconds(3);

}  // namespace

AdaptiveChargingNudgeController::AdaptiveChargingNudgeController() {}
AdaptiveChargingNudgeController::~AdaptiveChargingNudgeController() {}

void AdaptiveChargingNudgeController::ShowNudge() {
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();

  // Nudge should be shown only once for an user forever.
  if (!pref_service ||
      !pref_service->GetBoolean(ash::prefs::kPowerAdaptiveChargingEnabled) ||
      pref_service->GetBoolean(ash::prefs::kPowerAdaptiveChargingNudgeShown)) {
    return;
  }

  // Show nudge if the delay timer is complete.
  if (nudge_delay_timer_ && !nudge_delay_timer_->IsRunning()) {
    pref_service->SetBoolean(ash::prefs::kPowerAdaptiveChargingNudgeShown,
                             true);
    SystemNudgeController::ShowNudge();
    return;
  }

  nudge_delay_timer_ = std::make_unique<base::OneShotTimer>();
  nudge_delay_timer_->Start(
      FROM_HERE, kNudgeDelayTime,
      base::BindOnce(&AdaptiveChargingNudgeController::ShowNudge,
                     weak_ptr_factory_.GetWeakPtr()));
}

#if DCHECK_IS_ON()
void AdaptiveChargingNudgeController::ShowNudgeForTesting() {
  SystemNudgeController::ShowNudge();
}
#endif  // DCHECK_IS_ON()

std::unique_ptr<SystemNudge>
AdaptiveChargingNudgeController::CreateSystemNudge() {
  return std::make_unique<AdaptiveChargingNudge>();
}

}  // namespace ash
