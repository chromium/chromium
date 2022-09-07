// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_EXPERIMENTAL_H_
#define ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_EXPERIMENTAL_H_

#include "ash/ash_export.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace ash {

// Implementation of TrayBluetoothHelper on top of the BluetoothSystem Mojo
// interface.
class TrayBluetoothHelperExperimental
    : public TrayBluetoothHelper,
      public device::mojom::BluetoothSystemClient {
 public:
  explicit TrayBluetoothHelperExperimental(
      mojo::PendingRemote<device::mojom::BluetoothSystemFactory>
          bluetooth_system_factory);

  TrayBluetoothHelperExperimental(const TrayBluetoothHelperExperimental&) =
      delete;
  TrayBluetoothHelperExperimental& operator=(
      const TrayBluetoothHelperExperimental&) = delete;

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
  mojo::Remote<device::mojom::BluetoothSystemFactory> bluetooth_system_factory_;
  mojo::Remote<device::mojom::BluetoothSystem> bluetooth_system_;
  mojo::Receiver<device::mojom::BluetoothSystemClient>
      bluetooth_system_client_receiver_{this};

  device::mojom::BluetoothSystem::State cached_state_ =
      device::mojom::BluetoothSystem::State::kUnavailable;
  device::mojom::BluetoothSystem::ScanState cached_scan_state_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_EXPERIMENTAL_H_
