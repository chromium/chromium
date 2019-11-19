// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth_helper_experimental.h"

#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"

// base::Unretained():
//
// Usage of `base::Unretained(this)` is safe when calling
// mojo::Remote<BluetoothSystem> methods because mojo::Remote<BluetoothSystem>
// is owned by `this` and guarantees that no callbacks will be run after its
// destruction.

namespace ash {

TrayBluetoothHelperExperimental::TrayBluetoothHelperExperimental(
    service_manager::Connector* connector)
    : connector_(connector) {}

TrayBluetoothHelperExperimental::~TrayBluetoothHelperExperimental() = default;

void TrayBluetoothHelperExperimental::Initialize() {
  mojo::Remote<device::mojom::BluetoothSystemFactory> bluetooth_system_factory;
  connector_->Connect(device::mojom::kServiceName,
                      bluetooth_system_factory.BindNewPipeAndPassReceiver());

  bluetooth_system_factory->Create(
      bluetooth_system_.BindNewPipeAndPassReceiver(),
      bluetooth_system_client_receiver_.BindNewPipeAndPassRemote());
  bluetooth_system_->GetState(
      base::BindOnce(&TrayBluetoothHelperExperimental::OnStateChanged,
                     // See base::Unretained() note at the top.
                     base::Unretained(this)));
  bluetooth_system_->GetScanState(
      base::BindOnce(&TrayBluetoothHelperExperimental::OnScanStateChanged,
                     // See base::Unretained() note at the top.
                     base::Unretained(this)));
}

void TrayBluetoothHelperExperimental::StartBluetoothDiscovering() {
  bluetooth_system_->StartScan(base::NullCallback());
}

void TrayBluetoothHelperExperimental::StopBluetoothDiscovering() {
  bluetooth_system_->StopScan(base::NullCallback());
}

void TrayBluetoothHelperExperimental::ConnectToBluetoothDevice(
    const BluetoothAddress& address) {
  NOTIMPLEMENTED();
}

device::mojom::BluetoothSystem::State
TrayBluetoothHelperExperimental::GetBluetoothState() {
  return cached_state_;
}

void TrayBluetoothHelperExperimental::SetBluetoothEnabled(bool enabled) {
  bluetooth_system_->SetPowered(enabled, base::NullCallback());
}

bool TrayBluetoothHelperExperimental::HasBluetoothDiscoverySession() {
  return cached_scan_state_ ==
         device::mojom::BluetoothSystem::ScanState::kScanning;
}

void TrayBluetoothHelperExperimental::GetBluetoothDevices(
    GetBluetoothDevicesCallback callback) const {
  bluetooth_system_->GetAvailableDevices(std::move(callback));
}

void TrayBluetoothHelperExperimental::OnStateChanged(
    device::mojom::BluetoothSystem::State state) {
  cached_state_ = state;

  NotifyBluetoothSystemStateChanged();
  StartOrStopRefreshingDeviceList();
}

void TrayBluetoothHelperExperimental::OnScanStateChanged(
    device::mojom::BluetoothSystem::ScanState state) {
  cached_scan_state_ = state;
  NotifyBluetoothScanStateChanged();
}

}  // namespace ash
