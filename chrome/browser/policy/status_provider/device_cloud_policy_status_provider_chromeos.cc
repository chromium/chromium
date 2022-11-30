// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/device_cloud_policy_status_provider_chromeos.h"

#include <string>

#include "base/values.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"

DeviceCloudPolicyStatusProviderChromeOS::
    DeviceCloudPolicyStatusProviderChromeOS(
        const policy::BrowserPolicyConnectorAsh* connector)
    : CloudPolicyCoreStatusProvider(
          connector->GetDeviceCloudPolicyManager()->core()) {
  enterprise_domain_manager_ = connector->GetEnterpriseDomainManager();
}

DeviceCloudPolicyStatusProviderChromeOS::
    ~DeviceCloudPolicyStatusProviderChromeOS() = default;

base::Value::Dict DeviceCloudPolicyStatusProviderChromeOS::GetStatus() {
  base::Value::Dict dict =
      policy::PolicyStatusProvider::GetStatusFromCore(core_);
  dict.Set(policy::kEnterpriseDomainManagerKey, enterprise_domain_manager_);
  dict.Set(policy::kPolicyDescriptionKey, kDevicePolicyStatusDescription);
  GetOffHoursStatus(&dict);
  return dict;
}
