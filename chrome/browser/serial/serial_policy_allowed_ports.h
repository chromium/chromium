// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERIAL_SERIAL_POLICY_ALLOWED_PORTS_H_
#define CHROME_BROWSER_SERIAL_SERIAL_POLICY_ALLOWED_PORTS_H_

#include <map>
#include <set>

#include "components/prefs/pref_change_registrar.h"
#include "url/origin.h"

namespace device {
namespace mojom {
class SerialPortInfo;
}  // namespace mojom
}  // namespace device

class PrefRegistrySimple;
class PrefService;

// This class is used to maintain and interpret the SerialAllowForUrls and
// SerialAllowUsbDevicesForUrls policies.
//
// A PrefChangeRegistrar is used to observe changes to the preference values so
// that the policy can be updated in real-time.
class SerialPolicyAllowedPorts {
 public:
  explicit SerialPolicyAllowedPorts(PrefService* pref_service);
  SerialPolicyAllowedPorts(SerialPolicyAllowedPorts& other) = delete;
  SerialPolicyAllowedPorts& operator=(SerialPolicyAllowedPorts& other) = delete;
  ~SerialPolicyAllowedPorts();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Checks if |origin| is allowed to use the port with |port_info|.
  bool HasPortPermission(const url::Origin& origin,
                         const device::mojom::SerialPortInfo& port_info);

  const std::map<std::pair<int, int>, std::set<url::Origin>>&
  usb_device_policy() const {
    return usb_device_policy_;
  }
  const std::map<int, std::set<url::Origin>>& usb_vendor_policy() const {
    return usb_vendor_policy_;
  }
  const std::set<url::Origin>& all_ports_policy() const {
    return all_ports_policy_;
  }

 private:
  void LoadAllowAllPortsForUrlsPolicy();
  void LoadAllowUsbDevicesForUrlsPolicy();

  PrefChangeRegistrar pref_change_registrar_;

  // Stores the current policy configuration for specific USB devices
  // identified by vendor and product IDs (usb_device_policy_), all USB
  // devices from a particular vendor ID (usb_vendor_policy_) and origins
  // which are allowed to access all ports.
  std::map<std::pair<int, int>, std::set<url::Origin>> usb_device_policy_;
  std::map<int, std::set<url::Origin>> usb_vendor_policy_;
  std::set<url::Origin> all_ports_policy_;
};

#endif  // CHROME_BROWSER_SERIAL_SERIAL_POLICY_ALLOWED_PORTS_H_
