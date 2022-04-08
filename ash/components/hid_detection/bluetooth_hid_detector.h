// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_H_
#define ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_H_

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

  virtual ~BluetoothHidDetector();

  virtual void StartBluetoothHidDetection(
      InputDevicesStatus input_devices_status) = 0;
  virtual void StopBluetoothHidDetection() = 0;

 protected:
  BluetoothHidDetector();
};

}  // namespace hid_detection
}  // namespace ash

#endif  // ASH_COMPONENTS_HID_DETECTION_BLUETOOTH_HID_DETECTOR_H_
