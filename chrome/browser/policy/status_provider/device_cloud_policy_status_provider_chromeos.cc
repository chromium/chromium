// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/device_cloud_policy_status_provider_chromeos.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"

DeviceCloudPolicyStatusProviderChromeOS::
    DeviceCloudPolicyStatusProviderChromeOS(
        const policy::BrowserPolicyConnectorAsh* connector,
        Profile* profile)
    : CloudPolicyCoreStatusProvider(connector->GetDeviceCloudPolicyManager(),
                                    profile) {
  enterprise_domain_manager_ = connector->GetEnterpriseDomainManager();
}

DeviceCloudPolicyStatusProviderChromeOS::
    ~DeviceCloudPolicyStatusProviderChromeOS() = default;

base::DictValue DeviceCloudPolicyStatusProviderChromeOS::GetStatus() {
  base::DictValue dict =
      policy::PolicyStatusProvider::GetStatusFromCore(core());
  dict.Set(policy::kEnterpriseDomainManagerKey, enterprise_domain_manager_);
  dict.Set(policy::kPolicyDescriptionKey, kDevicePolicyStatusDescription);
  if (auto off_hours_status = GetOffHoursStatus()) {
    dict.Set("isOffHoursActive", off_hours_status.value());
  }
  return dict;
}

policy::mojom::StatusPtr
DeviceCloudPolicyStatusProviderChromeOS::GetStatusMojo() {
  auto status = policy::mojom::Status::New();
  policy::PolicyStatusProvider::PopulateStatusFromCore(
      core(), /*is_extension_install_policy=*/false, status);
  status->enterprise_domain_manager = enterprise_domain_manager_;
  status->policy_description_key = kDevicePolicyStatusDescription;
  status->is_off_hours_active = GetOffHoursStatus();
  return status;
}
