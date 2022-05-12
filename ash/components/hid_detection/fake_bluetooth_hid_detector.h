// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_HID_DETECTION_FAKE_BLUETOOTH_HID_DETECTOR_H_
#define ASH_COMPONENTS_HID_DETECTION_FAKE_BLUETOOTH_HID_DETECTOR_H_

#include "ash/components/hid_detection/bluetooth_hid_detector.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace hid_detection {

class FakeBluetoothHidDetector : public BluetoothHidDetector {
 public:
  FakeBluetoothHidDetector();
  ~FakeBluetoothHidDetector() override;

  // BluetoothHidDetector:
  void SetInputDevicesStatus(InputDevicesStatus input_devices_status) override;
  const BluetoothHidDetectionStatus GetBluetoothHidDetectionStatus() override;

  // Updates the HID detection status returned by
  // GetBluetoothHidDetectionStatus() and notifies the client.
  void SetBluetoothHidDetectionStatus(
      absl::optional<BluetoothHidDetector::BluetoothHidMetadata> pairing_device,
      absl::optional<BluetoothHidPairingState> pairing_state);

  const InputDevicesStatus& input_devices_status() {
    return input_devices_status_;
  }

  bool is_bluetooth_hid_detection_active() {
    return is_bluetooth_hid_detection_active_;
  }

 private:
  // BluetoothHidDetector:
  void PerformStartBluetoothHidDetection(
      InputDevicesStatus input_devices_status) override;
  void PerformStopBluetoothHidDetection() override;

  InputDevicesStatus input_devices_status_;
  absl::optional<BluetoothHidMetadata> current_pairing_device_;
  absl::optional<BluetoothHidPairingState> current_pairing_state_;
  bool is_bluetooth_hid_detection_active_ = false;
};

}  // namespace hid_detection
}  // namespace ash

#endif  // ASH_COMPONENTS_HID_DETECTION_FAKE_BLUETOOTH_HID_DETECTOR_H_
