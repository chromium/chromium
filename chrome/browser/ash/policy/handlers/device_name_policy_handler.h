// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/system/statistics_provider.h"

namespace ash {
class NetworkState;
class NetworkStateHandler;
}  // namespace ash

namespace policy {

class BrowserPolicyConnectorAsh;

// This class observes the device setting |DeviceHostname|, and calls
// NetworkStateHandler::SetHostname() appropriately based on the value of that
// setting.
class DeviceNamePolicyHandler : public ash::NetworkStateHandlerObserver {
 public:
  // Types of policies for device name functionality.
  enum class DeviceNamePolicy {
    // No device name policy in place.
    kNoPolicy,

    // Policy in place allowing administrator to specify a template
    // used to generate and format the hostname.
    kPolicyHostnameChosenByAdmin,

    // Policy in place which prohibits users from configuring device name.
    kPolicyHostnameNotConfigurable,
  };

  // `browser_policy_connector_ash` and `cros_settings` must be non-null and
  // must outlive `this`.
  DeviceNamePolicyHandler(
      BrowserPolicyConnectorAsh* browser_policy_connector_ash,
      ash::CrosSettings* cros_settings);

  DeviceNamePolicyHandler(const DeviceNamePolicyHandler&) = delete;
  DeviceNamePolicyHandler& operator=(const DeviceNamePolicyHandler&) = delete;

  ~DeviceNamePolicyHandler() override;

  std::optional<std::string> GetHostnameChosenByAdministrator() const;

  // For unit tests:
  DeviceNamePolicy GetDeviceNamePolicyForTesting() const {
    return device_name_policy_;
  }

 private:
  friend class DeviceNamePolicyHandlerTest;

  DeviceNamePolicyHandler(
      BrowserPolicyConnectorAsh* browser_policy_connector_ash,
      ash::CrosSettings* cros_settings,
      ash::system::StatisticsProvider* statistics_provider,
      ash::NetworkStateHandler* handler);

  // NetworkStateHandlerObserver overrides
  void DefaultNetworkChanged(const ash::NetworkState* network) override;
  void OnShuttingDown() override;

  void OnDeviceHostnamePropertyChanged();

  void OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded();

  // Returns the device name policy to be used. If kPolicyHostnameChosenByAdmin
  // is returned, |hostname_template_out| is set to the template chosen by the
  // administrator; otherwise, |hostname_template_out| is left unchanged.
  DeviceNamePolicy ComputePolicy(std::string* hostname_template_out);

  // Generates the hostname according to the template chosen by the
  // administrator.
  std::string GenerateHostname(const std::string& hostname_template) const;

  // Sets new device name and policy if different from the current device name
  // and/or policy.
  void SetDeviceNamePolicy(DeviceNamePolicy policy,
                           const std::string& new_hostname);

  const raw_ref<BrowserPolicyConnectorAsh> browser_policy_connector_ash_;
  const raw_ref<ash::CrosSettings> cros_settings_;
  const raw_ref<ash::system::StatisticsProvider> statistics_provider_;
  const raw_ref<ash::NetworkStateHandler> handler_;

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  DeviceNamePolicy device_name_policy_;

  base::CallbackListSubscription template_policy_subscription_;
  base::CallbackListSubscription configurable_policy_subscription_;
  std::string hostname_;
  base::WeakPtrFactory<DeviceNamePolicyHandler> weak_factory_{this};
};

std::ostream& operator<<(
    std::ostream& stream,
    const DeviceNamePolicyHandler::DeviceNamePolicy& state);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_H_
