// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_wifi_allowed_handler.h"

#include <vector>

#include "base/bind.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/prohibited_technologies_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace policy {

DeviceWiFiAllowedHandler::DeviceWiFiAllowedHandler(
    chromeos::CrosSettings* cros_settings)
    : cros_settings_(cros_settings) {
  wifi_policy_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kDeviceWiFiAllowed,
      base::BindRepeating(&DeviceWiFiAllowedHandler::OnWiFiPolicyChanged,
                          weak_factory_.GetWeakPtr()));

  // Fire it once so we're sure we get an invocation on startup.
  OnWiFiPolicyChanged();
}

DeviceWiFiAllowedHandler::~DeviceWiFiAllowedHandler() = default;

void DeviceWiFiAllowedHandler::OnWiFiPolicyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::BindRepeating(&DeviceWiFiAllowedHandler::OnWiFiPolicyChanged,
                              weak_factory_.GetWeakPtr()));
  if (status != chromeos::CrosSettingsProvider::TRUSTED)
    return;

  bool wifi_allowed = true;
  cros_settings_->GetBoolean(chromeos::kDeviceWiFiAllowed, &wifi_allowed);
  if (!wifi_allowed) {
    chromeos::NetworkHandler::Get()
        ->prohibited_technologies_handler()
        ->AddGloballyProhibitedTechnology(shill::kTypeWifi);
  } else {
    chromeos::NetworkHandler::Get()
        ->prohibited_technologies_handler()
        ->RemoveGloballyProhibitedTechnology(shill::kTypeWifi);
  }
}

}  // namespace policy
