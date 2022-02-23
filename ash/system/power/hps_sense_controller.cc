// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/hps_sense_controller.h"

#include "ash/system/hps/hps_configuration.h"
#include "base/bind.h"
#include "chromeos/dbus/hps/hps_dbus_client.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

HpsSenseController::HpsSenseController() {
  hps_observation_.Observe(chromeos::HpsDBusClient::Get());
}

HpsSenseController::~HpsSenseController() {
  hps_observation_.Reset();
}

void HpsSenseController::EnableHpsSense() {
  // Only enable if HpsSense is not enabled yet.
  if (is_hps_sense_enabled || !ash::GetEnableHpsSenseConfig().has_value()) {
    return;
  }
  chromeos::HpsDBusClient::Get()->WaitForServiceToBeAvailable(
      base::BindOnce(&HpsSenseController::OnHpsServiceAvailable,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HpsSenseController::DisableHpsSense() {
  // Only disable if HpsSense is enabled currently.
  if (!is_hps_sense_enabled)
    return;

  chromeos::HpsDBusClient::Get()->DisableHpsSense();
  is_hps_sense_enabled = false;
}

void HpsSenseController::OnHpsNotifyChanged(hps::HpsResult state) {}

void HpsSenseController::OnRestart() {
  // Only Re-enable HpsSense if it was enabled before restarted.
  if (!is_hps_sense_enabled)
    return;

  chromeos::HpsDBusClient::Get()->EnableHpsSense(
      ash::GetEnableHpsSenseConfig().value());
}

void HpsSenseController::OnShutdown() {}

void HpsSenseController::OnHpsServiceAvailable(
    const bool service_is_available) {
  if (!service_is_available)
    return;

  chromeos::HpsDBusClient::Get()->EnableHpsSense(
      ash::GetEnableHpsSenseConfig().value());
  is_hps_sense_enabled = true;
}

}  // namespace ash
