// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_EXPERIMENTAL_H_
#define ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_EXPERIMENTAL_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace ash {

// Implementation of TrayBluetoothHelper on top of the BluetoothSystem Mojo
// interface.
class TrayBluetoothHelperExperimental
    : public TrayBluetoothHelper,
      public device::mojom::BluetoothSystemClient {
 public:
  explicit TrayBluetoothHelperExperimental(
      service_manager::Connector* connector);
  ~TrayBluetoothHelperExperimental() override;

  // TrayBluetoothHelper:
  void Initialize() override;
  void StartBluetoothDiscovering() override;
  void StopBluetoothDiscovering() override;
  void ConnectToBluetoothDevice(const BluetoothAddress& address) override;
  device::mojom::BluetoothSystem::State GetBluetoothState() override;
  void SetBluetoothEnabled(bool enabled) override;
  bool HasBluetoothDiscoverySession() override;
  void GetBluetoothDevices(GetBluetoothDevicesCallback callback) const override;

  // device::mojom::BluetoothSystemClient
  void OnStateChanged(device::mojom::BluetoothSystem::State state) override;
  void OnScanStateChanged(
      device::mojom::BluetoothSystem::ScanState state) override;

 private:
  service_manager::Connector* connector_;

  mojo::Remote<device::mojom::BluetoothSystem> bluetooth_system_;
  mojo::Receiver<device::mojom::BluetoothSystemClient>
      bluetooth_system_client_receiver_{this};

  device::mojom::BluetoothSystem::State cached_state_ =
      device::mojom::BluetoothSystem::State::kUnavailable;
  device::mojom::BluetoothSystem::ScanState cached_scan_state_;

  DISALLOW_COPY_AND_ASSIGN(TrayBluetoothHelperExperimental);
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_EXPERIMENTAL_H_
