// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_REFRESH_RATE_CONTROLLER_H_
#define ASH_DISPLAY_REFRESH_RATE_CONTROLLER_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/display/display_performance_mode_controller.h"
#include "ash/system/power/power_status.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_configurator.h"

namespace ash {


// RefreshRateController manages features related to display refresh rate, such
// as the VRR enabled/disabled state and refresh rate throttling. It is responsible
// for communicating desired state to the configurator.
class ASH_EXPORT RefreshRateController
    : public PowerStatus::Observer,
      public aura::WindowObserver,
      public display::DisplayObserver,
      public display::DisplayConfigurator::Observer,
      public DisplayPerformanceModeController::Observer,
      public aura::WindowTreeHostObserver {
 public:
  RefreshRateController(
      display::DisplayConfigurator* display_configurator,
      PowerStatus* power_status,
      DisplayPerformanceModeController* display_performance_mode_controller);

  RefreshRateController(const RefreshRateController&) = delete;
  RefreshRateController& operator=(const RefreshRateController&) = delete;

  ~RefreshRateController() override;

  // PowerStatus::Observer implementation.
  void OnPowerStatusChanged() override;

  // Set Game mode on the window.
  void SetGameMode(aura::Window* window, bool game_mode_on);

  // WindowObserver implementation.
  void OnWindowAddedToRootWindow(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  // DisplayObserver implementation.
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // DisplayConfigurator::Observer implementation.
  void OnDisplayConfigurationChanged(
      const display::DisplayConfigurator::DisplayStateList& displays) override;

  // DisplayPerformanceModeController::Observer implementation.
  void OnDisplayPerformanceModeChanged(
      DisplayPerformanceModeController::ModeState new_state) override;

  // WindowTreeHostObserver implementation.
  void OnSetPreferredRefreshRate(aura::WindowTreeHost* host,
                                 float preferred_refresh_rate) override;

  void StopObservingPowerStatusForTest();

  void OnWindowTreeHostCreated(aura::WindowTreeHost* host);

 private:
  // The requested state for refresh rate throttling.
  enum class ThrottleState {
    kEnabled,
    kDisabled,
  };

  void UpdateSeamlessRefreshRates(int64_t display_id);
  void OnSeamlessRefreshRatesReceived(
      int64_t display_id,
      const std::optional<std::vector<float>>& refresh_rates);

  void UpdateStates();
  void RefreshOverrideState();
  void RefreshVrrState();
  ThrottleState GetDesiredThrottleState();
  ThrottleState GetDynamicThrottleState();
  // Returns a refresh rates override map populated according to the desired
  // throttle state.
  display::DisplayConfigurator::RefreshRateOverrideMap GetThrottleOverrides();

  // Not owned.
  raw_ptr<display::DisplayConfigurator> display_configurator_;
  raw_ptr<PowerStatus> power_status_;

  raw_ptr<DisplayPerformanceModeController>
      display_performance_mode_controller_;
  DisplayPerformanceModeController::DisplayPerformanceModeController::ModeState
      current_performance_mode_ = DisplayPerformanceModeController::
          DisplayPerformanceModeController::ModeState::kDefault;

  bool force_throttle_ = false;

  std::unordered_map<int64_t, std::vector<float>> display_refresh_rates_;
  std::unordered_map<int64_t, float> refresh_rate_preferences_;

  base::ScopedObservation<ash::PowerStatus, ash::PowerStatus::Observer>
      power_status_observer_{this};
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      game_window_observer_{this};
  display::ScopedDisplayObserver display_observer_{this};
  base::ScopedObservation<display::DisplayConfigurator,
                          display::DisplayConfigurator::Observer>
      display_configurator_observer_{this};

  base::WeakPtrFactory<RefreshRateController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_DISPLAY_REFRESH_RATE_CONTROLLER_H_
