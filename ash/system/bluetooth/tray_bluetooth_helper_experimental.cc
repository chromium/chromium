// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth_helper_experimental.h"

#include <string>
#include <utility>

#include "ash/shell.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"

// base::Unretained():
//
// Usage of `base::Unretained(this)` is safe when calling BluetoothSystemPtr
// methods because BluetoothSystemPtr is owned by `this` and guarantees that no
// callbacks will be run after its destruction.

namespace ash {

TrayBluetoothHelperExperimental::TrayBluetoothHelperExperimental(
    service_manager::Connector* connector)
    : connector_(connector) {}

TrayBluetoothHelperExperimental::~TrayBluetoothHelperExperimental() = default;

void TrayBluetoothHelperExperimental::Initialize() {
  device::mojom::BluetoothSystemFactoryPtr bluetooth_system_factory;
  connector_->BindInterface(device::mojom::kServiceName,
                            &bluetooth_system_factory);

  device::mojom::BluetoothSystemClientPtr client_ptr;
  bluetooth_system_client_binding_.Bind(mojo::MakeRequest(&client_ptr));

  bluetooth_system_factory->Create(mojo::MakeRequest(&bluetooth_system_ptr_),
                                   std::move(client_ptr));
  bluetooth_system_ptr_->GetState(
      base::BindOnce(&TrayBluetoothHelperExperimental::OnStateChanged,
                     // See base::Unretained() note at the top.
                     base::Unretained(this)));
}

BluetoothDeviceList
TrayBluetoothHelperExperimental::GetAvailableBluetoothDevices() const {
  NOTIMPLEMENTED();
  return BluetoothDeviceList();
}

void TrayBluetoothHelperExperimental::StartBluetoothDiscovering() {
  NOTIMPLEMENTED();
}

void TrayBluetoothHelperExperimental::StopBluetoothDiscovering() {
  NOTIMPLEMENTED();
}

void TrayBluetoothHelperExperimental::ConnectToBluetoothDevice(
    const std::string& address) {
  NOTIMPLEMENTED();
}

device::mojom::BluetoothSystem::State
TrayBluetoothHelperExperimental::GetBluetoothState() {
  return cached_state_;
}

void TrayBluetoothHelperExperimental::SetBluetoothEnabled(bool enabled) {
  bluetooth_system_ptr_->SetPowered(enabled, base::DoNothing());
}

bool TrayBluetoothHelperExperimental::HasBluetoothDiscoverySession() {
  NOTIMPLEMENTED();
  return false;
}

void TrayBluetoothHelperExperimental::OnStateChanged(
    device::mojom::BluetoothSystem::State state) {
  cached_state_ = state;
  Shell::Get()->system_tray_notifier()->NotifyRefreshBluetooth();
}

void TrayBluetoothHelperExperimental::OnScanStateChanged(
    device::mojom::BluetoothSystem::ScanState state) {
  Shell::Get()->system_tray_notifier()->NotifyBluetoothDiscoveringChanged();
}

}  // namespace ash
