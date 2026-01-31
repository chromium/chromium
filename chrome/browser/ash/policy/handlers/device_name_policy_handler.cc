// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"

#include <string_view>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler_name_generator.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/device_state.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"

namespace policy {

namespace {

// By default, device name policy should be kPolicyHostnameNotConfigurable for
// managed devices and kNoPolicy for unmanaged devices.
DeviceNamePolicyHandler::DeviceNamePolicy ComputeInitialPolicy() {
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    // We assume that the device name is not configurable unless/until we know
    // about any policies that are set.
    return DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameNotConfigurable;
  }

  return DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy;
}

}  // namespace

DeviceNamePolicyHandler::DeviceNamePolicyHandler(
    BrowserPolicyConnectorAsh* browser_policy_connector_ash,
    ash::CrosSettings* cros_settings)
    : DeviceNamePolicyHandler(
          browser_policy_connector_ash,
          cros_settings,
          ash::system::StatisticsProvider::GetInstance(),
          ash::NetworkHandler::Get()->network_state_handler()) {}

DeviceNamePolicyHandler::DeviceNamePolicyHandler(
    BrowserPolicyConnectorAsh* browser_policy_connector_ash,
    ash::CrosSettings* cros_settings,
    ash::system::StatisticsProvider* statistics_provider,
    ash::NetworkStateHandler* handler)
    : browser_policy_connector_ash_(CHECK_DEREF(browser_policy_connector_ash)),
      cros_settings_(CHECK_DEREF(cros_settings)),
      statistics_provider_(CHECK_DEREF(statistics_provider)),
      handler_(CHECK_DEREF(handler)),
      device_name_policy_(ComputeInitialPolicy()) {
  template_policy_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kDeviceHostnameTemplate,
      base::BindRepeating(
          &DeviceNamePolicyHandler::OnDeviceHostnamePropertyChanged,
          weak_factory_.GetWeakPtr()));
  configurable_policy_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kDeviceHostnameUserConfigurable,
      base::BindRepeating(
          &DeviceNamePolicyHandler::OnDeviceHostnamePropertyChanged,
          weak_factory_.GetWeakPtr()));

  network_state_handler_observer_.Observe(
      ash::NetworkHandler::Get()->network_state_handler());

  // Fire it once so we're sure we get an invocation on startup.
  OnDeviceHostnamePropertyChanged();
}

DeviceNamePolicyHandler::~DeviceNamePolicyHandler() = default;

std::optional<std::string>
DeviceNamePolicyHandler::GetHostnameChosenByAdministrator() const {
  if (device_name_policy_ == DeviceNamePolicy::kPolicyHostnameChosenByAdmin) {
    return hostname_;
  }
  return std::nullopt;
}

void DeviceNamePolicyHandler::DefaultNetworkChanged(
    const ash::NetworkState* network) {
  OnDeviceHostnamePropertyChanged();
}

void DeviceNamePolicyHandler::OnShuttingDown() {
  network_state_handler_observer_.Reset();
}

void DeviceNamePolicyHandler::OnDeviceHostnamePropertyChanged() {
  ash::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &DeviceNamePolicyHandler::OnDeviceHostnamePropertyChanged,
          weak_factory_.GetWeakPtr()));
  if (status != ash::CrosSettingsProvider::TRUSTED) {
    return;
  }

  // Continue when machine statistics are loaded, to avoid blocking.
  statistics_provider_->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
      &DeviceNamePolicyHandler::
          OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded,
      weak_factory_.GetWeakPtr()));
}

void DeviceNamePolicyHandler::
    OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded() {
  std::string hostname_template;
  DeviceNamePolicy policy = ComputePolicy(&hostname_template);

  std::string new_hostname;
  if (policy == DeviceNamePolicy::kPolicyHostnameChosenByAdmin) {
    new_hostname = GenerateHostname(hostname_template);
  }

  SetDeviceNamePolicy(policy, new_hostname);
}

DeviceNamePolicyHandler::DeviceNamePolicy
DeviceNamePolicyHandler::ComputePolicy(std::string* hostname_template_out) {
  if (cros_settings_->GetString(ash::kDeviceHostnameTemplate,
                                hostname_template_out)) {
    // Do not set an empty hostname (which would overwrite any custom hostname
    // set) if DeviceHostnameTemplate is not specified by policy.
    // No policy is set for administrator to choose hostname.
    return DeviceNamePolicy::kPolicyHostnameChosenByAdmin;
  }

  // If no policies are set, device name policy should be
  // kPolicyHostnameNotConfigurable for managed devices and kNoPolicy for
  // unmanaged devices.
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    return DeviceNamePolicy::kPolicyHostnameNotConfigurable;
  }

  return DeviceNamePolicy::kNoPolicy;
}

std::string DeviceNamePolicyHandler::GenerateHostname(
    const std::string& hostname_template) const {
  const std::string_view serial =
      statistics_provider_->GetMachineID().value_or(std::string_view());

  const std::string asset_id =
      browser_policy_connector_ash_->GetDeviceAssetID();
  const std::string machine_name =
      browser_policy_connector_ash_->GetMachineName();
  const std::string location =
      browser_policy_connector_ash_->GetDeviceAnnotatedLocation();

  std::string mac = "MAC_unknown";
  const ash::NetworkState* network = handler_->DefaultNetwork();
  if (network) {
    const ash::DeviceState* device =
        handler_->GetDeviceState(network->device_path());
    if (device) {
      mac = device->mac_address();
      base::ReplaceSubstringsAfterOffset(&mac, 0, ":", "");
    }
  }

  return FormatHostname(hostname_template, asset_id, serial, mac, machine_name,
                        location);
}

void DeviceNamePolicyHandler::SetDeviceNamePolicy(
    DeviceNamePolicy policy,
    const std::string& new_hostname) {
  if (device_name_policy_ == policy && hostname_ == new_hostname) {
    return;
  }

  // If the hostname has changed, set it using NetworkStateHandler.
  if (policy == DeviceNamePolicy::kPolicyHostnameChosenByAdmin &&
      hostname_ != new_hostname) {
    handler_->SetHostname(new_hostname);
  }

  device_name_policy_ = policy;
  hostname_ = new_hostname;
}

std::ostream& operator<<(
    std::ostream& stream,
    const DeviceNamePolicyHandler::DeviceNamePolicy& state) {
  switch (state) {
    case DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy:
      stream << "[No policy]";
      break;
    case DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameChosenByAdmin:
      stream << "[Admin chooses hostname template]";
      break;
    case DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameNotConfigurable:
      stream << "[Managed user cannot choose hostname]";
      break;
  }
  return stream;
}

}  // namespace policy
