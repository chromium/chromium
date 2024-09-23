// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_WIFI_ALLOWED_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_WIFI_ALLOWED_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/settings/cros_settings.h"

namespace policy {

// This class observes the device setting |DeviceWiFiAllowed|, and updates
// the list of shill ProhibitedTechnoligies based on this
// setting.
class DeviceWiFiAllowedHandler {
 public:
  explicit DeviceWiFiAllowedHandler(ash::CrosSettings* cros_settings);

  DeviceWiFiAllowedHandler(const DeviceWiFiAllowedHandler&) = delete;
  DeviceWiFiAllowedHandler& operator=(const DeviceWiFiAllowedHandler&) = delete;

  ~DeviceWiFiAllowedHandler();

 private:
  void OnWiFiPolicyChanged();

  raw_ptr<ash::CrosSettings, DanglingUntriaged> cros_settings_;
  base::CallbackListSubscription wifi_policy_subscription_;
  base::WeakPtrFactory<DeviceWiFiAllowedHandler> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_WIFI_ALLOWED_HANDLER_H_
