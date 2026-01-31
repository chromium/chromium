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
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/managed_installation_mode.h"
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
#include "extensions/common/extension_urls.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "ui/base/l10n/l10n_util.h"

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

bool HasNonDefaultInstallationMode(Profile* profile,
                                   const std::string& extension_id) {
  auto* extension_management =
      extensions::ExtensionManagementFactory::GetForBrowserContext(profile);
  CHECK(extension_management);
  extensions::ManagedInstallationMode installation_mode =
      extension_management->GetInstallationMode(
          extension_id, extension_urls::GetWebstoreUpdateUrl().spec());
  return installation_mode != extensions::ManagedInstallationMode::kAllowed;
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

MachineLevelUserCloudPolicyManager* GetMachineCloudPolicyManagerIfConnected() {
#if !BUILDFLAG(IS_CHROMEOS)
  MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  if (manager && manager->core()->client() &&
      manager->core()->extension_install_service()) {
    return manager;
  }
  return nullptr;
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
  if (auto* policy_service =
          profile_->GetProfilePolicyConnector()->policy_service()) {
    policy_service->AddObserver(POLICY_DOMAIN_EXTENSION_INSTALL, this);
  }
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled,
      base::BindRepeating(
          &ExtensionInstallPolicyServiceImpl::OnPolicyChecksEnabledChanged,
          base::Unretained(this)));
  OnPolicyChecksEnabledChanged();
}

ExtensionInstallPolicyServiceImpl::~ExtensionInstallPolicyServiceImpl() =
    default;

void ExtensionInstallPolicyServiceImpl::CanInstallExtension(
    const ExtensionIdAndVersion& extension_id_and_version,
    base::OnceCallback<void(bool)> callback) const {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  std::move(callback).Run(true);
  return;
#else
  if (!profile_->GetPrefs()->GetBoolean(
          extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled)) {
    std::move(callback).Run(true);
    return;
  }

  if (HasNonDefaultInstallationMode(profile_,
                                    extension_id_and_version.extension_id)) {
    // Installation mode always takes priority over cloud-based blocking. Do
    // not fetch policy.
    std::move(callback).Run(true);
    return;
  }

  CloudPolicyManager* user_cloud_policy_manager =
      GetUserCloudPolicyManagerIfConnected();
  MachineLevelUserCloudPolicyManager* machine_cloud_policy_manager =
      GetMachineCloudPolicyManagerIfConnected();

  size_t callback_count = (user_cloud_policy_manager ? 1 : 0) +
                          (machine_cloud_policy_manager ? 1 : 0);
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
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)
}

std::optional<bool> ExtensionInstallPolicyServiceImpl::IsExtensionAllowed(
    const ExtensionIdAndVersion& extension_id_and_version) const {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  return std::nullopt;
#else
  if (!profile_->GetPrefs()->GetBoolean(
          extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled)) {
    return true;
  }

  if (HasNonDefaultInstallationMode(profile_,
                                    extension_id_and_version.extension_id)) {
    // Installation mode always takes priority over cloud-based blocking.
    return true;
  }

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

void ExtensionInstallPolicyServiceImpl::AddObserver(
    ExtensionInstallPolicyService::Observer* observer) {
  observers_.AddObserver(observer);
}

void ExtensionInstallPolicyServiceImpl::RemoveObserver(
    ExtensionInstallPolicyService::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ExtensionInstallPolicyServiceImpl::OnPolicyUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  NotifyExtensionInstallPolicyUpdated();
}

void ExtensionInstallPolicyServiceImpl::Shutdown() {
  if (auto* policy_service =
          profile_->GetProfilePolicyConnector()->policy_service()) {
    policy_service->RemoveObserver(POLICY_DOMAIN_EXTENSION_INSTALL, this);
  }
  if (auto* user_cloud_policy_manager =
          GetUserCloudPolicyManagerIfConnected()) {
    user_cloud_policy_manager->core()->client()->RemovePolicyTypeToFetch(
        {dm_protocol::kChromeExtensionInstallUserCloudPolicyType, this});
  }
  if (auto* machine_cloud_policy_manager =
          GetMachineCloudPolicyManagerIfConnected()) {
    machine_cloud_policy_manager->core()->client()->RemovePolicyTypeToFetch(
        {dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType,
         this});
  }
}

void ExtensionInstallPolicyServiceImpl::NotifyExtensionInstallPolicyUpdated() {
  for (auto& observer : observers_) {
    observer.OnExtensionInstallPolicyUpdated();
  }
}

std::string ExtensionInstallPolicyServiceImpl::GetDebugPolicyProviderName()
    const {
#if DCHECK_IS_ON()
  return "ExtensionInstallPolicyServiceImpl";
#else
  base::ImmediateCrash();
#endif  // DCHECK_IS_ON()
}

void ExtensionInstallPolicyServiceImpl::UserMayInstall(
    scoped_refptr<const extensions::Extension> extension,
    base::OnceCallback<void(extensions::ManagementPolicy::Decision)> callback)
    const {
  if (!extension->from_webstore()) {
    // Always allow non-webstore extensions.
    std::move(callback).Run({true, std::u16string()});
    return;
  }
  CanInstallExtension(
      {extension->id(), extension->VersionString()},
      base::BindOnce(
          [](base::OnceCallback<void(extensions::ManagementPolicy::Decision)>
                 callback,
             bool can_install) {
            std::move(callback).Run(
                {can_install,
                 can_install
                     ? std::u16string()
                     : l10n_util::GetStringUTF16(
                           IDS_EXTENSION_CANT_INSTALL_BLOCKED_BY_RISK_SCORE)});
          },
          std::move(callback)));
}

bool ExtensionInstallPolicyServiceImpl::UserMayLoad(
    const extensions::Extension* extension,
    std::u16string* error) const {
  // TODO(crbug.com/477545526): Implement this based on cached policy values.
  // TODO(crbug.com/477545526): Notify ExtensionService of policy changes so it
  // re-runs CheckManagementPolicy() to apply the new cached value.
  return true;
}

bool ExtensionInstallPolicyServiceImpl::MustRemainDisabled(
    const extensions::Extension* extension,
    extensions::disable_reason::DisableReason* reason) const {
  // TODO(crbug.com/477545526): Implement this based on cached policy values.
  // TODO(crbug.com/477545526): Notify ExtensionService of policy changes so it
  // re-runs CheckManagementPolicy() to apply the new cached value.
  return false;
}

std::set<ExtensionIdAndVersion>
ExtensionInstallPolicyServiceImpl::GetExtensions() {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile_);
  if (!extension_registry) {
    return {};
  }
  std::set<ExtensionIdAndVersion> extensions;
  std::string webstore_update_url =
      extension_urls::GetWebstoreUpdateUrl().spec();
  // Include all installed extensions, even if they're already disabled.
  extensions::ExtensionSet installed_extensions =
      extension_registry->GenerateInstalledExtensionsSet();
  for (const auto& extension : installed_extensions) {
    if (!extension->from_webstore()) {
      // Only check webstore extensions.
      continue;
    }
    extensions.insert({extension->id(), extension->VersionString()});
  }
  return extensions;
}

CloudPolicyManager*
ExtensionInstallPolicyServiceImpl::GetUserCloudPolicyManagerIfConnected()
    const {
  CloudPolicyManager* manager = profile_->GetCloudPolicyManager();
  if (manager && manager->core()->client() &&
      manager->core()->extension_install_service()) {
    return manager;
  }
  return nullptr;
}

void ExtensionInstallPolicyServiceImpl::OnPolicyChecksEnabledChanged() {
  // TODO(b/449178423): Listen for OnCoreConnected(), in case the client
  // Connect()s *after* this runs.

  // TODO(b/449178423): RemovePolicyTypeToFetch() in OnCoreDisconnecting()?

  bool enabled = profile_->GetPrefs()->GetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled);
  CloudPolicyManager* user_cloud_policy_manager =
      GetUserCloudPolicyManagerIfConnected();
  MachineLevelUserCloudPolicyManager* machine_cloud_policy_manager =
      GetMachineCloudPolicyManagerIfConnected();
  if (enabled) {
    // Add to CloudPolicyClient::types_to_fetch_ in both clients.
    if (user_cloud_policy_manager) {
      user_cloud_policy_manager->core()->client()->AddPolicyTypeToFetch(
          {dm_protocol::kChromeExtensionInstallUserCloudPolicyType, this});
    }
    if (machine_cloud_policy_manager) {
      machine_cloud_policy_manager->core()->client()->AddPolicyTypeToFetch(
          {dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType,
           this});
    }
  } else {
    // Remove from CloudPolicyClient::types_to_fetch_ in both clients.
    if (user_cloud_policy_manager) {
      user_cloud_policy_manager->core()->client()->RemovePolicyTypeToFetch(
          {dm_protocol::kChromeExtensionInstallUserCloudPolicyType, this});
    }
    if (machine_cloud_policy_manager) {
      machine_cloud_policy_manager->core()->client()->RemovePolicyTypeToFetch(
          {dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType,
           this});
    }
  }
}

}  // namespace policy
