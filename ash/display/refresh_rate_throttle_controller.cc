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
const float kLowBatteryThreshold = 5.0f;

display::RefreshRateThrottleState GetDesiredThrottleState(
    const PowerStatus* status) {
  if (status->GetBatteryPercent() < kLowBatteryThreshold)
    return display::kRefreshRateThrottleEnabled;
  if (!status->IsMainsChargerConnected())
    return display::kRefreshRateThrottleEnabled;
  return display::kRefreshRateThrottleDisabled;
}

const display::DisplaySnapshot* GetInternalDisplay(
    display::DisplayConfigurator* configurator) {
  for (const display::DisplaySnapshot* snapshot :
       configurator->cached_displays()) {
    if (snapshot->type() == display::DISPLAY_CONNECTION_TYPE_INTERNAL)
      return snapshot;
  }
  return nullptr;
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
  const display::DisplaySnapshot* internal_display =
      GetInternalDisplay(display_configurator_);
  if (internal_display == nullptr) {
    VLOG(4) << "No internal display present.";
    return;
  }
  VLOG(4) << "Battery percent: " << power_status_->GetBatteryPercent()
          << ", High Power Charger: "
          << (power_status_->IsMainsChargerConnected() ? "yes" : "no");
  display::RefreshRateThrottleState state =
      GetDesiredThrottleState(power_status_);
  display_configurator_->MaybeSetRefreshRateThrottleState(
      internal_display->display_id(), state);
}

}  // namespace ash
