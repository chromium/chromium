// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/hid_battery_listener.h"

#include "ash/power/hid_battery_util.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace ash {

HidBatteryListener::HidBatteryListener(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(adapter) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
}

HidBatteryListener::~HidBatteryListener() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void HidBatteryListener::PeripheralBatteryStatusReceived(
    const std::string& path,
    const std::string& name,
    int level) {
  const std::string bluetooth_address =
      ExtractBluetoothAddressFromHIDBatteryPath(path);
  if (bluetooth_address.empty())
    return;

  device::BluetoothDevice* device = adapter_->GetDevice(bluetooth_address);
  if (!device)
    return;

  if (level < 0 || level > 100)
    device->SetBatteryPercentage(base::nullopt);
  else
    device->SetBatteryPercentage(level);
}

}  // namespace ash
