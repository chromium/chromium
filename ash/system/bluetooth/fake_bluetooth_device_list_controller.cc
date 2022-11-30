// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/fake_bluetooth_device_list_controller.h"

namespace ash {

FakeBluetoothDeviceListController::FakeBluetoothDeviceListController() =
    default;

FakeBluetoothDeviceListController::~FakeBluetoothDeviceListController() =
    default;

void FakeBluetoothDeviceListController::UpdateBluetoothEnabledState(
    bool enabled) {
  last_bluetooth_enabled_state_ = enabled;
}

void FakeBluetoothDeviceListController::UpdateDeviceList(
    const PairedBluetoothDevicePropertiesPtrs& connected,
    const PairedBluetoothDevicePropertiesPtrs& previously_connected) {
  connected_devices_count_ = connected.size();
  previously_connected_devices_count_ = previously_connected.size();
}

}  // namespace ash
