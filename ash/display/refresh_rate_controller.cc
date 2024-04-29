// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/refresh_rate_controller.h"

#include "ash/constants/ash_features.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display_features.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"
#include "base/command_line.h"
#include "ash/constants/ash_switches.h"

namespace ash {

namespace {
using DisplayStateList = display::DisplayConfigurator::DisplayStateList;
using ModeState = DisplayPerformanceModeController::ModeState;
}  // namespace

RefreshRateController::RefreshRateController(
    display::DisplayConfigurator* display_configurator,
    PowerStatus* power_status,
    DisplayPerformanceModeController* display_performance_mode_controller)
    : display_configurator_(display_configurator),
      power_status_(power_status),
      display_performance_mode_controller_(display_performance_mode_controller),
      force_throttle_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kForceRefreshRateThrottle)) {
  power_status_observer_.Observe(power_status);
  display_configurator_observer_.Observe(display_configurator);
  current_performance_mode_ =
      display_performance_mode_controller_->AddObserver(this);
  // Ensure initial states are calculated.
  UpdateStates();
}

RefreshRateController::~RefreshRateController() {
  display_performance_mode_controller_->RemoveObserver(this);
}

void RefreshRateController::OnPowerStatusChanged() {
  UpdateStates();
}

void RefreshRateController::SetGameMode(aura::Window* window,
                                        bool game_mode_on) {
  // Update the |game_window_observer_|.
  if (game_mode_on) {
    // The GameModeController will always turn off game mode before the observed
    // window is destroyed.
    game_window_observer_.Observe(window);
  } else {
    game_window_observer_.Reset();
  }

  UpdateStates();
}

void RefreshRateController::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window, game_window_observer_.GetSource());
  // Refresh state in case the window changed displays.
  UpdateStates();
}

void RefreshRateController::OnDisplayModeChanged(
    const DisplayStateList& displays) {
  for (const display::DisplaySnapshot* snapshot : displays) {
    if (!snapshot->current_mode()) {
      continue;
    }
    UpdateSeamlessRefreshRates(snapshot->display_id());
  }
}

void RefreshRateController::StopObservingPowerStatusForTest() {
  power_status_observer_.Reset();
  power_status_ = nullptr;
}

void RefreshRateController::UpdateSeamlessRefreshRates(int64_t display_id) {
  // Don't attempt dynamic refresh rate adjustment with hardware mirroring
  // enabled.
  if (display::features::IsHardwareMirrorModeEnabled()) {
    return;
  }

  auto callback =
      base::BindOnce(&RefreshRateController::OnSeamlessRefreshRangeReceived,
                     weak_ptr_factory_.GetWeakPtr(), display_id);
  display_configurator_->GetSeamlessRefreshRates(display_id,
                                                 std::move(callback));
}

void RefreshRateController::OnSeamlessRefreshRangeReceived(
    int64_t display_id,
    const std::optional<std::vector<float>>& refresh_ranges) {
  // TODO(b/323362145): Stash the refresh rates and request a refresh rate
  // explicitly when throttling.
}

void RefreshRateController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (game_window_observer_.IsObserving() &&
      (changed_metrics & DISPLAY_METRIC_PRIMARY)) {
    // Refresh state in case the window is affected by the primary display
    // change.
    UpdateStates();
  }
}

void RefreshRateController::OnDisplayPerformanceModeChanged(
    ModeState new_state) {
  current_performance_mode_ = new_state;
  UpdateStates();
}

void RefreshRateController::UpdateStates() {
  RefreshThrottleState();
  RefreshVrrState();
}

void RefreshRateController::RefreshThrottleState() {
  if (!base::FeatureList::IsEnabled(
          ash::features::kSeamlessRefreshRateSwitching)) {
    return;
  }

  // Don't attempt dynamic refresh rate adjustment with hardware mirroring
  // enabled.
  if (display::features::IsHardwareMirrorModeEnabled()) {
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
  if (game_window_observer_.IsObserving() &&
      current_performance_mode_ != ModeState::kPowerSaver) {
    display_configurator_->SetVrrEnabled(
        {display::Screen::GetScreen()
             ->GetDisplayNearestWindow(game_window_observer_.GetSource())
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

  switch (current_performance_mode_) {
    case ModeState::kPowerSaver:
      return display::kRefreshRateThrottleEnabled;
    case ModeState::kHighPerformance:
      return display::kRefreshRateThrottleDisabled;
    case ModeState::kIntelligent:
      return GetDynamicThrottleState();
    default:
      NOTREACHED();
      return display::kRefreshRateThrottleEnabled;
  }
}

display::RefreshRateThrottleState
RefreshRateController::GetDynamicThrottleState() {
  // Do not throttle when Borealis is active on the internal display.
  if (game_window_observer_.IsObserving() &&
      display::Screen::GetScreen()
              ->GetDisplayNearestWindow(game_window_observer_.GetSource())
              .id() == display::Display::InternalDisplayId()) {
    return display::kRefreshRateThrottleDisabled;
  }

  if (power_status_->IsMainsChargerConnected()) {
    return display::kRefreshRateThrottleDisabled;
  }

  return display::kRefreshRateThrottleEnabled;
}

}  // namespace ash
