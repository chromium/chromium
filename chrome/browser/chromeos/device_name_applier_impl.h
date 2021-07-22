// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DEVICE_NAME_APPLIER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_DEVICE_NAME_APPLIER_IMPL_H_

#include "chrome/browser/chromeos/device_name_applier.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

// DeviceNameApplier implementation which uses NetworkStateHandler to set
// the device name via DHCP.
class DeviceNameApplierImpl : public DeviceNameApplier {
 public:
  DeviceNameApplierImpl();
  ~DeviceNameApplierImpl() override;

 private:
  explicit DeviceNameApplierImpl(NetworkStateHandler* network_state_handler);

  // DeviceNameApplier:
  void SetDeviceName(const std::string& new_device_name) override;

  chromeos::NetworkStateHandler* network_state_handler_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DEVICE_NAME_APPLIER_IMPL_H_
