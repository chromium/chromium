// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/device_settings_ash.h"

#include <utility>

#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/policy/status_provider/device_cloud_policy_status_provider_chromeos.h"
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
    std::move(callback).Run(base::Value(), base::Value());
    return;
  }

  auto client = std::make_unique<policy::ChromePolicyConversionsClient>(
      ProfileManager::GetActiveUserProfile());
  client->EnableUserPolicies(false);
  DeviceCloudPolicyStatusProviderChromeOS provider(connector);
  base::DictionaryValue status;
  provider.GetStatus(&status);
  std::move(callback).Run(base::Value(client->GetChromePolicies()),
                          std::move(status));
}

}  // namespace crosapi
