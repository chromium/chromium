// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_H_
#define ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace hid_detection {

// Manages searching for unpaired Bluetooth human interactive devices and
// automatically attempting to pairing with them if their device type is not
// currently paired with.
class BluetoothHidDetector {
 public:
  struct InputDevicesStatus {
    bool pointer_is_missing;
    bool keyboard_is_missing;
  };

  enum class BluetoothHidType {
    // A mouse, trackball, touchpad, etc.
    kPointer,
    kKeyboard,
    kKeyboardPointerCombo
  };

  // Struct representing a Bluetooth human-interactive device.
  struct BluetoothHidMetadata {
    BluetoothHidMetadata(std::string name, BluetoothHidType type);
    BluetoothHidMetadata(BluetoothHidMetadata&& other);
    BluetoothHidMetadata& operator=(BluetoothHidMetadata&& other);
    ~BluetoothHidMetadata();

    std::string name;
    BluetoothHidType type;
  };

  // Struct representing the current status of BluetoothHidDetector.
  struct BluetoothHidDetectionStatus {
    explicit BluetoothHidDetectionStatus(
        absl::optional<BluetoothHidMetadata> current_pairing_device);
    BluetoothHidDetectionStatus(BluetoothHidDetectionStatus&& other);
    BluetoothHidDetectionStatus& operator=(BluetoothHidDetectionStatus&& other);
    ~BluetoothHidDetectionStatus();

    // The metadata of the device currently being paired with.
    absl::optional<BluetoothHidMetadata> current_pairing_device;

    // TODO(crbug.com/1299099): Add |pairing_state|.
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Invoked whenever any Bluetooth detection status property changes.
    virtual void OnBluetoothHidStatusChanged() = 0;
  };

  virtual ~BluetoothHidDetector();

  virtual void StartBluetoothHidDetection(
      Delegate* delegate,
      InputDevicesStatus input_devices_status) = 0;
  virtual void StopBluetoothHidDetection() = 0;
  virtual const BluetoothHidDetectionStatus
  GetBluetoothHidDetectionStatus() = 0;

 protected:
  BluetoothHidDetector();
};

}  // namespace hid_detection
}  // namespace ash

#endif  // ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_H_
