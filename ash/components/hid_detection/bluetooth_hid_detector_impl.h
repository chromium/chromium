// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_IMPL_H_
#define ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_IMPL_H_

#include "ash/components/hid_detection/bluetooth_hid_detector.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {
namespace hid_detection {

// Concrete BluetoothHidDetector implementation that uses CrosBluetoothConfig.
class BluetoothHidDetectorImpl
    : public BluetoothHidDetector,
      public chromeos::bluetooth_config::mojom::SystemPropertiesObserver,
      public chromeos::bluetooth_config::mojom::BluetoothDiscoveryDelegate {
 public:
  BluetoothHidDetectorImpl();
  ~BluetoothHidDetectorImpl() override;

  // BluetoothHidDetector:
  void StartBluetoothHidDetection() override;
  void StopBluetoothHidDetection() override;

 private:
  // States used for internal state machine.
  enum State {
    // HID detection is currently not active.
    kNotStarted,

    // HID detection has began activating.
    kStarting,

    // HID detection has began activating and is waiting for the Bluetooth
    // adapter to be enabled.
    kEnablingAdapter,

    // HID detection is fully active and is now searching for devices.
    kDetecting,

    // HID detection is paused due to the Bluetooth adapter becoming unenabled
    // for external reasons.
    kStoppedExternally,
  };

  // chromeos::bluetooth_config::mojom::SystemPropertiesObserver
  void OnPropertiesUpdated(
      chromeos::bluetooth_config::mojom::BluetoothSystemPropertiesPtr
          properties) override;

  // chromeos::bluetooth_config::mojom::BluetoothDiscoveryDelegate
  void OnBluetoothDiscoveryStarted(
      mojo::PendingRemote<
          chromeos::bluetooth_config::mojom::DevicePairingHandler> handler)
      override;
  void OnBluetoothDiscoveryStopped() override;
  void OnDiscoveredDevicesListChanged(
      std::vector<
          chromeos::bluetooth_config::mojom::BluetoothDevicePropertiesPtr>
          discovered_devices) override;

  State state_ = kNotStarted;

  mojo::Remote<chromeos::bluetooth_config::mojom::CrosBluetoothConfig>
      cros_bluetooth_config_remote_;
  mojo::Receiver<chromeos::bluetooth_config::mojom::SystemPropertiesObserver>
      system_properties_observer_receiver_{this};
  mojo::Receiver<chromeos::bluetooth_config::mojom::BluetoothDiscoveryDelegate>
      bluetooth_discovery_delegate_receiver_{this};
};

}  // namespace hid_detection
}  // namespace ash

#endif  // ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_IMPL_H_
