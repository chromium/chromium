// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_APPLIER_H_
#define CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_APPLIER_H_

#include <string>

namespace ash {

// This class is used to set the device name via DHCP and Bluetooth.
class DeviceNameApplier {
 public:
  virtual ~DeviceNameApplier() = default;

  // Replaces the existing device name in DHCP and Bluetooth with the new one.
  virtual void SetDeviceName(const std::string& new_device_name) = 0;

 protected:
  DeviceNameApplier() = default;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_APPLIER_H_
