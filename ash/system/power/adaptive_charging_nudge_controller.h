// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/tray/system_nudge_controller.h"

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

 private:
  // SystemNudgeController:
  std::unique_ptr<SystemNudge> CreateSystemNudge() override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NUDGE_CONTROLLER_H_
