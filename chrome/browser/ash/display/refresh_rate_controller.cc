// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/display/refresh_rate_controller.h"

#include "ash/constants/ash_features.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/util/display_util.h"

namespace ash {

namespace {

display::RefreshRateThrottleState GetDesiredThrottleState(
    const PowerStatus* status,
    GameMode game_mode) {
  if (status->IsBatterySaverActive()) {
    return display::kRefreshRateThrottleEnabled;
  }
  // Do not throttle when Borealis is active.
  if (game_mode == GameMode::BOREALIS) {
    return display::kRefreshRateThrottleDisabled;
  }
  if (!status->IsMainsChargerConnected()) {
    return display::kRefreshRateThrottleEnabled;
  }
  return display::kRefreshRateThrottleDisabled;
}

}  // namespace

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
}

RefreshRateController::~RefreshRateController() = default;

void RefreshRateController::OnPowerStatusChanged() {
  RefreshState();
}

void RefreshRateController::OnSetGameMode(GameMode game_mode) {
  game_mode_ = game_mode;
  RefreshState();
}

void RefreshRateController::RefreshState() {
  const bool battery_saver_mode_enabled = power_status_->IsBatterySaverActive();
  display_configurator_->SetVrrEnabled(
      ::features::IsVariableRefreshRateAlwaysOn() ||
      (::features::IsVariableRefreshRateEnabled() &&
       !battery_saver_mode_enabled && game_mode_ == GameMode::BOREALIS));

  if (base::FeatureList::IsEnabled(
          ash::features::kSeamlessRefreshRateSwitching)) {
    display::RefreshRateThrottleState state =
        GetDesiredThrottleState(power_status_, game_mode_);
    if (force_throttle_) {
      state = display::kRefreshRateThrottleEnabled;
    }
    if (display::HasInternalDisplay()) {
      display_configurator_->MaybeSetRefreshRateThrottleState(
          display::Display::InternalDisplayId(), state);
    }
  }
}

}  // namespace ash
