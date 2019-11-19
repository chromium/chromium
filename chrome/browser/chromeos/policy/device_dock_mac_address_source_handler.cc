// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_dock_mac_address_source_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace em = enterprise_management;

namespace policy {

DeviceDockMacAddressHandler::DeviceDockMacAddressHandler(
    chromeos::CrosSettings* cros_settings,
    chromeos::NetworkDeviceHandler* network_device_handler)
    : cros_settings_(cros_settings),
      network_device_handler_(network_device_handler) {
  dock_mac_address_source_policy_subscription_ =
      cros_settings_->AddSettingsObserver(
          chromeos::kDeviceDockMacAddressSource,
          base::BindRepeating(
              &DeviceDockMacAddressHandler::OnDockMacAddressSourcePolicyChanged,
              weak_factory_.GetWeakPtr()));
  // Fire it once so we're sure we get an invocation on startup.
  OnDockMacAddressSourcePolicyChanged();
}

DeviceDockMacAddressHandler::~DeviceDockMacAddressHandler() = default;

void DeviceDockMacAddressHandler::OnDockMacAddressSourcePolicyChanged() {
  // Wait for the |cros_settings_| to become trusted.
  const chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::BindRepeating(
          &DeviceDockMacAddressHandler::OnDockMacAddressSourcePolicyChanged,
          weak_factory_.GetWeakPtr()));
  if (status != chromeos::CrosSettingsProvider::TRUSTED)
    return;

  int dock_mac_address_source;
  if (!cros_settings_->GetInteger(chromeos::kDeviceDockMacAddressSource,
                                  &dock_mac_address_source)) {
    return;
  }

  const char* source = nullptr;
  switch (dock_mac_address_source) {
    case em::DeviceDockMacAddressSourceProto::DEVICE_DOCK_MAC_ADDRESS:
      source = shill::kUsbEthernetMacAddressSourceDesignatedDockMac;
      break;
    case em::DeviceDockMacAddressSourceProto::DEVICE_NIC_MAC_ADDRESS:
      source = shill::kUsbEthernetMacAddressSourceBuiltinAdapterMac;
      break;
    case em::DeviceDockMacAddressSourceProto::DOCK_NIC_MAC_ADDRESS:
      source = shill::kUsbEthernetMacAddressSourceUsbAdapterMac;
      break;
    default:
      LOG(ERROR) << "Unknown dock MAC address source: "
                 << dock_mac_address_source;
      return;
  }

  network_device_handler_->SetUsbEthernetMacAddressSource(source);
}

}  // namespace policy
