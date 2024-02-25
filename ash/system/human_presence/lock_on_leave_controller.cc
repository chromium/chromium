// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/human_presence/lock_on_leave_controller.h"

#include <optional>

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/dbus/human_presence/human_presence_dbus_client.h"
#include "chromeos/ash/components/human_presence/human_presence_configuration.h"

namespace ash {
namespace {

// Helper for EnableHpsSense.
void EnableLockOnLeaveViaDBus() {
  const auto config = hps::GetEnableLockOnLeaveConfig();
  if (config.has_value()) {
    HumanPresenceDBusClient::Get()->EnableHpsSense(config.value());
    LOG(ERROR) << "LockOnLeaveController: enabling HpsSense from chrome.";
  } else {
    LOG(ERROR)
        << "LockOnLeaveController: couldn't parse HpsSense configuration.";
  }
}

// Helper for DisableHpsSense.
void DisableLockOnLeaveViaDBus() {
  HumanPresenceDBusClient::Get()->DisableHpsSense();
}

}  // namespace

LockOnLeaveController::LockOnLeaveController() {
  human_presence_observation_.Observe(HumanPresenceDBusClient::Get());

  HumanPresenceDBusClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&LockOnLeaveController::OnServiceAvailable,
                     weak_ptr_factory_.GetWeakPtr()));

  // Orientation controller is instantiated before us in the shell.
  HumanPresenceOrientationController* orientation_controller =
      Shell::Get()->human_presence_orientation_controller();
  suitable_for_human_presence_ =
      orientation_controller->IsOrientationSuitable();
  orientation_observation_.Observe(orientation_controller);
}

LockOnLeaveController::~LockOnLeaveController() {
  human_presence_observation_.Reset();
  orientation_observation_.Reset();
}

void LockOnLeaveController::EnableLockOnLeave() {
  want_lock_on_leave_ = true;
  ReconfigViaDbus();
}

void LockOnLeaveController::DisableLockOnLeave() {
  want_lock_on_leave_ = false;
  ReconfigViaDbus();
}

// HumanPresenceOrientationObserver:
void LockOnLeaveController::OnOrientationChanged(
    bool suitable_for_human_presence) {
  suitable_for_human_presence_ = suitable_for_human_presence;
  ReconfigViaDbus();
}

void LockOnLeaveController::OnHpsSenseChanged(const hps::HpsResultProto&) {}

void LockOnLeaveController::OnHpsNotifyChanged(const hps::HpsResultProto&) {}

void LockOnLeaveController::OnRestart() {
  service_available_ = true;
  ReconfigViaDbus();
}

void LockOnLeaveController::OnShutdown() {
  // The service just stopped.
  service_available_ = false;
  configured_state_ = ConfiguredLockOnLeaveState::kDisabled;
}

void LockOnLeaveController::OnServiceAvailable(const bool service_available) {
  service_available_ = service_available;
  ReconfigViaDbus();
}

void LockOnLeaveController::ReconfigViaDbus() {
  if (!service_available_)
    return;

  // When chrome starts, it does not know the current configured_state_, because
  // it could be left enabled from previous chrome session, disable it so that
  // the new configuration can apply.
  if (configured_state_ == ConfiguredLockOnLeaveState::kUnknown) {
    DisableLockOnLeaveViaDBus();
    configured_state_ = ConfiguredLockOnLeaveState::kDisabled;
  }

  // Wanted state should be either kEnabled or kDisabled.
  const ConfiguredLockOnLeaveState wanted_state =
      want_lock_on_leave_ && suitable_for_human_presence_
          ? ConfiguredLockOnLeaveState::kEnabled
          : ConfiguredLockOnLeaveState::kDisabled;

  // Return if already configured to the wanted state.
  if (wanted_state == configured_state_)
    return;

  if (wanted_state == ConfiguredLockOnLeaveState::kEnabled) {
    EnableLockOnLeaveViaDBus();
  } else {
    DisableLockOnLeaveViaDBus();
  }
  configured_state_ = wanted_state;
}
}  // namespace ash
