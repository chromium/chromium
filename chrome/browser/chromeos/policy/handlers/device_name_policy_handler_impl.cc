// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/handlers/device_name_policy_handler_impl.h"

#include "base/bind.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_chromeos.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/handlers/device_name_policy_handler_name_generator.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"

namespace policy {

DeviceNamePolicyHandlerImpl::DeviceNamePolicyHandlerImpl(
    ash::CrosSettings* cros_settings)
    : DeviceNamePolicyHandlerImpl(
          cros_settings,
          chromeos::system::StatisticsProvider::GetInstance()) {}

DeviceNamePolicyHandlerImpl::DeviceNamePolicyHandlerImpl(
    ash::CrosSettings* cros_settings,
    chromeos::system::StatisticsProvider* statistics_provider)
    : cros_settings_(cros_settings), statistics_provider_(statistics_provider) {
  policy_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kDeviceHostnameTemplate,
      base::BindRepeating(
          &DeviceNamePolicyHandlerImpl::OnDeviceHostnamePropertyChanged,
          weak_factory_.GetWeakPtr()));
  chromeos::NetworkHandler::Get()->network_state_handler()->AddObserver(
      this, FROM_HERE);

  // Fire it once so we're sure we get an invocation on startup.
  OnDeviceHostnamePropertyChanged();
}

DeviceNamePolicyHandlerImpl::~DeviceNamePolicyHandlerImpl() {
  if (chromeos::NetworkHandler::IsInitialized()) {
    chromeos::NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
}

const std::string& DeviceNamePolicyHandlerImpl::GetDeviceHostname() const {
  return hostname_;
}

DeviceNamePolicyHandler::DeviceNamePolicy
DeviceNamePolicyHandlerImpl::GetDeviceNamePolicy() const {
  return device_name_policy_;
}

absl::optional<std::string>
DeviceNamePolicyHandlerImpl::GetHostnameChosenByAdministrator() const {
  if (GetDeviceNamePolicy() == DeviceNamePolicy::kPolicyHostnameChosenByAdmin) {
    return hostname_;
  }
  return absl::nullopt;
}

void DeviceNamePolicyHandlerImpl::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  OnDeviceHostnamePropertyChanged();
}

void DeviceNamePolicyHandlerImpl::OnDeviceHostnamePropertyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &DeviceNamePolicyHandlerImpl::OnDeviceHostnamePropertyChanged,
          weak_factory_.GetWeakPtr()));
  if (status != chromeos::CrosSettingsProvider::TRUSTED)
    return;

  // Continue when machine statistics are loaded, to avoid blocking.
  statistics_provider_->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
      &DeviceNamePolicyHandlerImpl::
          OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded,
      weak_factory_.GetWeakPtr()));
}

void DeviceNamePolicyHandlerImpl::
    OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded() {
  std::string hostname_template;
  if (!cros_settings_->GetString(chromeos::kDeviceHostnameTemplate,
                                 &hostname_template)) {
    // Do not set an empty hostname (which would overwrite any custom hostname
    // set) if DeviceHostnameTemplate is not specified by policy.
    // No policy is set for administrator to choose hostname.
    SetDeviceNamePolicy(DeviceNamePolicy::kNoPolicy);
    return;
  }

  // If we reach here, we know that the administrator specified a template
  // to generate the hostname of the device.
  device_name_policy_ = DeviceNamePolicy::kPolicyHostnameChosenByAdmin;

  const std::string serial = chromeos::system::StatisticsProvider::GetInstance()
                                 ->GetEnterpriseMachineID();

  const std::string asset_id = g_browser_process->platform_part()
                                   ->browser_policy_connector_chromeos()
                                   ->GetDeviceAssetID();

  const std::string machine_name = g_browser_process->platform_part()
                                       ->browser_policy_connector_chromeos()
                                       ->GetMachineName();

  const std::string location = g_browser_process->platform_part()
                                   ->browser_policy_connector_chromeos()
                                   ->GetDeviceAnnotatedLocation();

  chromeos::NetworkStateHandler* handler =
      chromeos::NetworkHandler::Get()->network_state_handler();

  std::string mac = "MAC_unknown";
  const chromeos::NetworkState* network = handler->DefaultNetwork();
  if (network) {
    const chromeos::DeviceState* device =
        handler->GetDeviceState(network->device_path());
    if (device) {
      mac = device->mac_address();
      base::ReplaceSubstringsAfterOffset(&mac, 0, ":", "");
    }
  }

  hostname_ = policy::FormatHostname(hostname_template, asset_id, serial, mac,
                                     machine_name, location);
  handler->SetHostname(hostname_);

  // Notify at the end only after changing both policy and hostname.
  NotifyHostnamePolicyChanged();
}

void DeviceNamePolicyHandlerImpl::SetDeviceNamePolicy(DeviceNamePolicy policy) {
  if (device_name_policy_ != policy) {
    device_name_policy_ = policy;
    NotifyHostnamePolicyChanged();
  }
}

std::ostream& operator<<(
    std::ostream& stream,
    const DeviceNamePolicyHandlerImpl::DeviceNamePolicy& state) {
  switch (state) {
    case DeviceNamePolicyHandlerImpl::DeviceNamePolicy::kNoPolicy:
      stream << "[No policy]";
      break;
    case DeviceNamePolicyHandlerImpl::DeviceNamePolicy::
        kPolicyHostnameChosenByAdmin:
      stream << "[Admin chooses hostname template]";
      break;
  }
  return stream;
}

}  // namespace policy
