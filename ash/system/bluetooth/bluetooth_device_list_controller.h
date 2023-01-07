// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"

namespace ash {

class BluetoothDetailedView;

// This class defines the interface used to add, modify, and remove devices from
// the device list of the detailed Bluetooth device page within the quick
// settings. This class includes the definition of the factory used to create
// instances of implementations of this class.
class ASH_EXPORT BluetoothDeviceListController {
 public:
  using PairedBluetoothDevicePropertiesPtrs =
      std::vector<bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>;

  class Factory {
   public:
    Factory(const Factory&) = delete;
    const Factory& operator=(const Factory&) = delete;
    virtual ~Factory() = default;

    static std::unique_ptr<BluetoothDeviceListController> Create(
        BluetoothDetailedView* bluetooth_detailed_view);
    static void SetFactoryForTesting(Factory* test_factory);

   protected:
    Factory() = default;

    virtual std::unique_ptr<BluetoothDeviceListController>
    CreateForTesting() = 0;
  };

  BluetoothDeviceListController(const BluetoothDeviceListController&) = delete;
  BluetoothDeviceListController& operator=(
      const BluetoothDeviceListController&) = delete;
  virtual ~BluetoothDeviceListController() = default;

  // Clears the detailed view device list when |enabled| is |false|.
  virtual void UpdateBluetoothEnabledState(bool enabled) = 0;

  // Updates the devices shown in the detailed view device list so long as
  // |is_bluetooth_enabled_| is |true|.
  virtual void UpdateDeviceList(
      const PairedBluetoothDevicePropertiesPtrs& connected,
      const PairedBluetoothDevicePropertiesPtrs& previously_connected) = 0;

 protected:
  BluetoothDeviceListController() = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_DEVICE_LIST_CONTROLLER_H_
