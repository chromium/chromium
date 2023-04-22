// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_REFRESH_RATE_THROTTLE_CONTROLLER_H_
#define ASH_DISPLAY_REFRESH_RATE_THROTTLE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/system/power/power_status.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/display/manager/display_configurator.h"

namespace ash {

// Watches device power state and requests the refresh rate to be throttled
// when in a low power state.
class ASH_EXPORT RefreshRateThrottleController : public PowerStatus::Observer {
 public:
  RefreshRateThrottleController(
      display::DisplayConfigurator* display_configurator,
      PowerStatus* power_status);

  RefreshRateThrottleController(const RefreshRateThrottleController&) = delete;
  RefreshRateThrottleController& operator=(
      const RefreshRateThrottleController&) = delete;
  ~RefreshRateThrottleController() override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

 private:
  base::ScopedObservation<ash::PowerStatus, ash::PowerStatus::Observer>
      power_status_observer_;

  // Not owned.
  const raw_ptr<display::DisplayConfigurator, ExperimentalAsh>
      display_configurator_;
  const raw_ptr<PowerStatus, ExperimentalAsh> power_status_;
};

}  // namespace ash

#endif  // ASH_DISPLAY_REFRESH_RATE_THROTTLE_CONTROLLER_H_
