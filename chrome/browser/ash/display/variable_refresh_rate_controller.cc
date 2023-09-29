// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/display/variable_refresh_rate_controller.h"

#include "chromeos/dbus/power_manager/battery_saver.pb.h"
#include "ui/base/ui_base_features.h"

namespace ash {

VariableRefreshRateController::VariableRefreshRateController(
    display::DisplayConfigurator* display_configurator,
    chromeos::PowerManagerClient* power_manager_client,
    game_mode::GameModeController* game_mode_controller)
    : display_configurator_(display_configurator) {
  battery_saver_mode_observer_.Observe(power_manager_client);
  game_mode_observer_.Observe(game_mode_controller);
}

VariableRefreshRateController::~VariableRefreshRateController() = default;

void VariableRefreshRateController::BatterySaverModeStateChanged(
    const power_manager::BatterySaverModeState& state) {
  battery_saver_mode_enabled_ = state.has_enabled() && state.enabled();
  RefreshState();
}

void VariableRefreshRateController::OnSetGameMode(GameMode game_mode) {
  game_mode_ = game_mode;
  RefreshState();
}

void VariableRefreshRateController::RefreshState() {
  display_configurator_->SetVrrEnabled(
      ::features::IsVariableRefreshRateAlwaysOn() ||
      (::features::IsVariableRefreshRateEnabled() &&
       !battery_saver_mode_enabled_ && game_mode_ == GameMode::BOREALIS));
}

}  // namespace ash
