// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_DOCK_MAC_ADDRESS_SOURCE_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_DOCK_MAC_ADDRESS_SOURCE_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace chromeos {
class NetworkDeviceHandler;
}  // namespace chromeos

namespace policy {

// This class observes the device setting |DeviceDockMacAddressSource|, and
// updates shill EthernetMacAddressSource property based on this setting.
class DeviceDockMacAddressHandler {
 public:
  DeviceDockMacAddressHandler(
      chromeos::CrosSettings* cros_settings,
      chromeos::NetworkDeviceHandler* network_device_handler);
  ~DeviceDockMacAddressHandler();

 private:
  void OnDockMacAddressSourcePolicyChanged();

  chromeos::CrosSettings* cros_settings_;
  chromeos::NetworkDeviceHandler* network_device_handler_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      dock_mac_address_source_policy_subscription_;
  base::WeakPtrFactory<DeviceDockMacAddressHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceDockMacAddressHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_DOCK_MAC_ADDRESS_SOURCE_HANDLER_H_
