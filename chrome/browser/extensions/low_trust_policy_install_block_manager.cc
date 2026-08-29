// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/low_trust_policy_install_block_manager.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/json/values_util.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

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
    PrefService& pref_service)
    : pref_service_(pref_service) {}

LowTrustPolicyInstallBlockManager::~LowTrustPolicyInstallBlockManager() =
    default;

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

}  // namespace extensions
