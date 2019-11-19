// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_WIFI_ALLOWED_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_WIFI_ALLOWED_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace policy {

// This class observes the device setting |DeviceWiFiAllowed|, and updates
// the list of shill ProhibitedTechnoligies based on this
// setting.
class DeviceWiFiAllowedHandler {
 public:
  explicit DeviceWiFiAllowedHandler(chromeos::CrosSettings* cros_settings);
  ~DeviceWiFiAllowedHandler();

 private:
  void OnWiFiPolicyChanged();

  chromeos::CrosSettings* cros_settings_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      wifi_policy_subscription_;
  base::WeakPtrFactory<DeviceWiFiAllowedHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceWiFiAllowedHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_WIFI_ALLOWED_HANDLER_H_
