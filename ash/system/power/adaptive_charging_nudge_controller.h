// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/tray/system_nudge_controller.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace ash {

class SystemNudge;

// Class that manages showing a nudge explaining that adaptive charging has been
// enabled on this device.
class ASH_EXPORT AdaptiveChargingNudgeController
    : public SystemNudgeController {
 public:
  AdaptiveChargingNudgeController();
  AdaptiveChargingNudgeController(const AdaptiveChargingNudgeController&) =
      delete;
  AdaptiveChargingNudgeController operator=(
      const AdaptiveChargingNudgeController&) = delete;
  ~AdaptiveChargingNudgeController() override;

  // Show the Adaptive Charging educational nudge.
  void ShowNudge();

  // Test method to get the nudge delay timer for testing.
  base::OneShotTimer* GetNudgeDelayTimerForTesting() {
    return nudge_delay_timer_.get();
  }

#if DCHECK_IS_ON()
  // This is intended to be used by developers to test the UI of the adaptive
  // charging feature.
  void ShowNudgeForTesting();
#endif  // DCHECK_IS_ON()

 private:
  // SystemNudgeController:
  std::unique_ptr<SystemNudge> CreateSystemNudge() override;

  // Timer to delay showing the nudge.
  std::unique_ptr<base::OneShotTimer> nudge_delay_timer_;

  base::WeakPtrFactory<AdaptiveChargingNudgeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NUDGE_CONTROLLER_H_
