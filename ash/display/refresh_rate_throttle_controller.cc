// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/refresh_rate_throttle_controller.h"

#include "base/logging.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"

namespace ash {
namespace {

display::RefreshRateThrottleState GetDesiredThrottleState(
    const PowerStatus* status) {
  if (status->IsBatterySaverActive()) {
    return display::kRefreshRateThrottleEnabled;
  }
  if (!status->IsMainsChargerConnected()) {
    return display::kRefreshRateThrottleEnabled;
  }
  return display::kRefreshRateThrottleDisabled;
}

}  // namespace

RefreshRateThrottleController::RefreshRateThrottleController(
    display::DisplayConfigurator* display_configurator,
    PowerStatus* power_status)
    : power_status_observer_(this),
      display_configurator_(display_configurator),
      power_status_(power_status) {
  power_status_observer_.Observe(power_status);
}

RefreshRateThrottleController::~RefreshRateThrottleController() = default;

void RefreshRateThrottleController::OnPowerStatusChanged() {
  VLOG(4) << "Battery percent: " << power_status_->GetBatteryPercent()
          << ", High Power Charger: "
          << (power_status_->IsMainsChargerConnected() ? "yes" : "no");
  display::RefreshRateThrottleState state =
      GetDesiredThrottleState(power_status_);
  if (display::HasInternalDisplay()) {
    display_configurator_->MaybeSetRefreshRateThrottleState(
        display::Display::InternalDisplayId(), state);
  }
}

}  // namespace ash
