// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/extension_install_policy_service.h"

#include <set>
#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_client_types.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/pref_names.h"

namespace policy {

namespace {

bool IsExtensionInstallBlocked(
    const PolicyMap::Entry& entry,
    const ExtensionIdAndVersion& extension_id_and_version) {
  const base::Value* policy_value = entry.value(base::Value::Type::DICT);
  if (!policy_value) {
    return false;
  }

  auto* value_for_version = policy_value->GetDict().FindDict(
      extension_id_and_version.extension_version);
  if (!value_for_version) {
    return true;
  }

  return value_for_version->FindInt("action").value_or(
             enterprise_management::ExtensionInstallPolicy::ACTION_ALLOW) ==
         enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK;
}

}  // namespace
ExtensionInstallPolicyService::ExtensionInstallPolicyService(Profile* profile)
    : profile_(profile) {}

ExtensionInstallPolicyService::~ExtensionInstallPolicyService() = default;

void ExtensionInstallPolicyService::CanInstallExtension(
    const ExtensionIdAndVersion& extension_id_and_version,
    base::OnceCallback<void(bool)> callback) {
  if (!profile_->GetPrefs()->GetBoolean(
          extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled)) {
    std::move(callback).Run(true);
    return;
  }

  CloudPolicyManager* user_cloud_policy_manager =
      profile_->GetCloudPolicyManager();
  MachineLevelUserCloudPolicyManager* machine_cloud_policy_manager = nullptr;
#if !BUILDFLAG(IS_CHROMEOS)
  machine_cloud_policy_manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
#endif  // !BUILDFLAG(IS_CHROMEOS)

  size_t callback_count = 0;
  if (user_cloud_policy_manager) {
    ++callback_count;
  }
  if (machine_cloud_policy_manager) {
    ++callback_count;
  }
  if (callback_count == 0) {
    std::move(callback).Run(true);
    return;
  }

  base::RepeatingCallback<void(ExtensionInstallDecision)> barrier_callback =
      base::BarrierCallback<ExtensionInstallDecision>(
          callback_count,
          base::BindOnce(
              [](base::OnceCallback<void(bool)> inner_callback,
                 const std::vector<ExtensionInstallDecision>& values) {
                bool can_install = true;
                for (const auto& value : values) {
                  if (value.action ==
                      enterprise_management::ExtensionInstallPolicy::
                          ACTION_BLOCK) {
                    can_install = false;
                    break;
                  }
                }
                std::move(inner_callback).Run(can_install);
              },
              std::move(callback)));

  if (user_cloud_policy_manager) {
    user_cloud_policy_manager->core()
        ->extension_install_service()
        ->FetchExtensionInstallPolicy(
            dm_protocol::kChromeExtensionInstallUserCloudPolicyType,
            extension_id_and_version, PolicyFetchReason::kExtensionInstall,
            barrier_callback);
  }
  if (machine_cloud_policy_manager) {
    machine_cloud_policy_manager->core()
        ->extension_install_service()
        ->FetchExtensionInstallPolicy(
            dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType,
            extension_id_and_version, PolicyFetchReason::kExtensionInstall,
            barrier_callback);
  }
}

std::optional<bool> ExtensionInstallPolicyService::IsExtensionAllowed(
    const ExtensionIdAndVersion& extension_id_and_version) {
  auto* policy_service =
      profile_->GetProfilePolicyConnector()->policy_service();
  if (!policy_service) {
    return std::nullopt;
  }

  if (!policy_service->IsInitializationComplete(
          policy::POLICY_DOMAIN_EXTENSION_INSTALL)) {
    return std::nullopt;
  }

  const PolicyMap& extension_install_policy_map =
      policy_service->GetPolicies(policy::PolicyNamespace(
          policy::POLICY_DOMAIN_EXTENSION_INSTALL, std::string()));

  const PolicyMap::Entry* entry =
      extension_install_policy_map.Get(extension_id_and_version.extension_id);
  if (!IsExtensionInstallBlocked(*entry, extension_id_and_version)) {
    return false;
  }

  for (const auto& conflict : entry->conflicts) {
    if (!IsExtensionInstallBlocked(conflict.entry(),
                                   extension_id_and_version)) {
      return false;
    }
  }
  return true;
}

}  // namespace policy
