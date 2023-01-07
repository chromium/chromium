// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POWER_HID_BATTERY_UTIL_H_
#define ASH_POWER_HID_BATTERY_UTIL_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

// Checks whether the device at |path| is a HID battery. |path| is a sysfs
// device path like "/sys/class/power-supply/hid-FF:EE:DD:CC:BB:AA-battery"
// Returns false if |path| is lacking the HID battery prefix or suffix, or if it
// contains them but has nothing in between.
ASH_EXPORT bool IsHIDBattery(const std::string& path);

// Extract the identifier in |path| found between the path prefix and suffix.
// |path| is a sysfs device path like
// "/sys/class/power-supply/hid-FF:EE:DD:CC:BB:AA-battery"
ASH_EXPORT std::string ExtractHIDBatteryIdentifier(const std::string& path);

// Extracts a Bluetooth address (e.g. "AA:BB:CC:DD:EE:FF") from |path|, a sysfs
// device path like "/sys/class/power-supply/hid-FF:EE:DD:CC:BB:AA-battery".
// The address supplied in |path| is reversed, so this method will reverse the
// extracted address. Returns an empty string if |path| does not contain a
// Bluetooth address.
ASH_EXPORT std::string ExtractBluetoothAddressFromHIDBatteryPath(
    const std::string& path);

}  // namespace ash

#endif  // ASH_POWER_HID_BATTERY_UTIL_H_
