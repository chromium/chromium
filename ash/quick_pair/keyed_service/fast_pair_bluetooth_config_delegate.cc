// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/keyed_service/fast_pair_bluetooth_config_delegate.h"

#include "ash/quick_pair/common/device.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "ash/quick_pair/repository/fast_pair_repository_impl.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ash/services/bluetooth_config/device_name_manager.h"
#include "chromeos/ash/services/bluetooth_config/fast_pair_delegate.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/device_image_info.h"
#include "url/gurl.h"

namespace ash {
namespace quick_pair {

FastPairBluetoothConfigDelegate::FastPairBluetoothConfigDelegate() = default;

FastPairBluetoothConfigDelegate::FastPairBluetoothConfigDelegate(
    Delegate* delegate)
    : delegate_(delegate) {}

FastPairBluetoothConfigDelegate::~FastPairBluetoothConfigDelegate() = default;

std::optional<bluetooth_config::DeviceImageInfo>
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

void FastPairBluetoothConfigDelegate::UpdateDeviceNickname(
    const std::string& mac_address,
    const std::string& nickname) {
  FastPairRepository::Get()->UpdateAssociatedDeviceFootprintsName(
      mac_address, nickname, /*cache_may_be_stale=*/true);
}

void FastPairBluetoothConfigDelegate::SetAdapterStateController(
    bluetooth_config::AdapterStateController* adapter_state_controller) {
  adapter_state_controller_ = adapter_state_controller;
  delegate_->OnAdapterStateControllerChanged(adapter_state_controller_);
}

void FastPairBluetoothConfigDelegate::SetDeviceNameManager(
    bluetooth_config::DeviceNameManager* device_name_manager) {
  device_name_manager_ = device_name_manager;
}

std::vector<bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>
FastPairBluetoothConfigDelegate::GetFastPairableDeviceProperties() {
  std::vector<bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr>
      fast_pairable_device_properties;
  for (const auto& paired_device : fast_pairable_device_properties_) {
    fast_pairable_device_properties.push_back(paired_device.Clone());
  }
  return fast_pairable_device_properties;
}

void FastPairBluetoothConfigDelegate::AddFastPairDevice(
    scoped_refptr<Device> device) {
  // TODO(b/291626339): Make this list alphabetical order.
  fast_pairable_devices_.push_back(device);
  fast_pairable_device_properties_.push_back(ConvertDeviceToProperties(
      device, bluetooth_config::mojom::FastPairableDevicePairingState::kReady));

  // The two lists should always be in sync.
  CHECK_EQ(fast_pairable_devices_.size(),
           fast_pairable_device_properties_.size());

  NotifyFastPairableDevicesChanged(fast_pairable_device_properties_);
}

void FastPairBluetoothConfigDelegate::RemoveFastPairDevice(
    scoped_refptr<Device> device) {
  auto it1 = fast_pairable_devices_.begin();
  auto it2 = fast_pairable_device_properties_.begin();
  while (it1 != fast_pairable_devices_.end()) {
    if (device->metadata_id() == (*it1)->metadata_id()) {
      // The two lists (fast_pairable_devices_ and
      // fast_pairable_device_properties_) are always in sync
      fast_pairable_devices_.erase(it1);
      fast_pairable_device_properties_.erase(it2);
      break;
    }
    ++it1;
    ++it2;
  }

  // The two lists should always be in sync.
  CHECK_EQ(fast_pairable_devices_.size(),
           fast_pairable_device_properties_.size());

  NotifyFastPairableDevicesChanged(fast_pairable_device_properties_);
}

void FastPairBluetoothConfigDelegate::UpdateFastPairableDevicePairingState(
    scoped_refptr<Device> device,
    bluetooth_config::mojom::FastPairableDevicePairingState pairing_state) {
  auto it = fast_pairable_device_properties_.begin();
  while (it != fast_pairable_device_properties_.end()) {
    if (device->ble_address() == (*it)->device_properties->address) {
      (*it)->fast_pairable_device_pairing_state = pairing_state;
      break;
    }
    ++it;
  }

  // The two lists should always be in sync.
  CHECK_EQ(fast_pairable_devices_.size(),
           fast_pairable_device_properties_.size());

  NotifyFastPairableDevicesChanged(fast_pairable_device_properties_);
}

void FastPairBluetoothConfigDelegate::ClearFastPairableDevices() {
  fast_pairable_devices_.clear();
  fast_pairable_device_properties_.clear();

  // The two lists should always be in sync.
  CHECK_EQ(fast_pairable_devices_.size(),
           fast_pairable_device_properties_.size());

  NotifyFastPairableDevicesChanged(fast_pairable_device_properties_);
}

bluetooth_config::mojom::PairedBluetoothDevicePropertiesPtr
FastPairBluetoothConfigDelegate::ConvertDeviceToProperties(
    scoped_refptr<Device> device,
    bluetooth_config::mojom::FastPairableDevicePairingState pairing_state) {
  auto bluetooth_device_properties =
      bluetooth_config::mojom::BluetoothDeviceProperties::New();

  // Using the ble_address since Subsequent Pair Devices don't have a classic
  // address until pairing begins.
  bluetooth_device_properties->address = device->ble_address();

  auto images = GetDeviceImageInfo(device->ble_address());

  bluetooth_config::mojom::DeviceImageInfoPtr device_image_info =
      bluetooth_config::mojom::DeviceImageInfo::New();
  if (!images.has_value()) {
    bluetooth_device_properties->image_info = nullptr;
  } else if (!images->default_image().empty()) {
    GURL default_image_url = GURL(images->default_image());
    DCHECK(default_image_url.is_valid() &&
           default_image_url.SchemeIs(url::kDataScheme));
    device_image_info->default_image_url = default_image_url;
    bluetooth_device_properties->image_info = std::move(device_image_info);
  } else {
    bluetooth_device_properties->image_info = nullptr;
  }

  auto properties =
      bluetooth_config::mojom::PairedBluetoothDeviceProperties::New();
  if (device->display_name().has_value()) {
    properties->nickname = device->display_name();
  }
  properties->fast_pairable_device_pairing_state = pairing_state;
  properties->device_properties = std::move(bluetooth_device_properties);

  return properties;
}

}  // namespace quick_pair
}  // namespace ash
