// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_NAME_APPLIER_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_NAME_APPLIER_H_

#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

// This class is used to set the device name via DHCP and Bluetooth.
class DeviceNameApplier {
 public:
  virtual ~DeviceNameApplier() = default;

  // Replaces the existing device name in DHCP with the new one.
  virtual void SetDeviceName(const std::string& new_device_name) = 0;

 protected:
  DeviceNameApplier() = default;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_NAME_APPLIER_H_
