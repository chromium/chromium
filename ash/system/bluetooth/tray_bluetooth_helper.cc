// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/tray_bluetooth_helper.h"

using device::mojom::BluetoothSystem;

namespace ash {

BluetoothDeviceInfo::BluetoothDeviceInfo() = default;

BluetoothDeviceInfo::BluetoothDeviceInfo(const BluetoothDeviceInfo& other) =
    default;

BluetoothDeviceInfo::~BluetoothDeviceInfo() = default;

TrayBluetoothHelper::TrayBluetoothHelper() = default;

TrayBluetoothHelper::~TrayBluetoothHelper() = default;

bool TrayBluetoothHelper::IsBluetoothStateAvailable() {
  switch (GetBluetoothState()) {
    case BluetoothSystem::State::kUnsupported:
    case BluetoothSystem::State::kUnavailable:
      return false;
    case BluetoothSystem::State::kPoweredOff:
    case BluetoothSystem::State::kTransitioning:
    case BluetoothSystem::State::kPoweredOn:
      return true;
  }
}

}  // namespace ash
