// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DISPLAY_VARIABLE_REFRESH_RATE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_DISPLAY_VARIABLE_REFRESH_RATE_CONTROLLER_H_

#include "ash/system/power/power_status.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/game_mode/game_mode_controller.h"
#include "ui/display/manager/display_configurator.h"

namespace ash {

namespace {
using GameMode = ash::ResourcedClient::GameMode;
}  // namespace

// VariableRefreshRateController manages the VRR enabled/disabled state. It is
// responsible for communicating the desired VRR state to the configurator. VRR
// is meant to be enabled as long as Borealis game mode is active, except when
// battery saver mode is also active.
class VariableRefreshRateController
    : public PowerStatus::Observer,
      public game_mode::GameModeController::Observer {
 public:
  VariableRefreshRateController(
      display::DisplayConfigurator* display_configurator,
      PowerStatus* power_status,
      game_mode::GameModeController* game_mode_controller);

  VariableRefreshRateController(const VariableRefreshRateController&) = delete;
  VariableRefreshRateController& operator=(
      const VariableRefreshRateController&) = delete;

  ~VariableRefreshRateController() override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  // GameModeController::Observer implementation.
  void OnSetGameMode(GameMode game_mode) override;

 private:
  void RefreshState();

  GameMode game_mode_ = GameMode::OFF;

  // Not owned.
  raw_ptr<display::DisplayConfigurator> display_configurator_;
  const raw_ptr<PowerStatus, ExperimentalAsh> power_status_;

  base::ScopedObservation<ash::PowerStatus, ash::PowerStatus::Observer>
      power_status_observer_{this};
  base::ScopedObservation<game_mode::GameModeController,
                          game_mode::GameModeController::Observer>
      game_mode_observer_{this};

  base::WeakPtrFactory<VariableRefreshRateController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DISPLAY_VARIABLE_REFRESH_RATE_CONTROLLER_H_
