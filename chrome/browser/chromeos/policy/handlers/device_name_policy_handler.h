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
#include "chromeos/system/statistics_provider.h"

namespace policy {

// This class observes the device setting |DeviceHostname|, and calls
// NetworkStateHandler::SetHostname() appropriately based on the value of that
// setting.
class DeviceNamePolicyHandler : public chromeos::NetworkStateHandlerObserver {
 public:
  // Types of policies for device name functionality.
  enum class DeviceNamePolicy {
    // No policy in place for administrator to choose a hostname.
    kNoPolicy,

    // Policy in place allowing administrator to specify a template
    // used to generate and format the hostname.
    kPolicyHostnameChosenByAdmin,
  };

  explicit DeviceNamePolicyHandler(ash::CrosSettings* cros_settings);
  ~DeviceNamePolicyHandler() override;

  // NetworkStateHandlerObserver overrides
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;

  // Returns the device hostname that DeviceNamePolicyHandler has last set in
  // shill. This is the hostname after formatting (by FormatHostname()).
  const std::string& GetDeviceHostname() const;

  // Provides the type of policy to be used for device name functionality.
  DeviceNamePolicy GetDeviceNamePolicy() const;

  // Provides hostname if requested by administrator.
  // Returns null if no hostname was requested by administrator.
  absl::optional<std::string> GetHostnameChosenByAdministrator() const;

 private:
  friend class DeviceNamePolicyHandlerTest;

  DeviceNamePolicyHandler(
      ash::CrosSettings* cros_settings,
      chromeos::system::StatisticsProvider* statistics_provider);

  void OnDeviceHostnamePropertyChanged();

  void OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded();

  ash::CrosSettings* cros_settings_;
  chromeos::system::StatisticsProvider* statistics_provider_;

  DeviceNamePolicy device_name_policy_ = DeviceNamePolicy::kNoPolicy;
  base::CallbackListSubscription policy_subscription_;
  std::string hostname_;
  base::WeakPtrFactory<DeviceNamePolicyHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DeviceNamePolicyHandler);
};

std::ostream& operator<<(
    std::ostream& stream,
    const DeviceNamePolicyHandler::DeviceNamePolicy& state);

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_H_
