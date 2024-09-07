// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/refresh_rate_controller.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display_features.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/util/display_util.h"

namespace ash {
namespace {

const float kMinThrottledRefreshRate = 59.f;

using DisplayStateList = display::DisplayConfigurator::DisplayStateList;
using ModeState = DisplayPerformanceModeController::ModeState;
using RefreshRateOverrideMap =
    display::DisplayConfigurator::RefreshRateOverrideMap;

std::string RefreshRatesToString(const std::vector<float>& refresh_rates) {
  std::vector<std::string> entries;
  for (auto refresh_rate : refresh_rates) {
    entries.push_back(base::NumberToString(refresh_rate));
  }
  return "{" + base::JoinString(entries, ", ") + "}";
}
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
  OnDisplayConfigurationChanged(display_configurator->cached_displays());
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
    if (game_window_observer_.GetSource() != window) {
      game_window_observer_.Reset();
      // The GameModeController will always turn off game mode before the
      // observed window is destroyed.
      game_window_observer_.Observe(window);
    }
  } else {
    if (game_window_observer_.GetSource() == window) {
      game_window_observer_.Reset();
    } else {
      DCHECK(!game_window_observer_.IsObserving());
      // Game mode is already off. Nothing to do.
    }
  }

  UpdateStates();
}

void RefreshRateController::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window, game_window_observer_.GetSource());
  // Refresh state in case the window changed displays.
  UpdateStates();
}

void RefreshRateController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, game_window_observer_.GetSource());
  game_window_observer_.Reset();
  UpdateStates();
}

void RefreshRateController::OnDisplayConfigurationChanged(
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
      base::BindOnce(&RefreshRateController::OnSeamlessRefreshRatesReceived,
                     weak_ptr_factory_.GetWeakPtr(), display_id);
  display_configurator_->GetSeamlessRefreshRates(display_id,
                                                 std::move(callback));
}

void RefreshRateController::OnSeamlessRefreshRatesReceived(
    int64_t display_id,
    const std::optional<std::vector<float>>& received_refresh_rates) {
  VLOG(3) << "Received refresh rates for display " << display_id << ": "
          << (received_refresh_rates
                  ? RefreshRatesToString(*received_refresh_rates)
                  : "empty");

  if (!received_refresh_rates || received_refresh_rates->empty()) {
    // These cases could occur if there is a race between requesting the refresh
    // rates and some display topology change such as removing or disabling a
    // display.
    display_refresh_rates_.erase(display_id);
    refresh_rate_preferences_.erase(display_id);
    return;
  }

  // Sort in ascending order.
  std::vector<float> refresh_rates = received_refresh_rates.value();
  std::sort(refresh_rates.begin(), refresh_rates.end());

  // If the received refresh rates are equal to the last received refresh rates,
  // then we're done.
  auto it = display_refresh_rates_.find(display_id);
  if (it != display_refresh_rates_.end() && it->second == refresh_rates) {
    return;
  }

  // Insert the new refresh rates, possibly replacing the old ones.
  display_refresh_rates_[display_id] = std::move(refresh_rates);
  refresh_rate_preferences_.erase(display_id);

  RefreshOverrideState();

  aura::Window* window = Shell::GetRootWindowForDisplayId(display_id);
  if (window) {
    window->GetHost()->compositor()->SetSeamlessRefreshRates(
        display_refresh_rates_[display_id]);
  }
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
  RefreshOverrideState();
  RefreshVrrState();
}

void RefreshRateController::RefreshOverrideState() {
  if (!base::FeatureList::IsEnabled(
          ash::features::kSeamlessRefreshRateSwitching)) {
    return;
  }

  // Don't attempt dynamic refresh rate adjustment with hardware mirroring
  // enabled.
  if (display::features::IsHardwareMirrorModeEnabled()) {
    return;
  }

  RefreshRateOverrideMap refresh_rate_overrides = GetThrottleOverrides();

  // Use preferred refresh rates instead of throttled refresh rates if present.
  for (const auto& it : refresh_rate_preferences_) {
    if (display_refresh_rates_.contains(it.first)) {
      refresh_rate_overrides[it.first] = it.second;
    }
  }

  display_configurator_->SetRefreshRateOverrides(refresh_rate_overrides);
}

RefreshRateOverrideMap RefreshRateController::GetThrottleOverrides() {
  const ThrottleState throttle_state = GetDesiredThrottleState();
  if (throttle_state == ThrottleState::kDisabled) {
    return {};
  }

  // Update the override state for each display.
  RefreshRateOverrideMap refresh_rate_overrides;
  for (const auto& it : display_refresh_rates_) {
    // Only throttle the internal display.
    if (!display::IsInternalDisplayId(it.first)) {
      continue;
    }

    // Filter out refresh rates lower than the minimum.
    std::vector<float> throttle_candidates;
    for (auto refresh_rate : it.second) {
      if (refresh_rate >= kMinThrottledRefreshRate) {
        throttle_candidates.push_back(refresh_rate);
      }
    }

    if (throttle_candidates.size() < 2) {
      VLOG(3) << "Fewer than 2 throttle candidates for display " << it.first;
      continue;
    }

    refresh_rate_overrides[it.first] = throttle_candidates.front();
    VLOG(3) << "Request refresh rate for display " << it.first << ": "
            << refresh_rate_overrides[it.first];
  }

  return refresh_rate_overrides;
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

RefreshRateController::ThrottleState
RefreshRateController::GetDesiredThrottleState() {
  if (force_throttle_) {
    return ThrottleState::kEnabled;
  }

  switch (current_performance_mode_) {
    case ModeState::kPowerSaver:
      return ThrottleState::kEnabled;
    case ModeState::kHighPerformance:
      return ThrottleState::kDisabled;
    case ModeState::kIntelligent:
      return GetDynamicThrottleState();
    default:
      NOTREACHED();
  }
}

RefreshRateController::ThrottleState
RefreshRateController::GetDynamicThrottleState() {
  // Do not throttle when Borealis is active on the internal display.
  if (game_window_observer_.IsObserving() &&
      display::Screen::GetScreen()
              ->GetDisplayNearestWindow(game_window_observer_.GetSource())
              .id() == display::Display::InternalDisplayId()) {
    return ThrottleState::kDisabled;
  }

  if (power_status_->IsMainsChargerConnected()) {
    return ThrottleState::kDisabled;
  }

  return ThrottleState::kEnabled;
}

void RefreshRateController::OnSetPreferredRefreshRate(
    aura::WindowTreeHost* host,
    float preferred_refresh_rate) {
  CHECK(display::Screen::HasScreen());
  const int64_t display_id = display::Screen::GetScreen()
                                 ->GetDisplayNearestWindow(host->window())
                                 .id();

  // Only honor preferences for the internal display.
  if (!display::IsInternalDisplayId(display_id)) {
    return;
  }

  // No change.
  const auto& it = refresh_rate_preferences_.find(display_id);
  if (it != refresh_rate_preferences_.end() &&
      it->second == preferred_refresh_rate) {
    return;
  }

  if (preferred_refresh_rate) {
    refresh_rate_preferences_[display_id] = preferred_refresh_rate;
  } else {
    refresh_rate_preferences_.erase(display_id);
  }

  RefreshOverrideState();
}

void RefreshRateController::OnWindowTreeHostCreated(
    aura::WindowTreeHost* host) {
  host->AddObserver(this);
}

}  // namespace ash
