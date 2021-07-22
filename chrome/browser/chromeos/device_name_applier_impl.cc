// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/device_name_applier_impl.h"

namespace chromeos {

DeviceNameApplierImpl::DeviceNameApplierImpl()
    : DeviceNameApplierImpl(NetworkHandler::Get()->network_state_handler()) {}

DeviceNameApplierImpl::DeviceNameApplierImpl(
    NetworkStateHandler* network_state_handler)
    : network_state_handler_(network_state_handler) {}

DeviceNameApplierImpl::~DeviceNameApplierImpl() = default;

// TODO: call BluetoothAdapter::SetName() with the new device name.
void DeviceNameApplierImpl::SetDeviceName(const std::string& new_device_name) {
  network_state_handler_->SetHostname(new_device_name);
}

}  // namespace chromeos
