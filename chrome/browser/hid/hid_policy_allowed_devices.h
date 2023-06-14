// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HID_HID_POLICY_ALLOWED_DEVICES_H_
#define CHROME_BROWSER_HID_HID_POLICY_ALLOWED_DEVICES_H_

#include <map>
#include <set>
#include <string>

#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "url/origin.h"

namespace device::mojom {
class HidDeviceInfo;
}  // namespace device::mojom

class PrefRegistrySimple;
class PrefService;

// This class is used to maintain and interpret the
// WebHidAllowAllDevicesForUrls, WebHidAllowDevicesForUrls,
// DeviceLoginScreenWebHidAllowDevicesForUrls, and
// WebHidAllowDevicesWithHidUsagesForUrls policies.
//
// A PrefChangeRegistrar is used to observe changes to the preference values so
// that the policy can be updated in real-time.
class HidPolicyAllowedDevices : public KeyedService {
 public:
  using OriginSet = std::set<url::Origin>;
  using VendorPolicyMap = std::map<uint16_t, OriginSet>;
  using DevicePolicyMap = std::map<std::pair<uint16_t, uint16_t>, OriginSet>;
  using UsagePagePolicyMap = std::map<uint16_t, OriginSet>;
  using UsagePolicyMap = std::map<std::pair<uint16_t, uint16_t>, OriginSet>;

  explicit HidPolicyAllowedDevices(PrefService* pref_service,
                                   bool on_login_screen);
  HidPolicyAllowedDevices(const HidPolicyAllowedDevices&) = delete;
  HidPolicyAllowedDevices& operator=(const HidPolicyAllowedDevices&) = delete;
  ~HidPolicyAllowedDevices() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  const VendorPolicyMap& vendor_policy() const { return vendor_policy_; }
  const DevicePolicyMap& device_policy() const { return device_policy_; }
  const UsagePagePolicyMap& usage_page_policy() const {
    return usage_page_policy_;
  }
  const UsagePolicyMap& usage_policy() const { return usage_policy_; }
  const OriginSet& all_devices_policy() const { return all_devices_policy_; }

  // Checks if |origin| is allowed to use |device|.
  bool HasDevicePermission(const url::Origin& origin,
                           const device::mojom::HidDeviceInfo& device);

 private:
  void LoadAllowAllDevicesForUrlsPolicy();
  void LoadAllowDevicesForUrlsPolicy();
  void LoadAllowDevicesWithHidUsagesForUrlsPolicy();

  // Stores the name of a pref that should be used by the class. It can either
  // be the login screen or the in-session pref.
  const std::string allow_devices_for_urls_pref_name_;

  PrefChangeRegistrar pref_change_registrar_;

  // Stores the current policy configuration for origins allowed to access any
  // connected device.
  OriginSet all_devices_policy_;

  // Stores the current policy configuration for origins allowed to access
  // specific devices identified by vendor and product IDs (device_policy_) or
  // all devices from a particular vendor ID (vendor_policy_).
  DevicePolicyMap device_policy_;
  VendorPolicyMap vendor_policy_;

  // Stores the current policy configuration for origins allowed to access
  // devices containing top-level collections with specific HID usages
  // (usage_policy_) or any usage from a particular usage page
  // (usage_page_policy_).
  UsagePolicyMap usage_policy_;
  UsagePagePolicyMap usage_page_policy_;
};

#endif  // CHROME_BROWSER_HID_HID_POLICY_ALLOWED_DEVICES_H_
