// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace policy {

// This class observes the device setting |DeviceHostname|, and calls
// NetworkStateHandler::SetHostname() appropriately based on the value of that
// setting.
class DeviceNamePolicyHandler : public chromeos::NetworkStateHandlerObserver {
 public:
  explicit DeviceNamePolicyHandler(ash::CrosSettings* cros_settings);
  ~DeviceNamePolicyHandler() override;

  // NetworkStateHandlerObserver overrides
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;

  void Shutdown();

  // Returns the device hostname that DeviceNamePolicyHandler has last set in
  // shill. This is the hostname after formatting (by FormatHostname()).
  const std::string& GetDeviceHostname() const;

 private:
  friend class DeviceNamePolicyHandlerTest;

  // Uses template to build a hostname. Returns valid hostname (after parameter
  // substitution) or empty string, if substitution result is not a valid
  // hostname.
  static std::string FormatHostname(const std::string& name_template,
                                    const std::string& asset_id,
                                    const std::string& serial,
                                    const std::string& mac,
                                    const std::string& machine_name,
                                    const std::string& location);

  void OnDeviceHostnamePropertyChanged();

  void OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded();

  ash::CrosSettings* cros_settings_;
  base::CallbackListSubscription policy_subscription_;
  std::string hostname_;
  base::WeakPtrFactory<DeviceNamePolicyHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceNamePolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_H_
