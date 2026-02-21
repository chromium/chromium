// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/extension_install_policy_service.h"

#include <set>
#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
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
#include "content/public/browser/network_service_instance.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension_urls.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
constexpr char kUserCanInstallPolicyFetchTime[] =
    "ExtensionInstall.UserCanInstall.PolicyFetchTime";
constexpr char kUserCanInstallPolicyFetchResult[] =
    "ExtensionInstall.UserCanInstall.PolicyFetchResult";
constexpr char kExtensionMustRemainDisabledResult[] =
    "ExtensionInstall.ExtensionMustRemainDisabled.Result";
constexpr char kExtensionUserMayLoadResult[] =
    "ExtensionInstall.UserMayLoadExtension.Result";
constexpr char kExtensionIsExtensionAllowedResult[] =
    "ExtensionInstall.IsExtensionAllowed.Result";

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IsExtensionAllowedResult {
  kUnspecified = 0,
  kExtensionInstallCloudPolicyChecksDisabled = 1,
  kHasNonDefaultInstallationMode = 2,
  kPolicyNotReady = 3,
  kNoPolicyService = 4,
  kExtensionAllowed = 5,
  kExtensionBlocked = 6,
  kNoPolicyForExtension = 7,
  kNoCloudPolicyManager = 8,
  kMaxValue = kNoCloudPolicyManager,
};

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

}  // namespace

// Helper class to wait for the CloudPolicyClient to be ready and registered.
class ExtensionInstallPolicyServiceImpl::ClientInitializationWaiter
    : public CloudPolicyCore::Observer,
      public CloudPolicyClient::Observer {
 public:
  using ReadyCallback = base::OnceCallback<void(CloudPolicyManager*)>;

  ClientInitializationWaiter(CloudPolicyManager* manager,
                             ReadyCallback callback)
      : manager_(*manager), callback_(std::move(callback)) {
    auto* core = manager_->core();
    if (core->IsConnected()) {
      OnCoreConnected(core);
    } else {
      core_observation_.Observe(core);
    }
  }

  ~ClientInitializationWaiter() override = default;

  // CloudPolicyCore::Observer:
  void OnCoreConnected(CloudPolicyCore* core) override {
    CHECK_EQ(core, manager_->core());
    auto* client = manager_->core()->client();
    CHECK(client);
    client_observation_.Observe(client);
    OnRegistrationStateChanged(client);
  }

  void OnCoreDisconnecting(CloudPolicyCore* core) override {
    CHECK_EQ(core, manager_->core());
    core_observation_.Reset();
    client_observation_.Reset();
  }

  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override {
    CHECK_EQ(core, manager_->core());
    OnRegistrationStateChanged(manager_->core()->client());
  }

  // CloudPolicyClient::Observer:
  void OnRegistrationStateChanged(CloudPolicyClient* client) override {
    CHECK_EQ(client, manager_->core()->client());
    if (!manager_->IsClientRegistered()) {
      return;
    }
    client_observation_.Reset();
    if (callback_) {
      std::move(callback_).Run(&manager_.get());
    }
  }

  CloudPolicyManager* GetManager() const { return &manager_.get(); }

 private:
  raw_ref<CloudPolicyManager> manager_;
  ReadyCallback callback_;

  base::ScopedObservation<CloudPolicyCore, CloudPolicyCore::Observer>
      core_observation_{this};
  base::ScopedObservation<CloudPolicyClient, CloudPolicyClient::Observer>
      client_observation_{this};
};

ExtensionInstallPolicyServiceImpl::ExtensionInstallPolicyServiceImpl(
    Profile* profile)
    : profile_(*profile) {
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
  local_state_change_registrar_.Init(g_browser_process->local_state());
  local_state_change_registrar_.Add(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled,
      base::BindRepeating(
          &ExtensionInstallPolicyServiceImpl::OnPolicyChecksEnabledChanged,
          base::Unretained(this)));

  for (const auto& info : GetPolicyManagerInfos()) {
    // If there is not store available, then that manager does not support
    // extension install policies yet.
    if (!info.manager->IsSupportingExtensionInstallPolicies()) {
      continue;
    }
    initialization_waiters_.emplace(
        &info.manager.get(),
        std::make_unique<ClientInitializationWaiter>(
            &info.manager.get(),
            base::BindOnce(
                &ExtensionInstallPolicyServiceImpl::OnCloudPolicyManagerReady,
                base::Unretained(this))));
  }
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
    base::UmaHistogramEnumeration(
        kUserCanInstallPolicyFetchResult,
        IsExtensionAllowedResult::kExtensionInstallCloudPolicyChecksDisabled);
    std::move(callback).Run(true);
    return;
  }

  if (HasNonDefaultInstallationMode(&profile_.get(),
                                    extension_id_and_version.extension_id)) {
    base::UmaHistogramEnumeration(
        kUserCanInstallPolicyFetchResult,
        IsExtensionAllowedResult::kHasNonDefaultInstallationMode);
    // Installation mode always takes priority over cloud-based blocking. Do
    // not fetch policy.
    std::move(callback).Run(true);
    return;
  }

  // Identify managers with an active extension install core.
  const std::vector<PolicyManagerInfo> active_managers =
      GetConnectedPolicyManagerInfos();

  size_t callback_count = active_managers.size();
  if (callback_count == 0) {
    base::UmaHistogramEnumeration(
        kUserCanInstallPolicyFetchResult,
        IsExtensionAllowedResult::kNoCloudPolicyManager);
    std::move(callback).Run(true);
    return;
  }

  base::TimeTicks fetch_time = base::TimeTicks::Now();
  base::RepeatingCallback<void(ExtensionInstallDecision)> barrier_callback =
      base::BarrierCallback<ExtensionInstallDecision>(
          callback_count,
          base::BindOnce(
              [](base::OnceCallback<void(bool)> inner_callback,
                 base::TimeTicks start_time,
                 const std::vector<ExtensionInstallDecision>& values) {
                base::UmaHistogramTimes(kUserCanInstallPolicyFetchTime,
                                        base::TimeTicks::Now() - start_time);
                bool can_install = true;
                for (const auto& value : values) {
                  if (value.action ==
                      enterprise_management::ExtensionInstallPolicy::
                          ACTION_BLOCK) {
                    can_install = false;
                    break;
                  }
                }
                base::UmaHistogramEnumeration(
                    kUserCanInstallPolicyFetchResult,
                    can_install ? IsExtensionAllowedResult::kExtensionAllowed
                                : IsExtensionAllowedResult::kExtensionBlocked);
                std::move(inner_callback).Run(can_install);
              },
              std::move(callback), std::move(fetch_time)));

  for (const auto& info : active_managers) {
    info.manager->extension_install_core()
        ->service()
        ->FetchExtensionInstallPolicy(
            info.policy_type, extension_id_and_version,
            PolicyFetchReason::kExtensionInstall, barrier_callback);
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
    base::UmaHistogramEnumeration(
        kExtensionIsExtensionAllowedResult,
        IsExtensionAllowedResult::kExtensionInstallCloudPolicyChecksDisabled);
    return true;
  }

  if (HasNonDefaultInstallationMode(&profile_.get(),
                                    extension_id_and_version.extension_id)) {
    base::UmaHistogramEnumeration(
        kExtensionIsExtensionAllowedResult,
        IsExtensionAllowedResult::kHasNonDefaultInstallationMode);
    // Installation mode always takes priority over cloud-based blocking.
    return true;
  }

  auto* policy_service =
      profile_->GetProfilePolicyConnector()->policy_service();
  if (!policy_service) {
    base::UmaHistogramEnumeration(kExtensionIsExtensionAllowedResult,
                                  IsExtensionAllowedResult::kNoPolicyService);
    return std::nullopt;
  }

  if (!policy_service->IsInitializationComplete(
          policy::POLICY_DOMAIN_EXTENSION_INSTALL)) {
    base::UmaHistogramEnumeration(kExtensionIsExtensionAllowedResult,
                                  IsExtensionAllowedResult::kPolicyNotReady);
    return std::nullopt;
  }

  const PolicyMap& extension_install_policy_map =
      policy_service->GetPolicies(policy::PolicyNamespace(
          policy::POLICY_DOMAIN_EXTENSION_INSTALL, std::string()));

  const PolicyMap::Entry* entry =
      extension_install_policy_map.Get(extension_id_and_version.extension_id);
  if (!entry) {
    base::UmaHistogramEnumeration(
        kExtensionIsExtensionAllowedResult,
        IsExtensionAllowedResult::kNoPolicyForExtension);
    return true;
  }

  if (IsExtensionInstallBlocked(*entry, extension_id_and_version)) {
    base::UmaHistogramEnumeration(kExtensionIsExtensionAllowedResult,
                                  IsExtensionAllowedResult::kExtensionBlocked);
    return false;
  }

  for (const auto& conflict : entry->conflicts) {
    if (IsExtensionInstallBlocked(conflict.entry(), extension_id_and_version)) {
      base::UmaHistogramEnumeration(
          kExtensionIsExtensionAllowedResult,
          IsExtensionAllowedResult::kExtensionBlocked);
      return false;
    }
  }
  base::UmaHistogramEnumeration(kExtensionIsExtensionAllowedResult,
                                IsExtensionAllowedResult::kExtensionAllowed);
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

void ExtensionInstallPolicyServiceImpl::OnCloudPolicyManagerReady(
    CloudPolicyManager* manager) {
  CloudPolicyClient* client = manager->core()->client();
  // TODO(crbug.com/485872483) : Add a check on the extension install core once
  // tests are properly setup.
  if (!manager || !manager->IsClientRegistered() ||
      manager->extension_install_core()) {
    return;
  }
  // Check that the client is fully initialized.
  // In tests the client might not be fully initialized, in that case, bail out.
  // TODO(crbug.com/485872483) : Add a check on the dm_topken and client_id once
  // tests are properly setup.
  if (!client || !client->is_registered() || client->dm_token().empty() ||
      client->client_id().empty()) {
    return;
  }

  auto new_client = std::make_unique<CloudPolicyClient>(
      client->service(), client->GetURLLoaderFactory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());
  new_client->SetupRegistration(client->dm_token(), client->client_id(),
                                client->user_affiliation_ids());

  manager->InitExtensionInstallPolicies(
      g_browser_process->local_state(), std::move(new_client),
      base::BindRepeating(&content::GetNetworkConnectionTracker));

  CHECK(manager->extension_install_core());
  CHECK(manager->extension_install_core()->IsConnected());
  CHECK(manager->extension_install_core()->client()->is_registered());
  OnPolicyChecksEnabledChanged();

  initialization_waiters_.erase(manager);
}

void ExtensionInstallPolicyServiceImpl::Shutdown() {
  initialization_waiters_.clear();
  pref_change_registrar_.Reset();
  if (auto* policy_service =
          profile_->GetProfilePolicyConnector()->policy_service()) {
    policy_service->RemoveObserver(POLICY_DOMAIN_EXTENSION_INSTALL, this);
  }

  for (const auto& info : GetConnectedPolicyManagerInfos()) {
    info.manager->extension_install_core()->client()->RemovePolicyTypeToFetch(
        {info.policy_type, this});
  }
}
void ExtensionInstallPolicyServiceImpl::NotifyExtensionInstallPolicyUpdated() {
  for (auto& observer : observers_) {
    observer.OnExtensionInstallPolicyUpdated();
  }
}

std::vector<ExtensionInstallPolicyServiceImpl::PolicyManagerInfo>
ExtensionInstallPolicyServiceImpl::GetPolicyManagerInfos() const {
  std::vector<PolicyManagerInfo> managers;
  if (auto* user_cloud_policy_manager = profile_->GetCloudPolicyManager()) {
    managers.push_back(
        {raw_ref<CloudPolicyManager>::from_ptr(user_cloud_policy_manager),
         dm_protocol::kChromeExtensionInstallUserCloudPolicyType});
  }
#if !BUILDFLAG(IS_CHROMEOS)
  if (auto* machine_level_policy_manager =
          g_browser_process->browser_policy_connector()
              ->machine_level_user_cloud_policy_manager()) {
    managers.push_back(
        {raw_ref<CloudPolicyManager>::from_ptr(machine_level_policy_manager),
         dm_protocol::kChromeExtensionInstallMachineLevelCloudPolicyType});
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)
  return managers;
}

std::vector<ExtensionInstallPolicyServiceImpl::PolicyManagerInfo>
ExtensionInstallPolicyServiceImpl::GetConnectedPolicyManagerInfos() const {
  std::vector<PolicyManagerInfo> managers;
  for (const auto& info : GetPolicyManagerInfos()) {
    if (info.manager->extension_install_core() &&
        info.manager->extension_install_core()->IsConnected()) {
      managers.push_back(info);
    }
  }
  return managers;
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
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  return false;
#else
  // TODO(crbug.com/477545526): Refresh policies when new extensions are
  // installed.
  bool user_may_load =
      IsExtensionAllowed({extension->id(), extension->VersionString()})
          .value_or(true);
  base::UmaHistogramBoolean(kExtensionUserMayLoadResult, user_may_load);
  return user_may_load;
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)
}

bool ExtensionInstallPolicyServiceImpl::MustRemainDisabled(
    const extensions::Extension* extension,
    extensions::disable_reason::DisableReason* reason) const {
#if !BUILDFLAG(ENABLE_EXTENSIONS)
  return false;
#else
  // TODO(crbug.com/477545526): Refresh policies when new extensions are
  // installed.
  bool must_remain_disabled =
      !IsExtensionAllowed({extension->id(), extension->VersionString()})
           .value_or(true);
  base::UmaHistogramBoolean(kExtensionMustRemainDisabledResult,
                            must_remain_disabled);
  return must_remain_disabled;
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS)
}

std::set<ExtensionIdAndVersion>
ExtensionInstallPolicyServiceImpl::GetExtensions() {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(&profile_.get());
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

void ExtensionInstallPolicyServiceImpl::OnPolicyChecksEnabledChanged() {
  // TODO(b/449178423): RemovePolicyTypeToFetch() in OnCoreDisconnecting()?

  bool user_enabled = profile_->GetPrefs()->GetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled);
  bool machine_enabled = g_browser_process->local_state()->GetBoolean(
      extensions::pref_names::kExtensionInstallCloudPolicyChecksEnabled);

  for (const auto& info : GetConnectedPolicyManagerInfos()) {
    if (auto* core = info.manager->extension_install_core()) {
      if (!core->client()) {
        continue;
      }
      bool is_user_policy =
          info.policy_type ==
          dm_protocol::kChromeExtensionInstallUserCloudPolicyType;
      if (is_user_policy ? user_enabled : machine_enabled) {
        bool already_has_policy_type = core->client()->HasPolicyTypeToFetch(
            info.policy_type, std::string());
        core->client()->AddPolicyTypeToFetch({info.policy_type, this});
        // In tests the policy client might not always have a real device
        // management service.
        if (!already_has_policy_type && core->client()->service()) {
          core->client()->FetchPolicy(
              PolicyFetchReason::kExtensionInstallInitialization);
        }
      } else {
        core->client()->RemovePolicyTypeToFetch({info.policy_type, this});
      }
    }
  }
}

}  // namespace policy
