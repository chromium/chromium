// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_UTILS_H_
#define ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_UTILS_H_

#include "services/device/public/mojom/input_service.mojom.h"

namespace ash::hid_detection {

// This enum is tied directly to the HidType UMA enum defined in
// //tools/metrics/histograms/enums.xml, and should always reflect it (do not
// change one without changing the other).
enum class HidType {
  kTouchscreen = 0,
  kUsbKeyboard = 1,
  kUsbPointer = 2,
  kSerialKeyboard = 3,
  kSerialPointer = 4,
  kBluetoothKeyboard = 5,
  kBluetoothPointer = 6,
  kUnknownKeyboard = 7,
  kUnknownPointer = 8,
  kMaxValue = kUnknownPointer
};

// Record each HID that is connected while the HID detection screen is shown.
void RecordHidConnected(const device::mojom::InputDeviceInfo& device);

}  // namespace ash::hid_detection

#endif  // ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_UTILS_H_
