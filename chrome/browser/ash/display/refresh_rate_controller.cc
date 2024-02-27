// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/display/refresh_rate_controller.h"

#include "ash/constants/ash_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/display/util/display_util.h"

namespace ash {

RefreshRateController::RefreshRateController(
    display::DisplayConfigurator* display_configurator,
    PowerStatus* power_status,
    game_mode::GameModeController* game_mode_controller,
    bool force_throttle)
    : display_configurator_(display_configurator),
      power_status_(power_status),
      force_throttle_(force_throttle) {
  power_status_observer_.Observe(power_status);
  game_mode_observer_.Observe(game_mode_controller);
  // Ensure initial states are calculated.
  RefreshThrottleState();
  RefreshVrrState();
}

RefreshRateController::~RefreshRateController() = default;

void RefreshRateController::OnPowerStatusChanged() {
  RefreshThrottleState();
  RefreshVrrState();
}

void RefreshRateController::OnSetGameMode(GameMode game_mode,
                                          WindowState* window_state) {
  // Update the |borealis_window_observer_|.
  if (game_mode == GameMode::BOREALIS) {
    // The GameModeController will always turn off game mode before the observed
    // window is destroyed.
    borealis_window_observer_.Observe(window_state->window());
  } else {
    borealis_window_observer_.Reset();
  }

  RefreshThrottleState();
  RefreshVrrState();
}

void RefreshRateController::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window, borealis_window_observer_.GetSource());
  // Refresh state in case the window changed displays.
  RefreshThrottleState();
  RefreshVrrState();
}

void RefreshRateController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (borealis_window_observer_.IsObserving() &&
      (changed_metrics & DISPLAY_METRIC_PRIMARY)) {
    // Refresh state in case the window is affected by the primary display
    // change.
    RefreshThrottleState();
    RefreshVrrState();
  }
}

void RefreshRateController::RefreshThrottleState() {
  if (!base::FeatureList::IsEnabled(
          ash::features::kSeamlessRefreshRateSwitching)) {
    return;
  }

  // Only internal displays utilize refresh rate throttling.
  if (!display::HasInternalDisplay()) {
    return;
  }

  display_configurator_->MaybeSetRefreshRateThrottleState(
      display::Display::InternalDisplayId(), GetDesiredThrottleState());
}

void RefreshRateController::RefreshVrrState() {
  // If VRR is always on, state will not need to be refreshed.
  if (::features::IsVariableRefreshRateAlwaysOn()) {
    return;
  }

  if (!::features::IsVariableRefreshRateEnabled()) {
    return;
  }

  // Enable VRR on the borealis-hosting display if battery saver is inactive.
  const bool battery_saver_mode_enabled = power_status_->IsBatterySaverActive();
  if (borealis_window_observer_.IsObserving() && !battery_saver_mode_enabled) {
    display_configurator_->SetVrrEnabled(
        {display::Screen::GetScreen()
             ->GetDisplayNearestWindow(borealis_window_observer_.GetSource())
             .id()});
  } else {
    display_configurator_->SetVrrEnabled({});
  }
}

display::RefreshRateThrottleState
RefreshRateController::GetDesiredThrottleState() {
  if (force_throttle_) {
    return display::kRefreshRateThrottleEnabled;
  }

  if (power_status_->IsBatterySaverActive()) {
    return display::kRefreshRateThrottleEnabled;
  }

  // Do not throttle when Borealis is active on the internal display.
  if (borealis_window_observer_.IsObserving() &&
      display::Screen::GetScreen()
              ->GetDisplayNearestWindow(borealis_window_observer_.GetSource())
              .id() == display::Display::InternalDisplayId()) {
    return display::kRefreshRateThrottleDisabled;
  }

  if (power_status_->IsMainsChargerConnected()) {
    return display::kRefreshRateThrottleDisabled;
  }

  return display::kRefreshRateThrottleEnabled;
}

}  // namespace ash
