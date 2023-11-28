// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/human_presence/human_presence_orientation_controller.h"

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/bind.h"
#include "base/scoped_observation.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/util/display_util.h"

namespace ash {

HumanPresenceOrientationController::HumanPresenceOrientationController() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  DCHECK(tablet_mode_controller);
  physical_tablet_state_ =
      tablet_mode_controller->is_in_tablet_physical_state();
  tablet_mode_observation_.Observe(tablet_mode_controller);

  chromeos::PowerManagerClient* power_manager_client =
      chromeos::PowerManagerClient::Get();
  DCHECK(power_manager_client);
  power_manager_client_observation_.Observe(power_manager_client);
  power_manager_client->GetSwitchStates(
      base::BindOnce(&HumanPresenceOrientationController::OnReceiveSwitchStates,
                     weak_factory_.GetWeakPtr()));

  // Only care about rotation of the actual device.
  if (!display::HasInternalDisplay())
    return;

  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  DCHECK(display_manager);
  display_rotated_ =
      display_manager->GetDisplayForId(display::Display::InternalDisplayId())
          .rotation() != display::Display::ROTATE_0;
}

HumanPresenceOrientationController::~HumanPresenceOrientationController() =
    default;

void HumanPresenceOrientationController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HumanPresenceOrientationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool HumanPresenceOrientationController::IsOrientationSuitable() const {
  return !physical_tablet_state_ && !display_rotated_ && !lid_closed_;
}

void HumanPresenceOrientationController::OnTabletPhysicalStateChanged() {
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  DCHECK(tablet_mode_controller);
  const bool physical_tablet_state =
      tablet_mode_controller->is_in_tablet_physical_state();
  UpdateOrientation(physical_tablet_state, display_rotated_, lid_closed_);
}

void HumanPresenceOrientationController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // We only care when the rotation of the in-built display changes.
  if (!display.IsInternal() ||
      !(changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)) {
    return;
  }

  const bool display_rotated = display.rotation() != display::Display::ROTATE_0;
  UpdateOrientation(physical_tablet_state_, display_rotated, lid_closed_);
}

void HumanPresenceOrientationController::LidEventReceived(
    chromeos::PowerManagerClient::LidState state,
    base::TimeTicks /* timestamp */) {
  const bool lid_closed = state != chromeos::PowerManagerClient::LidState::OPEN;
  UpdateOrientation(physical_tablet_state_, display_rotated_, lid_closed);
}

void HumanPresenceOrientationController::OnReceiveSwitchStates(
    std::optional<chromeos::PowerManagerClient::SwitchStates> switch_states) {
  if (switch_states.has_value()) {
    const bool lid_closed = switch_states->lid_state !=
                            chromeos::PowerManagerClient::LidState::OPEN;
    UpdateOrientation(physical_tablet_state_, display_rotated_, lid_closed);
  }
}

void HumanPresenceOrientationController::UpdateOrientation(
    bool physical_tablet_state,
    bool display_rotated,
    bool lid_closed) {
  const bool was_suitable = IsOrientationSuitable();

  physical_tablet_state_ = physical_tablet_state;
  display_rotated_ = display_rotated;
  lid_closed_ = lid_closed;

  const bool is_suitable = IsOrientationSuitable();

  if (was_suitable == is_suitable)
    return;

  for (Observer& observer : observers_)
    observer.OnOrientationChanged(is_suitable);
}

}  // namespace ash
