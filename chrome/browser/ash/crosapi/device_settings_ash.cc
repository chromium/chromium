// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_settings_ash.h"

#include <utility>

#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_pref_names.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/policy/status_provider/device_cloud_policy_status_provider_chromeos.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/management/management_ui_handler_chromeos.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

DeviceSettingsAsh::DeviceSettingsAsh() {
  if (ash::DeviceSettingsService::IsInitialized())
    ash::DeviceSettingsService::Get()->AddObserver(this);
}

DeviceSettingsAsh::~DeviceSettingsAsh() {
  if (ash::DeviceSettingsService::IsInitialized())
    ash::DeviceSettingsService::Get()->RemoveObserver(this);
}

void DeviceSettingsAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DeviceSettingsService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void DeviceSettingsAsh::DeviceSettingsUpdated() {
  for (auto& observer : observers_)
    observer->UpdateDeviceSettings(browser_util::GetDeviceSettings());
}

void DeviceSettingsAsh::AddDeviceSettingsObserver(
    mojo::PendingRemote<mojom::DeviceSettingsObserver> observer) {
  mojo::Remote<mojom::DeviceSettingsObserver> remote(std::move(observer));
  observers_.Add(std::move(remote));
}

void DeviceSettingsAsh::GetDevicePolicy(GetDevicePolicyCallback callback) {
  const policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector->IsDeviceEnterpriseManaged()) {
    std::move(callback).Run(base::Value::Dict(), base::Value::Dict());
    return;
  }

  auto client = std::make_unique<policy::ChromePolicyConversionsClient>(
      ProfileManager::GetActiveUserProfile());
  client->EnableUserPolicies(false);
  client->EnableConvertValues(true);
  DeviceCloudPolicyStatusProviderChromeOS provider(connector);
  base::Value::Dict status = provider.GetStatus();
  std::move(callback).Run(client->GetChromePolicies(), std::move(status));
}

void DeviceSettingsAsh::GetDevicePolicyDeprecated(
    GetDevicePolicyDeprecatedCallback callback) {
  const policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector->IsDeviceEnterpriseManaged()) {
    std::move(callback).Run(base::Value(), base::Value());
    return;
  }

  auto client = std::make_unique<policy::ChromePolicyConversionsClient>(
      ProfileManager::GetActiveUserProfile());
  client->EnableUserPolicies(false);
  DeviceCloudPolicyStatusProviderChromeOS provider(connector);
  base::Value::Dict status = provider.GetStatus();
  std::move(callback).Run(base::Value(client->GetChromePolicies()),
                          base::Value(std::move(status)));
}

void DeviceSettingsAsh::GetDeviceReportSources(
    GetDeviceReportSourcesCallback callback) {
  const policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  if (!connector->IsDeviceEnterpriseManaged()) {
    std::move(callback).Run(base::Value::List(),
                            /*plugin_vm_data_collection_enabled=*/false);
    return;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  Profile* profile = ash::ProfileHelper::Get()->GetProfileByUser(user);
  base::Value::List report_sources =
      ManagementUIHandlerChromeOS::GetDeviceReportingInfo(
          connector->GetDeviceCloudPolicyManager(), profile);
  bool plugin_vm_data_collection_enabled = profile->GetPrefs()->GetBoolean(
      plugin_vm::prefs::kPluginVmDataCollectionAllowed);
  std::move(callback).Run(std::move(report_sources),
                          plugin_vm_data_collection_enabled);
}

void DeviceSettingsAsh::IsDeviceDeprovisioned(
    IsDeviceDeprovisionedCallback callback) {
  auto is_deprovisioned =
      ::ash::DeviceSettingsService::IsInitialized() &&
      ::ash::DeviceSettingsService::Get()->policy_data() &&
      ::ash::DeviceSettingsService::Get()->policy_data()->state() ==
          enterprise_management::PolicyData::DEPROVISIONED;
  std::move(callback).Run(is_deprovisioned);
}

}  // namespace crosapi
