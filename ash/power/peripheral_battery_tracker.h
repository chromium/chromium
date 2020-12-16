// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POWER_PERIPHERAL_BATTERY_TRACKER_H_
#define ASH_POWER_PERIPHERAL_BATTERY_TRACKER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"

namespace device {
class BluetoothAdapter;
}  // namespace device

namespace ash {

class HfpBatteryListener;
class HidBatteryListener;

// Creates instances of classes to collect the battery status of peripheral
// devices. Currently only tracks Bluetooth devices that support GATT, HFP or
// HID.
class ASH_EXPORT PeripheralBatteryTracker {
 public:
  PeripheralBatteryTracker();
  ~PeripheralBatteryTracker();

 private:
  void InitializeOnBluetoothReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  scoped_refptr<device::BluetoothAdapter> adapter_;

  std::unique_ptr<HidBatteryListener> hid_battery_listener_;

  base::WeakPtrFactory<PeripheralBatteryTracker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PeripheralBatteryTracker);
};

}  // namespace ash

#endif  // ASH_POWER_PERIPHERAL_BATTERY_TRACKER_H_
