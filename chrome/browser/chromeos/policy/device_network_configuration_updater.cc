// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_network_configuration_updater.h"

#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/onc/onc_parsed_certificates.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/variable_expander.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"

namespace policy {

namespace {

std::string GetDeviceAssetID() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->GetDeviceAssetID();
}

}  // namespace

DeviceNetworkConfigurationUpdater::~DeviceNetworkConfigurationUpdater() {}

// static
std::unique_ptr<DeviceNetworkConfigurationUpdater>
DeviceNetworkConfigurationUpdater::CreateForDevicePolicy(
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
    chromeos::NetworkDeviceHandler* network_device_handler,
    chromeos::CrosSettings* cros_settings,
    const DeviceNetworkConfigurationUpdater::DeviceAssetIDFetcher&
        device_asset_id_fetcher) {
  std::unique_ptr<DeviceNetworkConfigurationUpdater> updater(
      new DeviceNetworkConfigurationUpdater(
          policy_service, network_config_handler, network_device_handler,
          cros_settings, device_asset_id_fetcher));
  updater->Init();
  return updater;
}

DeviceNetworkConfigurationUpdater::DeviceNetworkConfigurationUpdater(
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
    chromeos::NetworkDeviceHandler* network_device_handler,
    chromeos::CrosSettings* cros_settings,
    const DeviceNetworkConfigurationUpdater::DeviceAssetIDFetcher&
        device_asset_id_fetcher)
    : NetworkConfigurationUpdater(onc::ONC_SOURCE_DEVICE_POLICY,
                                  key::kDeviceOpenNetworkConfiguration,
                                  policy_service,
                                  network_config_handler),
      network_device_handler_(network_device_handler),
      cros_settings_(cros_settings),
      device_asset_id_fetcher_(device_asset_id_fetcher) {
  DCHECK(network_device_handler_);
  data_roaming_setting_subscription_ = cros_settings->AddSettingsObserver(
      chromeos::kSignedDataRoamingEnabled,
      base::Bind(
          &DeviceNetworkConfigurationUpdater::OnDataRoamingSettingChanged,
          base::Unretained(this)));
  if (device_asset_id_fetcher_.is_null())
    device_asset_id_fetcher_ = base::BindRepeating(&GetDeviceAssetID);
}

void DeviceNetworkConfigurationUpdater::Init() {
  NetworkConfigurationUpdater::Init();

  const policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();

  // The highest authority regarding whether cellular data roaming should be
  // allowed is the Device Policy. If there is no Device Policy, then
  // data roaming should be allowed if this is a Cellular First device.
  if (!connector->IsEnterpriseManaged() &&
      chromeos::switches::IsCellularFirstDevice()) {
    network_device_handler_->SetCellularAllowRoaming(true);
  } else {
    // Apply the roaming setting initially.
    OnDataRoamingSettingChanged();
  }

  // Set up MAC address randomization if we are not enterprise managed.

  network_device_handler_->SetMACAddressRandomizationEnabled(
      !connector->IsEnterpriseManaged());
}

void DeviceNetworkConfigurationUpdater::ImportClientCertificates() {
  // Importing client certificates from device policy is not implemented.
}

void DeviceNetworkConfigurationUpdater::ApplyNetworkPolicy(
    base::ListValue* network_configs_onc,
    base::DictionaryValue* global_network_config) {
  // Ensure this is runnng on the UI thead because we're accessing global data
  // to populate the substitutions.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Expand device-specific placeholders. Note: It is OK that if the serial
  // number or Asset ID are empty, the placeholders will be expanded to an empty
  // string. This is to be consistent with user policy identity string
  // expansions.
  std::map<std::string, std::string> substitutions;
  substitutions[::onc::substitutes::kDeviceSerialNumber] =
      chromeos::system::StatisticsProvider::GetInstance()
          ->GetEnterpriseMachineID();
  substitutions[::onc::substitutes::kDeviceAssetId] =
      device_asset_id_fetcher_.Run();

  chromeos::VariableExpander variable_expander(std::move(substitutions));
  chromeos::onc::ExpandStringsInNetworks(variable_expander,
                                         network_configs_onc);

  network_config_handler_->SetPolicy(onc_source_,
                                     std::string() /* no username hash */,
                                     *network_configs_onc,
                                     *global_network_config);
}

void DeviceNetworkConfigurationUpdater::OnDataRoamingSettingChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus trusted_status =
      cros_settings_->PrepareTrustedValues(base::Bind(
          &DeviceNetworkConfigurationUpdater::OnDataRoamingSettingChanged,
          weak_factory_.GetWeakPtr()));

  if (trusted_status == chromeos::CrosSettingsProvider::TEMPORARILY_UNTRUSTED) {
    // Return, this function will be called again later by
    // PrepareTrustedValues.
    return;
  }

  bool data_roaming_setting = false;
  if (trusted_status == chromeos::CrosSettingsProvider::TRUSTED) {
    if (!cros_settings_->GetBoolean(chromeos::kSignedDataRoamingEnabled,
                                    &data_roaming_setting)) {
      LOG(ERROR) << "Couldn't get device setting "
                 << chromeos::kSignedDataRoamingEnabled;
      data_roaming_setting = false;
    }
  } else {
    DCHECK_EQ(trusted_status,
              chromeos::CrosSettingsProvider::PERMANENTLY_UNTRUSTED);
    // Roaming is disabled as we can't determine the correct setting.
  }

  network_device_handler_->SetCellularAllowRoaming(data_roaming_setting);
}

}  // namespace policy
