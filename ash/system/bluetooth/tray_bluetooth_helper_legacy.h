// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_LEGACY_H_
#define ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_LEGACY_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/bluetooth/tray_bluetooth_helper.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "services/device/public/mojom/bluetooth_system.mojom.h"

namespace device {
class BluetoothDiscoverySession;
}

namespace ash {

// Implementation of TrayBluetoothHelper on top of the //device/bluetooth/ APIs.
// Exported for tests.
class ASH_EXPORT TrayBluetoothHelperLegacy
    : public TrayBluetoothHelper,
      public device::BluetoothAdapter::Observer {
 public:
  TrayBluetoothHelperLegacy();
  ~TrayBluetoothHelperLegacy() override;

  // Completes initialization after the Bluetooth adapter is ready.
  void InitializeOnAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // TrayBluetoothHelper:
  void Initialize() override;
  BluetoothDeviceList GetAvailableBluetoothDevices() const override;
  void StartBluetoothDiscovering() override;
  void StopBluetoothDiscovering() override;
  void ConnectToBluetoothDevice(const std::string& address) override;
  device::mojom::BluetoothSystem::State GetBluetoothState() override;
  void SetBluetoothEnabled(bool enabled) override;
  bool HasBluetoothDiscoverySession() override;

  // BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                 bool discovering) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

 private:
  void OnStartDiscoverySession(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);

  bool should_run_discovery_ = false;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // Object could be deleted during a prolonged Bluetooth operation.
  base::WeakPtrFactory<TrayBluetoothHelperLegacy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TrayBluetoothHelperLegacy);
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_TRAY_BLUETOOTH_HELPER_LEGACY_H_
