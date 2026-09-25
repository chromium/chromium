// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/low_trust_policy_install_block_manager.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/one_shot_event.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

namespace extensions {

namespace {

// Preference key path string for storing blocked low-trust policy extensions.
constexpr char kBlockedLowTrustPolicyExtensions[] =
    "extensions.blocked_low_trust_policy_installs";

// Preference dictionary keys for blocked extension entries.
constexpr char kOverrideTypeKey[] = "override_type";
constexpr char kUpdateUrlKey[] = "update_url";
constexpr char kTimestampKey[] = "timestamp";

// Maximum duration a low-trust block preference entry remains valid before
// being considered stale and evicted by the TTL mechanism.
constexpr base::TimeDelta kLowTrustBlockTTL = base::Days(7);

// Parses and validates a raw entry from the low-trust blocked preference dict.
// Returns a populated BlockedExtensionInfo if the entry is a dictionary,
// contains all required and valid fields (override_type, update_url,
// timestamp), and is within the TTL window; returns std::nullopt otherwise.
std::optional<BlockedExtensionInfo> ParseValidBlockedEntry(
    const base::Value& value,
    base::Time now) {
  if (!value.is_dict()) {
    return std::nullopt;
  }

  const base::DictValue& dict = value.GetDict();
  auto override_type_opt = dict.FindInt(kOverrideTypeKey);
  const std::string* update_url_opt = dict.FindString(kUpdateUrlKey);
  std::optional<base::Time> timestamp_opt =
      base::ValueToTime(dict.Find(kTimestampKey));

  if (!override_type_opt ||
      !util::IsValidDseNtpOverrideType(*override_type_opt) || !update_url_opt ||
      !timestamp_opt) {
    return std::nullopt;
  }

  if ((now - *timestamp_opt) > kLowTrustBlockTTL) {
    return std::nullopt;
  }

  return BlockedExtensionInfo{
      .override_type =
          static_cast<util::DseNtpOverrideType>(*override_type_opt),
      .update_url = *update_url_opt,
      .timestamp = *timestamp_opt};
}

}  // namespace

// static
void LowTrustPolicyInstallBlockManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kBlockedLowTrustPolicyExtensions);
}

LowTrustPolicyInstallBlockManager::LowTrustPolicyInstallBlockManager(
    content::BrowserContext* context,
    PrefService& pref_service,
    ExtensionManagement& extension_management)
    : context_(context),
      pref_service_(pref_service),
      extension_management_(extension_management) {
  CHECK(context_);
  extension_management_observation_.Observe(&extension_management_.get());

  // ExtensionSystem cannot be retrieved synchronously here because
  // ChromeExtensionSystemSharedFactory depends on ExtensionManagementFactory.
  // Querying ExtensionSystem while ExtensionManagement is still being
  // constructed would re-entrantly trigger dependency resolution and cause
  // a KeyedService cycle assertion. Posting a task defers registration until
  // construction finishes.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LowTrustPolicyInstallBlockManager::
                                    CleanupBlockedCacheAndExtensionsWhenReady,
                                weak_factory_.GetWeakPtr()));
}

LowTrustPolicyInstallBlockManager::~LowTrustPolicyInstallBlockManager() {
  if (observing_policy_service_) {
    policy_service()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  }
}

void LowTrustPolicyInstallBlockManager::
    CleanupBlockedCacheAndExtensionsWhenReady() {
  // Wait for ExtensionSystem::ready() so that all installed extensions are
  // loaded into memory before inspecting and uninstalling policy-blocked
  // extensions. This also triggers blocked cache cleanup (which will defer
  // further if PolicyService has not yet finished initializing).
  ExtensionSystem::Get(context_)->ready().Post(
      FROM_HERE, base::BindOnce(&LowTrustPolicyInstallBlockManager::
                                    OnExtensionManagementSettingsChanged,
                                weak_factory_.GetWeakPtr()));
}

void LowTrustPolicyInstallBlockManager::OnExtensionManagementSettingsChanged() {
  // Uninstall blocked extensions first so that if an extension's policy was
  // removed at the same time as a low-trust transition, any cache entry added
  // during uninstallation is immediately evicted below.
  UninstallBlockedExtensions();
  CleanupRemovedPolicyRecords();
}

void LowTrustPolicyInstallBlockManager::OnPolicyUpdated(
    const policy::PolicyNamespace& ns,
    const policy::PolicyMap& previous,
    const policy::PolicyMap& current) {}

void LowTrustPolicyInstallBlockManager::OnPolicyServiceInitialized(
    policy::PolicyDomain domain) {
  CHECK_EQ(domain, policy::POLICY_DOMAIN_CHROME);
  CHECK(observing_policy_service_);
  policy_service()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
  observing_policy_service_ = false;
  CleanupRemovedPolicyRecords();
}

void LowTrustPolicyInstallBlockManager::CleanupRemovedPolicyRecords() {
  base::flat_map<ExtensionId, BlockedExtensionInfo> blocked_extensions =
      GetAllBlocked();
  if (blocked_extensions.empty()) {
    return;
  }

  // Wait until Chrome browser policies (`ExtensionSettings` and
  // `ExtensionInstallForcelist`) have finished loading before evicting entries
  // missing from `extension_management_`. On the synchronous profile startup
  // path (`CreateMode::kSynchronous`), `ExtensionSystem::ready()` can be
  // signaled before asynchronous policy providers complete initialization;
  // `OnPolicyServiceInitialized()` will re-invoke this method once
  // `POLICY_DOMAIN_CHROME` is ready.
  if (!policy_service()->IsInitializationComplete(
          policy::POLICY_DOMAIN_CHROME)) {
    if (!observing_policy_service_) {
      policy_service()->AddObserver(policy::POLICY_DOMAIN_CHROME, this);
      observing_policy_service_ = true;
    }
    return;
  }

  std::vector<ExtensionId> removed_ids;
  for (const auto& [id, info] : blocked_extensions) {
    // Note: If an ID-based policy's update URL changes,
    // `IsForcedOrRecommendedInstallConfigured` will still return true
    // (matching by ID), so the cache entry is retained with the old update
    // URL. This is safe because metrics will still resolve the mode by ID,
    // and the entry will be cleared if the policy is removed entirely.
    if (!extension_management_->IsForcedOrRecommendedInstallConfigured(
            id, info.update_url)) {
      removed_ids.push_back(id);
    }
  }

  if (removed_ids.empty()) {
    return;
  }

  ScopedDictPrefUpdate update(&pref_service_.get(),
                              kBlockedLowTrustPolicyExtensions);
  for (const auto& id : removed_ids) {
    // Querying ExtensionManagement above may lazily load deferred settings and
    // re-entrantly invoke this method, so verify the entry was still present
    // before logging its removal.
    if (update->Remove(id)) {
      LOG_POLICY(INFO, POLICY_PROCESSING)
          << "[BlockLowTrustExtension] Cleared blocked cache entry for "
             "extension "
          << id << ": Enterprise policy is no longer configured.";
    }
  }
}

void LowTrustPolicyInstallBlockManager::UninstallBlockedExtensions() {
  if (!extension_management_->IsDseNtpOverrideBlockingActive()) {
    return;
  }

  std::vector<scoped_refptr<const Extension>> low_trust_uninstall_list;
  ExtensionSet installed_extensions =
      ExtensionRegistry::Get(context_)->GenerateInstalledExtensionsSet();
  for (const auto& extension : installed_extensions) {
    if (extension_management_
            ->ShouldBlockPolicyInstalledDseNtpOverrideExtension(*extension)) {
      low_trust_uninstall_list.push_back(extension);
    }
  }

  ExtensionRegistrar* registrar = ExtensionRegistrar::Get(context_);
  for (const auto& extension : low_trust_uninstall_list) {
    std::string update_url =
        extension_management_->GetEffectiveUpdateURL(*extension).spec();
    MarkBlocked(extension->id(),
                BlockedExtensionInfo{
                    .override_type = util::GetDseNtpOverrideType(*extension),
                    .update_url = std::move(update_url),
                    .timestamp = base::Time::Now()});
    std::u16string error;
    if (registrar->UninstallExtension(
            extension->id(), UNINSTALL_REASON_INTERNAL_MANAGEMENT, &error)) {
      LOG_POLICY(WARNING, POLICY_PROCESSING)
          << "[BlockLowTrustExtension] Uninstalled policy extension "
          << extension->id() << ": Device lost management trust status.";
    } else {
      LOG_POLICY(ERROR, POLICY_PROCESSING)
          << "[BlockLowTrustExtension] Failed to uninstall policy extension "
          << extension->id() << ": " << base::UTF16ToUTF8(error);
    }
  }
}

void LowTrustPolicyInstallBlockManager::MarkBlocked(
    const ExtensionId& extension_id,
    const BlockedExtensionInfo& info) {
  ScopedDictPrefUpdate update(&pref_service_.get(),
                              kBlockedLowTrustPolicyExtensions);
  base::DictValue entry;
  entry.Set(kOverrideTypeKey, static_cast<int>(info.override_type));
  entry.Set(kUpdateUrlKey, info.update_url);
  entry.Set(kTimestampKey, base::TimeToValue(info.timestamp));
  update->Set(extension_id, std::move(entry));
}

void LowTrustPolicyInstallBlockManager::Clear(const ExtensionId& extension_id) {
  if (!pref_service_->GetDict(kBlockedLowTrustPolicyExtensions)
           .contains(extension_id)) {
    return;
  }

  ScopedDictPrefUpdate update(&pref_service_.get(),
                              kBlockedLowTrustPolicyExtensions);
  update->Remove(extension_id);
}

bool LowTrustPolicyInstallBlockManager::IsBlocked(
    const ExtensionId& extension_id) const {
  const base::DictValue& dict =
      pref_service_->GetDict(kBlockedLowTrustPolicyExtensions);
  const base::Value* entry = dict.Find(extension_id);
  if (!entry) {
    return false;
  }

  return ParseValidBlockedEntry(*entry, base::Time::Now()).has_value();
}

base::flat_map<ExtensionId, BlockedExtensionInfo>
LowTrustPolicyInstallBlockManager::GetAllBlocked() const {
  base::flat_map<ExtensionId, BlockedExtensionInfo> result;
  const base::DictValue& dict =
      pref_service_->GetDict(kBlockedLowTrustPolicyExtensions);
  base::Time now = base::Time::Now();

  for (auto [id, value] : dict) {
    if (auto info = ParseValidBlockedEntry(value, now)) {
      result.emplace(id, std::move(*info));
    }
  }

  return result;
}

size_t LowTrustPolicyInstallBlockManager::CleanupStaleRecords() {
  const base::DictValue& dict =
      pref_service_->GetDict(kBlockedLowTrustPolicyExtensions);
  std::vector<ExtensionId> stale_ids;
  base::Time now = base::Time::Now();

  for (auto [id, value] : dict) {
    if (!ParseValidBlockedEntry(value, now).has_value()) {
      stale_ids.push_back(id);
    }
  }

  if (stale_ids.empty()) {
    return 0;
  }

  ScopedDictPrefUpdate update(&pref_service_.get(),
                              kBlockedLowTrustPolicyExtensions);
  for (const auto& stale_id : stale_ids) {
    update->Remove(stale_id);
  }

  return stale_ids.size();
}

// static
base::TimeDelta LowTrustPolicyInstallBlockManager::GetTTLForTesting() {
  return kLowTrustBlockTTL;
}

void LowTrustPolicyInstallBlockManager::SetPolicyServiceForTesting(
    policy::PolicyService* policy_service) {
  if (observing_policy_service_) {
    this->policy_service()->RemoveObserver(policy::POLICY_DOMAIN_CHROME, this);
    observing_policy_service_ = false;
  }
  policy_service_for_testing_ = policy_service;
}

policy::PolicyService* LowTrustPolicyInstallBlockManager::policy_service()
    const {
  if (policy_service_for_testing_) {
    return policy_service_for_testing_;
  }
  return Profile::FromBrowserContext(context_)
      ->GetProfilePolicyConnector()
      ->policy_service();
}

}  // namespace extensions
