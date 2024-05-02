// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_ON_DEVICE_UTILS_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_ON_DEVICE_UTILS_H_

#include <string>

namespace ash::on_device_controls {

// Returns the device region from VPD in the 2 uppercase letters format or empty
// string if device region is virtual, unavailable or invalid.
// Insensitive to the input case.
std::string GetDeviceRegionCode();

// Returns whether on device controls are available in the region identified by
// `region_code`. Returns false for invalid regions. Case sensitive.
bool IsOnDeviceControlsRegion(const std::string& region_code);

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_ON_DEVICE_UTILS_H_
