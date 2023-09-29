// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DISPLAY_VARIABLE_REFRESH_RATE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_DISPLAY_VARIABLE_REFRESH_RATE_CONTROLLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/game_mode/game_mode_controller.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/display/manager/display_configurator.h"

namespace power_manager {
class BatterySaverModeState;
}  // namespace power_manager

namespace ash {

namespace {
using GameMode = ash::ResourcedClient::GameMode;
}  // namespace

// VariableRefreshRateController manages the VRR enabled/disabled state. It is
// responsible for communicating the desired VRR state to the configurator. VRR
// is meant to be enabled as long as Borealis game mode is active, except when
// battery saver mode is also active.
class VariableRefreshRateController
    : public chromeos::PowerManagerClient::Observer,
      public game_mode::GameModeController::Observer {
 public:
  VariableRefreshRateController(
      display::DisplayConfigurator* display_configurator,
      chromeos::PowerManagerClient* power_status,
      game_mode::GameModeController* game_mode_controller);

  VariableRefreshRateController(const VariableRefreshRateController&) = delete;
  VariableRefreshRateController& operator=(
      const VariableRefreshRateController&) = delete;

  ~VariableRefreshRateController() override;

  // PowerManagerClient::Observer implementation.
  void BatterySaverModeStateChanged(
      const power_manager::BatterySaverModeState& state) override;

  // GameModeController::Observer implementation.
  void OnSetGameMode(GameMode game_mode) override;

 private:
  void RefreshState();

  bool battery_saver_mode_enabled_ = false;
  GameMode game_mode_ = GameMode::OFF;

  // Not owned.
  raw_ptr<display::DisplayConfigurator> display_configurator_;

  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      battery_saver_mode_observer_{this};
  base::ScopedObservation<game_mode::GameModeController,
                          game_mode::GameModeController::Observer>
      game_mode_observer_{this};

  base::WeakPtrFactory<VariableRefreshRateController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DISPLAY_VARIABLE_REFRESH_RATE_CONTROLLER_H_
