// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/display/refresh_rate_controller.h"

#include "ash/constants/ash_features.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"

namespace ash {

namespace {
using GameMode = ash::ResourcedClient::GameMode;
using DisplayStateList = display::DisplayConfigurator::DisplayStateList;
using ModeState = DisplayPerformanceModeController::ModeState;
}  // namespace

RefreshRateController::RefreshRateController(
    display::DisplayConfigurator* display_configurator,
    PowerStatus* power_status,
    game_mode::GameModeController* game_mode_controller,
    DisplayPerformanceModeController* display_performance_mode_controller,
    bool force_throttle)
    : display_configurator_(display_configurator),
      power_status_(power_status),
      display_performance_mode_controller_(display_performance_mode_controller),
      force_throttle_(force_throttle) {
  power_status_observer_.Observe(power_status);
  game_mode_observer_.Observe(game_mode_controller);
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

  UpdateStates();
}

void RefreshRateController::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window, borealis_window_observer_.GetSource());
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

void RefreshRateController::UpdateSeamlessRefreshRates(int64_t display_id) {
  auto callback =
      base::BindOnce(&RefreshRateController::OnSeamlessRefreshRangeReceived,
                     weak_ptr_factory_.GetWeakPtr(), display_id);
  display_configurator_->GetSeamlessRefreshRates(display_id,
                                                 std::move(callback));
}

void RefreshRateController::OnSeamlessRefreshRangeReceived(
    int64_t display_id,
    const std::optional<display::RefreshRange>& refresh_ranges) {
  // TODO(b/323362145): Stash the refresh rates and request a refresh rate
  // explicitly when throttling.
}

void RefreshRateController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (borealis_window_observer_.IsObserving() &&
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
  if (borealis_window_observer_.IsObserving() &&
      current_performance_mode_ != ModeState::kPowerSaver) {
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
