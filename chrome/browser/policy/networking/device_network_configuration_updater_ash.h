// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_NETWORKING_DEVICE_NETWORK_CONFIGURATION_UPDATER_ASH_H_
#define CHROME_BROWSER_POLICY_NETWORKING_DEVICE_NETWORK_CONFIGURATION_UPDATER_ASH_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/policy/networking/network_configuration_updater.h"
#include "components/onc/onc_constants.h"
#include "net/cert/x509_certificate.h"

namespace ash {
class CrosSettings;
class ManagedNetworkConfigurationHandler;
class NetworkDeviceHandler;
}  // namespace ash

namespace base {
class Value;
}  // namespace base

namespace policy {

class PolicyService;

// Implements addtional special handling of ONC device policies, which requires
// listening for notifications from CrosSettings.
class DeviceNetworkConfigurationUpdaterAsh
    : public NetworkConfigurationUpdater {
 public:
  DeviceNetworkConfigurationUpdaterAsh(
      const DeviceNetworkConfigurationUpdaterAsh&) = delete;
  DeviceNetworkConfigurationUpdaterAsh& operator=(
      const DeviceNetworkConfigurationUpdaterAsh&) = delete;

  ~DeviceNetworkConfigurationUpdaterAsh() override;

  // Fetches the device's administrator-annotated asset ID.
  using DeviceAssetIDFetcher = base::RepeatingCallback<std::string()>;

  // Creates an updater that applies the ONC device policy from |policy_service|
  // once the policy service is completely initialized and on each policy
  // change. The argument objects passed as pointers must outlive the returned
  // updater. |device_assed_id_fetcher| should return the
  // administrator-annotated asset ID of the device and is used for variable
  // replacement. If a null callback is passed, the asset ID from device policy
  // will be used.
  static std::unique_ptr<DeviceNetworkConfigurationUpdaterAsh>
  CreateForDevicePolicy(
      PolicyService* policy_service,
      ash::ManagedNetworkConfigurationHandler* network_config_handler,
      ash::NetworkDeviceHandler* network_device_handler,
      ash::CrosSettings* cros_settings,
      const DeviceAssetIDFetcher& device_asset_id_fetcher);

 private:
  DeviceNetworkConfigurationUpdaterAsh(
      PolicyService* policy_service,
      ash::ManagedNetworkConfigurationHandler* network_config_handler,
      ash::NetworkDeviceHandler* network_device_handler,
      ash::CrosSettings* cros_settings,
      const DeviceAssetIDFetcher& device_asset_id_fetcher);

  // NetworkConfigurationUpdater:
  void Init() override;
  void ImportClientCertificates() override;
  void ApplyNetworkPolicy(
      const base::Value::List& network_configs_onc,
      const base::Value::Dict& global_network_config) override;
  void OnDataRoamingSettingChanged();

  // Pointer to the global singleton or a test instance.
  const raw_ptr<ash::ManagedNetworkConfigurationHandler>
      network_config_handler_;

  const raw_ptr<ash::NetworkDeviceHandler> network_device_handler_;
  const raw_ptr<ash::CrosSettings> cros_settings_;
  base::CallbackListSubscription data_roaming_setting_subscription_;

  // Returns the device's administrator-set asset id.
  DeviceAssetIDFetcher device_asset_id_fetcher_;

  base::WeakPtrFactory<DeviceNetworkConfigurationUpdaterAsh> weak_factory_{
      this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_NETWORKING_DEVICE_NETWORK_CONFIGURATION_UPDATER_ASH_H_
