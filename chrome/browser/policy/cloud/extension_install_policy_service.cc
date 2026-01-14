// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/extension_install_policy_service.h"

#include <set>
#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_client_types.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"

namespace policy {

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
    return false;
  }

  enterprise_management::ExtensionInstallPolicy::Action action =
      static_cast<enterprise_management::ExtensionInstallPolicy::Action>(
          value_for_version->FindInt("action").value_or(
              enterprise_management::ExtensionInstallPolicy::ACTION_ALLOW));
  return action == enterprise_management::ExtensionInstallPolicy::ACTION_BLOCK;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

MachineLevelUserCloudPolicyManager* GetMachineCloudPolicyManager() {
#if !BUILDFLAG(IS_CHROMEOS)
  return g_browser_process->browser_policy_connector()
      ->machine_level_user_cloud_policy_manager();
#else
  return nullptr;
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

}  // namespace

ExtensionInstallPolicyServiceImpl::ExtensionInstallPolicyServiceImpl(
    Profile* profile)
    : profile_(profile) {
  CHECK(base::FeatureList::IsEnabled(
      features::kEnableExtensionInstallPolicyFetching));
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled,
      base::BindRepeating(
          &ExtensionInstallPolicyServiceImpl::OnPolicyChecksEnabledChanged,
          base::Unretained(this)));
  OnPolicyChecksEnabledChanged();
}

ExtensionInstallPolicyServiceImpl::~ExtensionInstallPolicyServiceImpl() {
  CloudPolicyManager* user_level = profile_->GetCloudPolicyManager();
  if (user_level && user_level->core()->client()) {
    user_level->core()->client()->RemovePolicyTypeToFetch(
        {dm_protocol::kChromeExtensionInstallUserCloudPolicyType, this});
  }
  MachineLevelUserCloudPolicyManager* machine_level =
      GetMachineCloudPolicyManager();
  if (machine_level && machine_level->core()->client()) {
    machine_level->core()->client()->RemovePolicyTypeToFetch(
        {dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType,
         this});
  }
}

void ExtensionInstallPolicyServiceImpl::CanInstallExtension(
    const ExtensionIdAndVersion& extension_id_and_version,
    base::OnceCallback<void(bool)> callback) {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  std::move(callback).Run(true);
  return;
#else
  if (!profile_->GetPrefs()->GetBoolean(
          extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled)) {
    std::move(callback).Run(true);
    return;
  }

  CloudPolicyManager* user_cloud_policy_manager =
      profile_->GetCloudPolicyManager();
  MachineLevelUserCloudPolicyManager* machine_cloud_policy_manager =
      GetMachineCloudPolicyManager();

  size_t callback_count = 0;
  if (user_cloud_policy_manager &&
      user_cloud_policy_manager->core()->extension_install_service()) {
    ++callback_count;
  }
  if (machine_cloud_policy_manager &&
      machine_cloud_policy_manager->core()->extension_install_service()) {
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

  if (user_cloud_policy_manager &&
      user_cloud_policy_manager->core()->extension_install_service()) {
    user_cloud_policy_manager->core()
        ->extension_install_service()
        ->FetchExtensionInstallPolicy(
            dm_protocol::kChromeExtensionInstallUserCloudPolicyType,
            extension_id_and_version, PolicyFetchReason::kExtensionInstall,
            barrier_callback);
  }
  if (machine_cloud_policy_manager &&
      machine_cloud_policy_manager->core()->extension_install_service()) {
    machine_cloud_policy_manager->core()
        ->extension_install_service()
        ->FetchExtensionInstallPolicy(
            dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType,
            extension_id_and_version, PolicyFetchReason::kExtensionInstall,
            barrier_callback);
  }
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)
}

std::optional<bool> ExtensionInstallPolicyServiceImpl::IsExtensionAllowed(
    const ExtensionIdAndVersion& extension_id_and_version) {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  return std::nullopt;
#else
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
  if (!entry) {
    return true;
  }

  if (IsExtensionInstallBlocked(*entry, extension_id_and_version)) {
    return false;
  }

  for (const auto& conflict : entry->conflicts) {
    if (IsExtensionInstallBlocked(conflict.entry(), extension_id_and_version)) {
      return false;
    }
  }
  return true;
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)
}

std::set<ExtensionIdAndVersion>
ExtensionInstallPolicyServiceImpl::GetExtensions() {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile_);
  if (!extension_registry) {
    return {};
  }
  std::set<ExtensionIdAndVersion> extensions;
  // Include all installed extensions, even if they're already disabled.
  extensions::ExtensionSet installed_extensions =
      extension_registry->GenerateInstalledExtensionsSet();
  for (const auto& extension : installed_extensions) {
    extensions.insert(ExtensionIdAndVersion{
        .extension_id = extension->id(),
        .extension_version = extension->version().GetString(),
    });
  }
  return extensions;
}

void ExtensionInstallPolicyServiceImpl::OnPolicyChecksEnabledChanged() {
  // TODO(b/449178423): Listen for OnCoreConnected(), in case the client
  // Connect()s *after* this runs.

  // TODO(b/449178423): RemovePolicyTypeToFetch() in OnCoreDisconnecting()?

  bool enabled = profile_->GetPrefs()->GetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled);
  CloudPolicyManager* user_level = profile_->GetCloudPolicyManager();
  MachineLevelUserCloudPolicyManager* machine_level =
      GetMachineCloudPolicyManager();
  if (enabled) {
    // Add to CloudPolicyClient::types_to_fetch_ in both clients.
    if (user_level && user_level->core()->client()) {
      user_level->core()->client()->AddPolicyTypeToFetch(
          {dm_protocol::kChromeExtensionInstallUserCloudPolicyType, this});
    }
    if (machine_level && machine_level->core()->client()) {
      machine_level->core()->client()->AddPolicyTypeToFetch(
          {dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType,
           this});
    }
  } else {
    // Remove from CloudPolicyClient::types_to_fetch_ in both clients.
    if (user_level && user_level->core()->client()) {
      user_level->core()->client()->RemovePolicyTypeToFetch(
          {dm_protocol::kChromeExtensionInstallUserCloudPolicyType, this});
    }
    if (machine_level && machine_level->core()->client()) {
      machine_level->core()->client()->RemovePolicyTypeToFetch(
          {dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType,
           this});
    }
  }
}

}  // namespace policy
