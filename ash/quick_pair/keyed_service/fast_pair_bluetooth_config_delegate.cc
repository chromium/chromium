// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/fast_pair_bluetooth_config_delegate.h"

#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/services/bluetooth_config/device_name_manager.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"

namespace ash {
namespace quick_pair {

FastPairBluetoothConfigDelegate::FastPairBluetoothConfigDelegate() = default;

FastPairBluetoothConfigDelegate::~FastPairBluetoothConfigDelegate() = default;

void FastPairBluetoothConfigDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FastPairBluetoothConfigDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

absl::optional<bluetooth_config::DeviceImageInfo>
FastPairBluetoothConfigDelegate::GetDeviceImageInfo(
    const std::string& mac_address) {
  return FastPairRepository::Get()->GetImagesForDevice(mac_address);
}

void FastPairBluetoothConfigDelegate::ForgetDevice(
    const std::string& mac_address) {
  FastPairRepository::Get()->DeleteAssociatedDevice(mac_address,
                                                    base::DoNothing());
  FastPairRepository::Get()->EvictDeviceImages(mac_address);
}

void FastPairBluetoothConfigDelegate::SetAdapterStateController(
    bluetooth_config::AdapterStateController* adapter_state_controller) {
  adapter_state_controller_ = adapter_state_controller;
  for (auto& observer : observers_) {
    observer.OnAdapterStateControllerChanged(adapter_state_controller_);
  }
}

void FastPairBluetoothConfigDelegate::SetDeviceNameManager(
    bluetooth_config::DeviceNameManager* device_name_manager) {
  device_name_manager_ = device_name_manager;
}

}  // namespace quick_pair
}  // namespace ash
