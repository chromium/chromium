// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/bluetooth_hid_detector_impl.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "base/strings/utf_string_conversions.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {
namespace hid_detection {
namespace {

using chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr;
using chromeos::bluetooth_config::mojom::BluetoothSystemState;
using chromeos::bluetooth_config::mojom::DeviceType;
using chromeos::bluetooth_config::mojom::KeyEnteredHandler;

absl::optional<BluetoothHidDetector::BluetoothHidType> GetBluetoothHidType(
    const BluetoothDevicePropertiesPtr& device) {
  switch (device->device_type) {
    case DeviceType::kMouse:
      [[fallthrough]];
    case DeviceType::kTablet:
      return BluetoothHidDetector::BluetoothHidType::kPointer;
    case DeviceType::kKeyboard:
      return BluetoothHidDetector::BluetoothHidType::kKeyboard;
    case DeviceType::kKeyboardMouseCombo:
      return BluetoothHidDetector::BluetoothHidType::kKeyboardPointerCombo;
    default:
      NOTREACHED() << "Device with id " << device->id
                   << " has a device type that doesn't correspond with a HID: "
                   << device->device_type;
      return absl::nullopt;
  }
}

}  // namespace

BluetoothHidDetectorImpl::BluetoothHidDetectorImpl() = default;

BluetoothHidDetectorImpl::~BluetoothHidDetectorImpl() {
  DCHECK_EQ(kNotStarted, state_) << " HID detection must be stopped before "
                                 << "BluetoothHidDetectorImpl is destroyed.";
}

void BluetoothHidDetectorImpl::StartBluetoothHidDetection(
    Delegate* delegate,
    InputDevicesStatus input_devices_status) {
  DCHECK(input_devices_status.pointer_is_missing ||
         input_devices_status.keyboard_is_missing)
      << " StartBluetoothHidDetection() called when neither pointer or "
      << "keyboard is missing";
  DCHECK_EQ(kNotStarted, state_);
  HID_LOG(EVENT) << "Starting Bluetooth HID detection";
  delegate_ = delegate;
  input_devices_status_ = input_devices_status;
  state_ = kStarting;
  GetBluetoothConfigService(
      cros_bluetooth_config_remote_.BindNewPipeAndPassReceiver());
  cros_bluetooth_config_remote_->ObserveSystemProperties(
      system_properties_observer_receiver_.BindNewPipeAndPassRemote());
}

void BluetoothHidDetectorImpl::StopBluetoothHidDetection() {
  DCHECK_NE(kNotStarted, state_)
      << " Call to StopBluetoothHidDetection() while "
      << "HID detection is inactive.";
  HID_LOG(EVENT) << "Stopping Bluetooth HID detection";
  state_ = kNotStarted;
  cros_bluetooth_config_remote_->SetBluetoothHidDetectionActive(false);
  cros_bluetooth_config_remote_.reset();
  system_properties_observer_receiver_.reset();
  ResetDiscoveryState();
  delegate_ = nullptr;
}

const BluetoothHidDetector::BluetoothHidDetectionStatus
BluetoothHidDetectorImpl::GetBluetoothHidDetectionStatus() {
  if (!current_pairing_device_.has_value()) {
    return BluetoothHidDetectionStatus(
        /*current_pairing_device*/ absl::nullopt);
  }

  // TODO(crbug.com/1299099): Add |pairing_state|.
  return BluetoothHidDetectionStatus{BluetoothHidMetadata{
      base::UTF16ToUTF8(current_pairing_device_.value()->public_name),
      GetBluetoothHidType(current_pairing_device_.value()).value()}};
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
  HID_LOG(EVENT) << "Bluetooth discovery started.";
  DCHECK(!device_pairing_handler_remote_);
  device_pairing_handler_remote_.Bind(std::move(handler));
}

void BluetoothHidDetectorImpl::OnBluetoothDiscoveryStopped() {
  HID_LOG(EVENT) << "Bluetooth discovery stopped.";
  ResetDiscoveryState();
}

void BluetoothHidDetectorImpl::OnDiscoveredDevicesListChanged(
    std::vector<BluetoothDevicePropertiesPtr> discovered_devices) {
  for (const auto& discovered_device : discovered_devices) {
    if (!ShouldAttemptToPairWithDevice(discovered_device))
      continue;
    if (queued_device_ids_.contains(discovered_device->id))
      continue;

    queued_device_ids_.insert(discovered_device->id);
    queue_->emplace(discovered_device.Clone());
    HID_LOG(EVENT) << "Queuing device: " << discovered_device->id << ". ["
                   << queue_->size() << "] devices now in queue.";
  }
  ProcessQueue();
}

void BluetoothHidDetectorImpl::RequestPinCode(RequestPinCodeCallback callback) {
  // TODO(crbug/1299099): Implement.
}

void BluetoothHidDetectorImpl::RequestPasskey(RequestPasskeyCallback callback) {
  // TODO(crbug/1299099): Implement.
}

void BluetoothHidDetectorImpl::DisplayPinCode(
    const std::string& pin_code,
    mojo::PendingReceiver<KeyEnteredHandler> handler) {
  // TODO(crbug/1299099): Implement.
}

void BluetoothHidDetectorImpl::DisplayPasskey(
    const std::string& passkey,
    mojo::PendingReceiver<KeyEnteredHandler> handler) {
  // TODO(crbug/1299099): Implement.
}

void BluetoothHidDetectorImpl::ConfirmPasskey(const std::string& passkey,
                                              ConfirmPasskeyCallback callback) {
  // TODO(crbug/1299099): Implement.
}

void BluetoothHidDetectorImpl::AuthorizePairing(
    AuthorizePairingCallback callback) {
  // TODO(crbug/1299099): Implement.
}

bool BluetoothHidDetectorImpl::ShouldAttemptToPairWithDevice(
    const BluetoothDevicePropertiesPtr& device) {
  switch (device->device_type) {
    case DeviceType::kMouse:
      [[fallthrough]];
    case DeviceType::kTablet:
      return input_devices_status_.pointer_is_missing;
    case DeviceType::kKeyboard:
      return input_devices_status_.keyboard_is_missing;
    case DeviceType::kKeyboardMouseCombo:
      return input_devices_status_.pointer_is_missing ||
             input_devices_status_.keyboard_is_missing;
    default:
      return false;
  }
}

void BluetoothHidDetectorImpl::ProcessQueue() {
  if (current_pairing_device_)
    return;

  if (queue_->empty()) {
    HID_LOG(DEBUG) << "No devices queued";
    return;
  }

  current_pairing_device_ = std::move(queue_->front());
  queue_->pop();
  HID_LOG(EVENT) << "Popped device with id: "
                 << current_pairing_device_.value()->id
                 << " from front of queue. [" << queue_->size()
                 << "] devices now in queue.";

  // TODO(crbug.com/1299099): Check if device type is still missing and return
  // early if not.

  HID_LOG(EVENT) << "Pairing with device with id: "
                 << current_pairing_device_.value()->id;
  device_pairing_handler_remote_->PairDevice(
      current_pairing_device_.value()->id,
      device_pairing_delegate_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&BluetoothHidDetectorImpl::OnPairDevice,
                     weak_ptr_factory_.GetWeakPtr()));
  delegate_->OnBluetoothHidStatusChanged();
}

void BluetoothHidDetectorImpl::OnPairDevice(
    chromeos::bluetooth_config::mojom::PairingResult pairing_result) {
  HID_LOG(EVENT) << "Finished pairing with "
                 << current_pairing_device_.value()->id
                 << ", result: " << pairing_result << ", [" << queue_->size()
                 << "] devices still in queue.";
  queued_device_ids_.erase(current_pairing_device_.value()->id);
  current_pairing_device_.reset();
  device_pairing_delegate_receiver_.reset();
  delegate_->OnBluetoothHidStatusChanged();
  ProcessQueue();
}

void BluetoothHidDetectorImpl::ResetDiscoveryState() {
  // Reset Mojo-related properties.
  bluetooth_discovery_delegate_receiver_.reset();
  device_pairing_handler_remote_.reset();
  device_pairing_delegate_receiver_.reset();

  // Reset queue-related properties.
  current_pairing_device_.reset();
  queue_ = std::make_unique<base::queue<
      chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr>>();
  queued_device_ids_.clear();

  // Inform |delegate_| that no device is currently pairing.
  delegate_->OnBluetoothHidStatusChanged();
}

}  // namespace hid_detection
}  // namespace ash
