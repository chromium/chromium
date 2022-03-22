// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/hps_sense_controller.h"

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
}

HpsSenseController::~HpsSenseController() {
  hps_observation_.Reset();
}

void HpsSenseController::EnableHpsSense() {
  // Only enable if HpsSense is not enabled yet.
  if (want_hps_sense_)
    return;
  want_hps_sense_ = true;

  // If hps_service is available then EnableHpsSense; otherwise, it will be
  // enabled when the service becomes available.
  if (service_available_)
    EnableHpsSenseViaDBus();
}

void HpsSenseController::DisableHpsSense() {
  // Only disable if HpsSense is enabled currently.
  if (!want_hps_sense_)
    return;
  want_hps_sense_ = false;

  // If hps_service is available then DisableHpsSense; otherwise, it will be
  // disabled when the service becomes available.
  if (service_available_)
    DisableHpsSenseViaDBus();
}

void HpsSenseController::OnHpsSenseChanged(hps::HpsResult state) {}

void HpsSenseController::OnHpsNotifyChanged(hps::HpsResult state) {}

void HpsSenseController::OnRestart() {
  service_available_ = true;

  // HpsDBusService just restarted, only need to send enabling signal.
  if (want_hps_sense_)
    EnableHpsSenseViaDBus();
}

void HpsSenseController::OnShutdown() {
  // HpsDBusService just stopped.
  service_available_ = false;
}

void HpsSenseController::OnHpsServiceAvailable(
    const bool service_is_available) {
  if (!service_is_available)
    return;
  service_available_ = true;

  // Always disable first, just in case the service was left enabled from
  // previous chrome session.
  DisableHpsSenseViaDBus();

  if (want_hps_sense_)
    EnableHpsSenseViaDBus();
}

}  // namespace ash
