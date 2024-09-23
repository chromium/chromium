// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_DEVICETYPE_H_
#define ASH_CONSTANTS_DEVICETYPE_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/constants/devicetype.h"

namespace ash {

// Returns the name of current device for Bluetooth. |bluetooth_address| is
// so it can be used (after hashing) to create a more identifiable device name,
// e.g., "Chromebook_1A2B", "Chromebox_F9E8'.
COMPONENT_EXPORT(ASH_CONSTANTS)
std::string GetDeviceBluetoothName(const std::string& bluetooth_address);

// Returns the name of the provided Chrome device type.
COMPONENT_EXPORT(ASH_CONSTANTS)
std::string DeviceTypeToString(chromeos::DeviceType device_type);

// Returns true if the device is Google branded.
COMPONENT_EXPORT(ASH_CONSTANTS) bool IsGoogleBrandedDevice();

}  // namespace ash

#endif  // ASH_CONSTANTS_DEVICETYPE_H_
