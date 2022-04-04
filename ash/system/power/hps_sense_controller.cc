// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/hps_sense_controller.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "chromeos/components/hps/hps_configuration.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

// Helper for EnableHpsSense.
void EnableHpsSenseViaDBus() {
  const auto config = hps::GetEnableHpsSenseConfig();
  if (config.has_value()) {
    chromeos::HpsDBusClient::Get()->EnableHpsSense(config.value());
  }
}

// Helper for DisableHpsSense.
void DisableHpsSenseViaDBus() {
  chromeos::HpsDBusClient::Get()->DisableHpsSense();
}

}  // namespace

HpsSenseController::HpsSenseController() {
  hps_observation_.Observe(chromeos::HpsDBusClient::Get());

  chromeos::HpsDBusClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&HpsSenseController::OnHpsServiceAvailable,
                     weak_ptr_factory_.GetWeakPtr()));

  // Orientation controller is instantiated before us in the shell.
  HpsOrientationController* orientation_controller =
      Shell::Get()->hps_orientation_controller();
  suitable_for_hps_ = orientation_controller->IsOrientationSuitable();
  orientation_observation_.Observe(orientation_controller);
}

HpsSenseController::~HpsSenseController() {
  hps_observation_.Reset();
  orientation_observation_.Reset();
}

void HpsSenseController::EnableHpsSense() {
  want_hps_sense_ = true;
  ReconfigViaDbus();
}

void HpsSenseController::DisableHpsSense() {
  want_hps_sense_ = false;
  ReconfigViaDbus();
}

// HpsOrientationObserver:
void HpsSenseController::OnOrientationChanged(bool suitable_for_hps) {
  suitable_for_hps_ = suitable_for_hps;
  ReconfigViaDbus();
}

void HpsSenseController::OnHpsSenseChanged(hps::HpsResult state) {}

void HpsSenseController::OnHpsNotifyChanged(hps::HpsResult state) {}

void HpsSenseController::OnRestart() {
  service_available_ = true;
  ReconfigViaDbus();
}

void HpsSenseController::OnShutdown() {
  // HpsDBusService just stopped.
  service_available_ = false;
  configured_state_ = ConfiguredHpsSenseState::kDisabled;
}

void HpsSenseController::OnHpsServiceAvailable(const bool service_available) {
  service_available_ = service_available;
  ReconfigViaDbus();
}

void HpsSenseController::ReconfigViaDbus() {
  if (!service_available_)
    return;

  // When chrome starts, it does not know the current configured_state_, because
  // it could be left enabled from previous chrome session, disable it so that
  // the new configuration can apply.
  if (configured_state_ == ConfiguredHpsSenseState::kUnknown) {
    DisableHpsSenseViaDBus();
    configured_state_ = ConfiguredHpsSenseState::kDisabled;
  }

  // Wanted state should be either kEnabled or kDisabled.
  const ConfiguredHpsSenseState wanted_state =
      want_hps_sense_ && suitable_for_hps_ ? ConfiguredHpsSenseState::kEnabled
                                           : ConfiguredHpsSenseState::kDisabled;

  // Return if already configured to the wanted state.
  if (wanted_state == configured_state_)
    return;

  if (wanted_state == ConfiguredHpsSenseState::kEnabled) {
    EnableHpsSenseViaDBus();
  } else {
    DisableHpsSenseViaDBus();
  }
  configured_state_ = wanted_state;
}
}  // namespace ash
