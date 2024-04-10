// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/networking/device_network_configuration_updater_ash.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"

namespace policy {

namespace {

std::string GetDeviceAssetID() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_ash()
      ->GetDeviceAssetID();
}

}  // namespace

DeviceNetworkConfigurationUpdaterAsh::~DeviceNetworkConfigurationUpdaterAsh() =
    default;

// static
std::unique_ptr<DeviceNetworkConfigurationUpdaterAsh>
DeviceNetworkConfigurationUpdaterAsh::CreateForDevicePolicy(
    PolicyService* policy_service,
    ash::ManagedNetworkConfigurationHandler* network_config_handler,
    ash::NetworkDeviceHandler* network_device_handler,
    ash::CrosSettings* cros_settings,
    const DeviceNetworkConfigurationUpdaterAsh::DeviceAssetIDFetcher&
        device_asset_id_fetcher) {
  std::unique_ptr<DeviceNetworkConfigurationUpdaterAsh> updater(
      new DeviceNetworkConfigurationUpdaterAsh(
          policy_service, network_config_handler, network_device_handler,
          cros_settings, device_asset_id_fetcher));
  updater->Init();
  return updater;
}

DeviceNetworkConfigurationUpdaterAsh::DeviceNetworkConfigurationUpdaterAsh(
    PolicyService* policy_service,
    ash::ManagedNetworkConfigurationHandler* network_config_handler,
    ash::NetworkDeviceHandler* network_device_handler,
    ash::CrosSettings* cros_settings,
    const DeviceNetworkConfigurationUpdaterAsh::DeviceAssetIDFetcher&
        device_asset_id_fetcher)
    : NetworkConfigurationUpdater(onc::ONC_SOURCE_DEVICE_POLICY,
                                  key::kDeviceOpenNetworkConfiguration,
                                  policy_service),
      network_config_handler_(network_config_handler),
      network_device_handler_(network_device_handler),
      cros_settings_(cros_settings),
      device_asset_id_fetcher_(device_asset_id_fetcher) {
  DCHECK(network_device_handler_);
  data_roaming_setting_subscription_ = cros_settings->AddSettingsObserver(
      ash::kSignedDataRoamingEnabled,
      base::BindRepeating(
          &DeviceNetworkConfigurationUpdaterAsh::OnDataRoamingSettingChanged,
          base::Unretained(this)));
  if (device_asset_id_fetcher_.is_null())
    device_asset_id_fetcher_ = base::BindRepeating(&GetDeviceAssetID);
}

void DeviceNetworkConfigurationUpdaterAsh::Init() {
  NetworkConfigurationUpdater::Init();

  // The highest authority regarding whether cellular data roaming should be
  // allowed is the Device Policy. If there is no Device Policy, then
  // data roaming should be allowed if this is a Cellular First device.
  if (!ash::InstallAttributes::Get()->IsEnterpriseManaged() &&
      ash::switches::IsCellularFirstDevice()) {
    network_device_handler_->SetCellularPolicyAllowRoaming(
        /*policy_allow_roaming=*/true);
  } else {
    // Apply the roaming setting initially.
    OnDataRoamingSettingChanged();
  }

  // Set up MAC address randomization if we are not enterprise managed.

  network_device_handler_->SetMACAddressRandomizationEnabled(
      !ash::InstallAttributes::Get()->IsEnterpriseManaged());
}

void DeviceNetworkConfigurationUpdaterAsh::ImportClientCertificates() {
  LOG(WARNING)
      << "Importing client certificates from device policy is not implemented.";
}

void DeviceNetworkConfigurationUpdaterAsh::ApplyNetworkPolicy(
    const base::Value::List& network_configs_onc,
    const base::Value::Dict& global_network_config) {
  // Ensure this is runnng on the UI thead because we're accessing global data
  // to populate the substitutions.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Expand device-specific placeholders. Note: It is OK that if the serial
  // number or Asset ID are empty, the placeholders will be expanded to an empty
  // string. This is to be consistent with user policy identity string
  // expansions.
  base::flat_map<std::string, std::string> substitutions;
  substitutions[::onc::substitutes::kDeviceSerialNumber] = std::string(
      ash::system::StatisticsProvider::GetInstance()->GetMachineID().value_or(
          ""));
  substitutions[::onc::substitutes::kDeviceAssetId] =
      device_asset_id_fetcher_.Run();

  network_config_handler_->SetProfileWideVariableExpansions(
      /*userhash=*/std::string(), std::move(substitutions));
  network_config_handler_->SetPolicy(onc_source_, /*userhash=*/std::string(),
                                     network_configs_onc,
                                     global_network_config);
}

void DeviceNetworkConfigurationUpdaterAsh::OnDataRoamingSettingChanged() {
  ash::CrosSettingsProvider::TrustedStatus trusted_status =
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &DeviceNetworkConfigurationUpdaterAsh::OnDataRoamingSettingChanged,
          weak_factory_.GetWeakPtr()));

  if (trusted_status == ash::CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    // Return, this function will be called again later by
    // PrepareTrustedValues.
    return;
  }

  bool data_roaming_setting = false;
  if (trusted_status == ash::CrosSettingsProvider::TRUSTED) {
    if (!cros_settings_->GetBoolean(ash::kSignedDataRoamingEnabled,
                                    &data_roaming_setting)) {
      LOG(ERROR) << "Couldn't get device setting "
                 << ash::kSignedDataRoamingEnabled;
      data_roaming_setting = false;
    }
  } else {
    DCHECK_EQ(trusted_status, ash::CrosSettingsProvider::PERMANENTLY_UNTRUSTED);
    // Roaming is disabled as we can't determine the correct setting.
  }

  // Roaming is disabled by policy only when the device is both enterprise
  // managed and the value of |data_roaming_setting| is |false|.
  const bool policy_allow_roaming =
      ash::InstallAttributes::Get()->IsEnterpriseManaged()
          ? data_roaming_setting
          : true;

  network_device_handler_->SetCellularPolicyAllowRoaming(policy_allow_roaming);
}

}  // namespace policy
