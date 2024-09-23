// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/cec_private_ash.h"

#include "chromeos/ash/components/dbus/cec_service/cec_service_client.h"

using crosapi::mojom::PowerState;

namespace {

PowerState ConvertCecServiceClientPowerState(
    ash::CecServiceClient::PowerState power_state) {
  switch (power_state) {
    case ash::CecServiceClient::PowerState::kError:
      return PowerState::kError;
    case ash::CecServiceClient::PowerState::kAdapterNotConfigured:
      return PowerState::kAdapterNotConfigured;
    case ash::CecServiceClient::PowerState::kNoDevice:
      return PowerState::kNoDevice;
    case ash::CecServiceClient::PowerState::kOn:
      return PowerState::kOn;
    case ash::CecServiceClient::PowerState::kStandBy:
      return PowerState::kStandBy;
    case ash::CecServiceClient::PowerState::kTransitioningToOn:
      return PowerState::kTransitioningToOn;
    case ash::CecServiceClient::PowerState::kTransitioningToStandBy:
      return PowerState::kTransitioningToStandBy;
    case ash::CecServiceClient::PowerState::kUnknown:
      return PowerState::kUnknown;
  }

  NOTREACHED();
}

}  // namespace

namespace crosapi {

CecPrivateAsh::CecPrivateAsh() = default;
CecPrivateAsh::~CecPrivateAsh() = default;

void CecPrivateAsh::BindReceiver(
    mojo::PendingReceiver<mojom::CecPrivate> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void CecPrivateAsh::SendStandBy(SendStandByCallback callback) {
  ash::CecServiceClient* dbus_client = ash::CecServiceClient::Get();
  if (!dbus_client) {
    LOG(WARNING)
        << "CecPrivate crosapi invoked before dbus client became available.";
  } else {
    dbus_client->SendStandBy();
  }
  std::move(callback).Run();
}

void CecPrivateAsh::SendWakeUp(SendWakeUpCallback callback) {
  ash::CecServiceClient* dbus_client = ash::CecServiceClient::Get();
  if (!dbus_client) {
    LOG(WARNING)
        << "CecPrivate crosapi invoked before dbus client became available.";
  } else {
    dbus_client->SendWakeUp();
  }
  std::move(callback).Run();
}

void CecPrivateAsh::QueryDisplayCecPowerState(
    QueryDisplayCecPowerStateCallback callback) {
  ash::CecServiceClient* dbus_client = ash::CecServiceClient::Get();
  if (!dbus_client) {
    LOG(WARNING)
        << "CecPrivate crosapi invoked before dbus client became available.";
    std::move(callback).Run({});
    return;
  }
  dbus_client->QueryDisplayCecPowerState(
      base::BindOnce([](const std::vector<ash::CecServiceClient::PowerState>&
                            power_states) {
        std::vector<PowerState> converted_states;
        for (const ash::CecServiceClient::PowerState& state : power_states) {
          converted_states.push_back(ConvertCecServiceClientPowerState(state));
        }
        return converted_states;
      }).Then(std::move(callback)));
}

}  // namespace crosapi
