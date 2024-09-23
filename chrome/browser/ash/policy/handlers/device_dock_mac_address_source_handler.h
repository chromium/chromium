// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_DOCK_MAC_ADDRESS_SOURCE_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_DOCK_MAC_ADDRESS_SOURCE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/settings/cros_settings.h"

namespace ash {
class NetworkDeviceHandler;
}

namespace policy {

// This class observes the device setting |DeviceDockMacAddressSource|, and
// updates shill EthernetMacAddressSource property based on this setting.
class DeviceDockMacAddressHandler {
 public:
  DeviceDockMacAddressHandler(
      ash::CrosSettings* cros_settings,
      ash::NetworkDeviceHandler* network_device_handler);

  DeviceDockMacAddressHandler(const DeviceDockMacAddressHandler&) = delete;
  DeviceDockMacAddressHandler& operator=(const DeviceDockMacAddressHandler&) =
      delete;

  ~DeviceDockMacAddressHandler();

 private:
  void OnDockMacAddressSourcePolicyChanged();

  raw_ptr<ash::CrosSettings, DanglingUntriaged> cros_settings_;
  raw_ptr<ash::NetworkDeviceHandler, DanglingUntriaged> network_device_handler_;
  base::CallbackListSubscription dock_mac_address_source_policy_subscription_;
  base::WeakPtrFactory<DeviceDockMacAddressHandler> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_DOCK_MAC_ADDRESS_SOURCE_HANDLER_H_
