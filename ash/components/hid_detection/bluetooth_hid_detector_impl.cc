// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/bluetooth_hid_detector_impl.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "components/device_event_log/device_event_log.h"

using chromeos::bluetooth_config::mojom::BluetoothSystemState;

namespace ash {
namespace hid_detection {

BluetoothHidDetectorImpl::BluetoothHidDetectorImpl() = default;

BluetoothHidDetectorImpl::~BluetoothHidDetectorImpl() {
  DCHECK_EQ(kNotStarted, state_) << "HID detection must be stopped before "
                                 << "BluetoothHidDetectorImpl is destroyed.";
}

void BluetoothHidDetectorImpl::StartBluetoothHidDetection() {
  DCHECK_EQ(kNotStarted, state_);
  HID_LOG(EVENT) << "Starting Bluetooth HID detection";
  state_ = kStarting;
  GetBluetoothConfigService(
      cros_bluetooth_config_remote_.BindNewPipeAndPassReceiver());
  cros_bluetooth_config_remote_->ObserveSystemProperties(
      system_properties_observer_receiver_.BindNewPipeAndPassRemote());
}

void BluetoothHidDetectorImpl::StopBluetoothHidDetection() {
  DCHECK_NE(kNotStarted, state_) << "Call to StopBluetoothHidDetection() while "
                                 << "HID detection is inactive.";
  HID_LOG(EVENT) << "Stopping Bluetooth HID detection";
  state_ = kNotStarted;
  cros_bluetooth_config_remote_->SetBluetoothHidDetectionActive(false);
  cros_bluetooth_config_remote_.reset();
  system_properties_observer_receiver_.reset();
  bluetooth_discovery_delegate_receiver_.reset();
}

void BluetoothHidDetectorImpl::OnPropertiesUpdated(
    chromeos::bluetooth_config::mojom::BluetoothSystemPropertiesPtr
        properties) {
  switch (state_) {
    case kNotStarted:
      NOTREACHED() << "SystemPropertiesObserver should not be bound while in "
                      "state |kNotStarted|";
      return;
    case kStarting:
      if (properties->system_state == BluetoothSystemState::kEnabled) {
        HID_LOG(EVENT)
            << "Bluetooth adapter is already enabled, starting discovery";
        state_ = kDetecting;
        cros_bluetooth_config_remote_->StartDiscovery(
            bluetooth_discovery_delegate_receiver_.BindNewPipeAndPassRemote());
      } else if (properties->system_state == BluetoothSystemState::kDisabled ||
                 properties->system_state == BluetoothSystemState::kDisabling) {
        HID_LOG(EVENT) << "Bluetooth adapter is disabled or disabling, "
                       << "enabling adapter";
        state_ = kEnablingAdapter;
        cros_bluetooth_config_remote_->SetBluetoothHidDetectionActive(true);
      } else {
        HID_LOG(EVENT)
            << "Bluetooth adapter is unavailable or enabling, waiting "
            << "for next state change";
      }
      return;
    case kEnablingAdapter:
      if (properties->system_state == BluetoothSystemState::kEnabled) {
        HID_LOG(EVENT)
            << "Bluetooth adapter has become enabled, starting discovery";
        state_ = kDetecting;
        cros_bluetooth_config_remote_->StartDiscovery(
            bluetooth_discovery_delegate_receiver_.BindNewPipeAndPassRemote());
      }
      return;
    case kDetecting:
      if (properties->system_state != BluetoothSystemState::kEnabled) {
        HID_LOG(EVENT) << "Bluetooth adapter has stopped being enabled while "
                       << "Bluetooth HID detection is in progress";
        state_ = kStoppedExternally;
        bluetooth_discovery_delegate_receiver_.reset();
      }
      return;
    case kStoppedExternally:
      if (properties->system_state == BluetoothSystemState::kEnabled) {
        HID_LOG(EVENT) << "Bluetooth adapter has become enabled after being "
                       << "unenabled externally, starting discovery";
        state_ = kDetecting;
        cros_bluetooth_config_remote_->StartDiscovery(
            bluetooth_discovery_delegate_receiver_.BindNewPipeAndPassRemote());
      }
      return;
  }
}

void BluetoothHidDetectorImpl::OnBluetoothDiscoveryStarted(
    mojo::PendingRemote<chromeos::bluetooth_config::mojom::DevicePairingHandler>
        handler) {
  // TODO(crbug.com/1299099): Implement pairing.
}

void BluetoothHidDetectorImpl::OnBluetoothDiscoveryStopped() {}

void BluetoothHidDetectorImpl::OnDiscoveredDevicesListChanged(
    std::vector<chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr>
        discovered_devices) {
  // TODO(crbug.com/1299099): Implement pairing.
}

}  // namespace hid_detection
}  // namespace ash
