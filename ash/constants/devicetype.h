// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_DEVICETYPE_H_
#define ASH_CONSTANTS_DEVICETYPE_H_

#include "base/component_export.h"

namespace ash {

enum class DeviceType {
  kChromebase,
  kChromebit,
  kChromebook,
  kChromebox,
  kUnknown,  // Unknown fallback device.
};

// Returns the current device type, eg, Chromebook, Chromebox.
COMPONENT_EXPORT(ASH_CONSTANTS) DeviceType GetDeviceType();

// Returns true if the device is Google branded.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsGoogleBrandedDevice();

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the Chrome OS source code
// directory migration is finished.
namespace chromeos {
using ::ash::DeviceType;
using ::ash::GetDeviceType;
using ::ash::IsGoogleBrandedDevice;
}  // namespace chromeos

#endif  // ASH_CONSTANTS_DEVICETYPE_H_
