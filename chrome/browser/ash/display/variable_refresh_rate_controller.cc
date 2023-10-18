// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/display/variable_refresh_rate_controller.h"

#include "ui/base/ui_base_features.h"

namespace ash {

VariableRefreshRateController::VariableRefreshRateController(
    display::DisplayConfigurator* display_configurator,
    PowerStatus* power_status,
    game_mode::GameModeController* game_mode_controller)
    : display_configurator_(display_configurator), power_status_(power_status) {
  power_status_observer_.Observe(power_status);
  game_mode_observer_.Observe(game_mode_controller);
}

VariableRefreshRateController::~VariableRefreshRateController() = default;

void VariableRefreshRateController::OnPowerStatusChanged() {
  RefreshState();
}

void VariableRefreshRateController::OnSetGameMode(GameMode game_mode) {
  game_mode_ = game_mode;
  RefreshState();
}

void VariableRefreshRateController::RefreshState() {
  const bool battery_saver_mode_enabled = power_status_->IsBatterySaverActive();
  display_configurator_->SetVrrEnabled(
      ::features::IsVariableRefreshRateAlwaysOn() ||
      (::features::IsVariableRefreshRateEnabled() &&
       !battery_saver_mode_enabled && game_mode_ == GameMode::BOREALIS));
}

}  // namespace ash
