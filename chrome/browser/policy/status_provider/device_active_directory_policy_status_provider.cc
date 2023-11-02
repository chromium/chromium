// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/device_active_directory_policy_status_provider.h"

#include "base/values.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"

DeviceActiveDirectoryPolicyStatusProvider::
    DeviceActiveDirectoryPolicyStatusProvider(
        policy::ActiveDirectoryPolicyManager* policy_manager,
        const std::string& enterprise_domain_manager)
    : UserActiveDirectoryPolicyStatusProvider(policy_manager, nullptr),
      enterprise_domain_manager_(enterprise_domain_manager) {}

base::Value::Dict DeviceActiveDirectoryPolicyStatusProvider::GetStatus() {
  base::Value::Dict dict = UserActiveDirectoryPolicyStatusProvider::GetStatus();
  dict.Set(policy::kEnterpriseDomainManagerKey, enterprise_domain_manager_);
  dict.Set(policy::kPolicyDescriptionKey, kDevicePolicyStatusDescription);
  return dict;
}
