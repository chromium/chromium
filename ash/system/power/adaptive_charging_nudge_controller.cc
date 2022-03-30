// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/adaptive_charging_nudge_controller.h"

#include "ash/system/power/adaptive_charging_nudge.h"

namespace ash {

AdaptiveChargingNudgeController::AdaptiveChargingNudgeController() {}
AdaptiveChargingNudgeController::~AdaptiveChargingNudgeController() {}

std::unique_ptr<SystemNudge>
AdaptiveChargingNudgeController::CreateSystemNudge() {
  return std::make_unique<AdaptiveChargingNudge>();
}

}  // namespace ash
