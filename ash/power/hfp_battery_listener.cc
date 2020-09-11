// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/power/hfp_battery_listener.h"

#include "device/bluetooth/bluetooth_device.h"

namespace ash {

HfpBatteryListener::HfpBatteryListener(
    scoped_refptr<device::BluetoothAdapter> adapter)
    : adapter_(adapter) {
  DCHECK(adapter);
  chromeos::CrasAudioHandler::Get()->AddAudioObserver(this);
  adapter_->AddObserver(this);

  // We may be late for DeviceAdded notifications. So for the already added
  // devices, simulate DeviceAdded events.
  for (auto* const device : adapter_->GetDevices())
    DeviceAdded(adapter_.get(), device);
}

HfpBatteryListener::~HfpBatteryListener() {
  chromeos::CrasAudioHandler::Get()->RemoveAudioObserver(this);
  adapter_->RemoveObserver(this);
}

void HfpBatteryListener::OnBluetoothBatteryChanged(const std::string& address,
                                                   uint32_t level) {
  device::BluetoothDevice* device = adapter_->GetDevice(address);
  if (!device)
    return;

  if (level > 100)
    device->SetBatteryPercentage(base::nullopt);
  else
    device->SetBatteryPercentage(level);
}

void HfpBatteryListener::DeviceAdded(device::BluetoothAdapter* adapter,
                                     device::BluetoothDevice* device) {
  chromeos::CrasAudioHandler::Get()->ResendBluetoothBattery();
}

}  // namespace ash
