// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/hid_detection_utils.h"

#include "base/metrics/histogram_functions.h"
#include "components/device_event_log/device_event_log.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::hid_detection {
namespace {

using InputDeviceType = device::mojom::InputDeviceType;

absl::optional<HidType> GetHidType(
    const device::mojom::InputDeviceInfo& device) {
  if (device.is_touchscreen || device.is_tablet)
    return HidType::kTouchscreen;

  if (device.is_mouse || device.is_touchpad) {
    switch (device.type) {
      case InputDeviceType::TYPE_BLUETOOTH:
        return HidType::kBluetoothPointer;
      case InputDeviceType::TYPE_USB:
        return HidType::kUsbPointer;
      case InputDeviceType::TYPE_SERIO:
        return HidType::kSerialPointer;
      case InputDeviceType::TYPE_UNKNOWN:
        return HidType::kUnknownPointer;
    }
  }

  if (device.is_keyboard) {
    switch (device.type) {
      case InputDeviceType::TYPE_BLUETOOTH:
        return HidType::kBluetoothKeyboard;
      case InputDeviceType::TYPE_USB:
        return HidType::kUsbKeyboard;
      case InputDeviceType::TYPE_SERIO:
        return HidType::kSerialKeyboard;
      case InputDeviceType::TYPE_UNKNOWN:
        return HidType::kUnknownKeyboard;
    }
  }

  return absl::nullopt;
}

}  // namespace

void RecordHidConnected(const device::mojom::InputDeviceInfo& device) {
  absl::optional<HidType> hid_type = GetHidType(device);

  // If |device| is not relevant (i.e. an accelerometer, joystick, etc), don't
  // emit metric.
  if (!hid_type.has_value()) {
    HID_LOG(DEBUG) << "HidConnected not logged for device " << device.id
                   << " because it doesn't have a relevant device type.";
    return;
  }

  base::UmaHistogramEnumeration("OOBE.HidDetectionScreen.HidConnected",
                                hid_type.value());
}

}  // namespace ash::hid_detection
