// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions/extension_key_permissions_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_impl.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"

namespace chromeos {
namespace platform_keys {

namespace {
const char kStateStoreSPKI[] = "SPKI";
const char kStateStoreSignOnce[] = "signOnce";
const char kStateStoreSignUnlimited[] = "signUnlimited";

const char kPolicyAllowCorporateKeyUsage[] = "allowCorporateKeyUsage";

const base::DictionaryValue* GetKeyPermissionsMap(
    policy::PolicyService* const profile_policies) {
  if (!profile_policies)
    return nullptr;

  const policy::PolicyMap& policies = profile_policies->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  const base::Value* policy_value =
      policies.GetValue(policy::key::kKeyPermissions);
  if (!policy_value) {
    DVLOG(1) << "KeyPermissions policy is not set";
    return nullptr;
  }
  const base::DictionaryValue* key_permissions_map = nullptr;
  policy_value->GetAsDictionary(&key_permissions_map);
  return key_permissions_map;
}

bool GetCorporateKeyUsageFromPref(
    const base::DictionaryValue* key_permissions_for_ext) {
  if (!key_permissions_for_ext)
    return false;

  const base::Value* allow_corporate_key_usage =
      key_permissions_for_ext->FindKey(kPolicyAllowCorporateKeyUsage);
  if (!allow_corporate_key_usage || !allow_corporate_key_usage->is_bool())
    return false;
  return allow_corporate_key_usage->GetBool();
}

// Returns true if the extension with id |extension_id| is allowed to use
// corporate usage keys by policy in |profile_policies|.
bool PolicyAllowsCorporateKeyUsageForExtension(
    const std::string& extension_id,
    policy::PolicyService* const profile_policies) {
  if (!profile_policies)
    return false;

  const base::DictionaryValue* key_permissions_map =
      GetKeyPermissionsMap(profile_policies);
  if (!key_permissions_map)
    return false;

  const base::Value* key_permissions_for_ext_value =
      key_permissions_map->FindKey(extension_id);
  const base::DictionaryValue* key_permissions_for_ext = nullptr;
  if (!key_permissions_for_ext_value ||
      !key_permissions_for_ext_value->GetAsDictionary(
          &key_permissions_for_ext) ||
      !key_permissions_for_ext)
    return false;

  bool allow_corporate_key_usage =
      GetCorporateKeyUsageFromPref(key_permissions_for_ext);

  VLOG_IF(allow_corporate_key_usage, 2)
      << "Policy allows usage of corporate keys by extension " << extension_id;
  return allow_corporate_key_usage;
}

bool IsKeyOnUserSlot(const std::vector<TokenId>& key_locations) {
  return base::Contains(key_locations, TokenId::kUser);
}

}  // namespace

ExtensionKeyPermissionsService::ExtensionKeyPermissionsService(
    const std::string& extension_id,
    extensions::StateStore* extensions_state_store,
    std::unique_ptr<base::Value> state_store_value,
    policy::PolicyService* profile_policies,
    PlatformKeysService* platform_keys_service,
    KeyPermissionsService* key_permissions_service)
    : extension_id_(extension_id),
      extensions_state_store_(extensions_state_store),
      profile_policies_(profile_policies),
      platform_keys_service_(platform_keys_service),
      key_permissions_service_(key_permissions_service) {
  DCHECK(extensions_state_store_);
  DCHECK(profile_policies_);
  DCHECK(platform_keys_service_);
  DCHECK(key_permissions_service_);

  if (state_store_value)
    KeyEntriesFromState(*state_store_value);
}

ExtensionKeyPermissionsService::~ExtensionKeyPermissionsService() = default;

ExtensionKeyPermissionsService::KeyEntry*
ExtensionKeyPermissionsService::GetStateStoreEntry(
    const std::string& public_key_spki_der_b64) {
  for (KeyEntry& entry : state_store_entries_) {
    // For every ASN.1 value there is exactly one DER encoding, so it is fine to
    // compare the DER (or its base64 encoding).
    if (entry.spki_b64 == public_key_spki_der_b64)
      return &entry;
  }

  state_store_entries_.push_back(KeyEntry(public_key_spki_der_b64));
  return &state_store_entries_.back();
}

void ExtensionKeyPermissionsService::CanUseKeyForSigning(
    const std::string& public_key_spki_der,
    CanUseKeyForSigningCallback callback) {
  DCHECK(platform_keys_service_);

  platform_keys_service_->GetKeyLocations(
      public_key_spki_der,
      base::BindOnce(
          &ExtensionKeyPermissionsService::CanUseKeyForSigningWithLocations,
          weak_factory_.GetWeakPtr(), public_key_spki_der,
          std::move(callback)));
}

void ExtensionKeyPermissionsService::CanUseKeyForSigningWithLocations(
    const std::string& public_key_spki_der,
    CanUseKeyForSigningCallback callback,
    const std::vector<TokenId>& key_locations,
    Status key_locations_retrieval_status) {
  if (key_locations_retrieval_status != Status::kSuccess) {
    LOG(ERROR) << "PlatformKeysService error on requesting key locations: "
               << StatusToString(key_locations_retrieval_status);
    std::move(callback).Run(/*allowed=*/false);
    return;
  }

  if (key_locations.empty()) {
    std::move(callback).Run(/*allowed=*/false);
    return;
  }

  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  KeyEntry* matching_entry = GetStateStoreEntry(public_key_spki_der_b64);

  // In any case, we allow the generating extension to use the generated key a
  // single time for signing arbitrary data. The reason is, that the extension
  // usually has to sign a certification request containing the public key in
  // order to obtain a certificate for the key.
  // That means, once a certificate authority generated a certificate for the
  // key, the generating extension doesn't have access to the key anymore,
  // except if explicitly permitted by the administrator.
  if (matching_entry->sign_once) {
    std::move(callback).Run(/*allowed=*/true);
    return;
  }

  auto bound_callback = base::BindOnce(
      &ExtensionKeyPermissionsService::CanUseKeyForSigningWithFlags,
      weak_factory_.GetWeakPtr(), std::move(callback),
      matching_entry->sign_unlimited);
  key_permissions_service_->IsCorporateKey(public_key_spki_der,
                                           std::move(bound_callback));
}

void ExtensionKeyPermissionsService::CanUseKeyForSigningWithFlags(
    CanUseKeyForSigningCallback callback,
    bool sign_unlimited_allowed,
    bool is_corporate_key) {
  // Usage of corporate keys is solely determined by policy. The user must not
  // circumvent this decision.
  if (is_corporate_key) {
    std::move(callback).Run(/*allowed=*/PolicyAllowsCorporateKeyUsage());
    return;
  }

  // Only permissions for keys that are not designated for corporate usage are
  // determined by user decisions.
  std::move(callback).Run(sign_unlimited_allowed);
}

void ExtensionKeyPermissionsService::SetKeyUsedForSigning(
    const std::string& public_key_spki_der,
    SetKeyUsedForSigningCallback callback) {
  DCHECK(platform_keys_service_);

  platform_keys_service_->GetKeyLocations(
      public_key_spki_der,
      base::BindOnce(
          &ExtensionKeyPermissionsService::SetKeyUsedForSigningWithLocations,
          weak_factory_.GetWeakPtr(), public_key_spki_der,
          std::move(callback)));
}

void ExtensionKeyPermissionsService::SetKeyUsedForSigningWithLocations(
    const std::string& public_key_spki_der,
    SetKeyUsedForSigningCallback callback,
    const std::vector<TokenId>& key_locations,
    Status key_locations_retrieval_status) {
  if (key_locations_retrieval_status != Status::kSuccess) {
    std::move(callback).Run(key_locations_retrieval_status);
    return;
  }

  if (key_locations.empty()) {
    std::move(callback).Run(Status::kErrorKeyNotFound);
    return;
  }

  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  KeyEntry* matching_entry = GetStateStoreEntry(public_key_spki_der_b64);
  matching_entry->sign_once = false;
  WriteToStateStore();

  std::move(callback).Run(Status::kSuccess);
}

void ExtensionKeyPermissionsService::RegisterKeyForCorporateUsage(
    const std::string& public_key_spki_der,
    RegisterKeyForCorporateUsageCallback callback) {
  DCHECK(platform_keys_service_);

  platform_keys_service_->GetKeyLocations(
      public_key_spki_der,
      base::BindOnce(&ExtensionKeyPermissionsService::
                         RegisterKeyForCorporateUsageWithLocations,
                     weak_factory_.GetWeakPtr(), public_key_spki_der,
                     std::move(callback)));
}

void ExtensionKeyPermissionsService::RegisterKeyForCorporateUsageWithLocations(
    const std::string& public_key_spki_der,
    RegisterKeyForCorporateUsageCallback callback,
    const std::vector<TokenId>& key_locations,
    Status key_locations_retrieval_status) {
  if (key_locations_retrieval_status != Status::kSuccess) {
    std::move(callback).Run(key_locations_retrieval_status);
    return;
  }

  if (key_locations.empty()) {
    std::move(callback).Run(Status::kErrorKeyNotFound);
    return;
  }
  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  KeyEntry* matching_entry = GetStateStoreEntry(public_key_spki_der_b64);

  if (matching_entry->sign_once) {
    VLOG(1) << "Key is already allowed for signing, skipping.";
    std::move(callback).Run(Status::kSuccess);
    return;
  }

  matching_entry->sign_once = true;
  WriteToStateStore();

  // Only register the key as corporate in the profile prefs if it is on the
  // user slot. Keys on the system slot are implicitly corporate. We have still
  // stored the sign_once permission, so the enrolling extension in the same
  // profile can use the key for signing once in order to build a CSR even if it
  // doesn't have permission to use corporate keys.
  if (!IsKeyOnUserSlot(key_locations)) {
    std::move(callback).Run(Status::kSuccess);
    return;
  }

  key_permissions_service_->SetCorporateKey(public_key_spki_der,
                                            std::move(callback));
}

void ExtensionKeyPermissionsService::SetUserGrantedPermission(
    const std::string& public_key_spki_der,
    SetUserGrantedPermissionCallback callback) {
  DCHECK(platform_keys_service_);

  platform_keys_service_->GetKeyLocations(
      public_key_spki_der,
      base::BindOnce(&ExtensionKeyPermissionsService::
                         SetUserGrantedPermissionWithLocations,
                     weak_factory_.GetWeakPtr(), public_key_spki_der,
                     std::move(callback)));
}

void ExtensionKeyPermissionsService::SetUserGrantedPermissionWithLocations(
    const std::string& public_key_spki_der,
    SetUserGrantedPermissionCallback callback,
    const std::vector<TokenId>& key_locations,
    Status key_locations_retrieval_status) {
  key_permissions_service_->CanUserGrantPermissionForKey(
      public_key_spki_der,
      base::BindOnce(&ExtensionKeyPermissionsService::
                         SetUserGrantedPermissionWithLocationsAndFlag,
                     weak_factory_.GetWeakPtr(), public_key_spki_der,
                     std::move(callback), key_locations,
                     key_locations_retrieval_status));
}

void ExtensionKeyPermissionsService::
    SetUserGrantedPermissionWithLocationsAndFlag(
        const std::string& public_key_spki_der,
        SetUserGrantedPermissionCallback callback,
        const std::vector<TokenId>& key_locations,
        Status key_locations_retrieval_status,
        bool can_user_grant_permission) {
  if (key_locations_retrieval_status != Status::kSuccess) {
    std::move(callback).Run(key_locations_retrieval_status);
    return;
  }

  if (!can_user_grant_permission) {
    std::move(callback).Run(Status::kErrorGrantKeyPermissionForExtension);
    return;
  }

  // It only makes sense to store the sign_unlimited flag for a key if it is on
  // a user slot. Currently, system-slot keys are implicitly corporate, so
  // CanUserGrantPermissionForKey should return false for them.
  DCHECK(IsKeyOnUserSlot(key_locations));

  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);
  KeyEntry* matching_entry = GetStateStoreEntry(public_key_spki_der_b64);

  if (matching_entry->sign_unlimited) {
    VLOG(1) << "Key is already allowed for signing, skipping.";
    std::move(callback).Run(Status::kSuccess);
    return;
  }

  matching_entry->sign_unlimited = true;
  WriteToStateStore();
  std::move(callback).Run(Status::kSuccess);
}

bool ExtensionKeyPermissionsService::PolicyAllowsCorporateKeyUsage() const {
  return PolicyAllowsCorporateKeyUsageForExtension(extension_id_,
                                                   profile_policies_);
}

void ExtensionKeyPermissionsService::WriteToStateStore() {
  extensions_state_store_->SetExtensionValue(
      extension_id_, kStateStorePlatformKeys, KeyEntriesToState());
}

void ExtensionKeyPermissionsService::KeyEntriesFromState(
    const base::Value& state) {
  state_store_entries_.clear();

  const base::ListValue* entries = nullptr;
  if (!state.GetAsList(&entries)) {
    LOG(ERROR) << "Found a state store of wrong type.";
    return;
  }
  for (const auto& entry : *entries) {
    std::string spki_b64;
    const base::DictionaryValue* dict_entry = nullptr;
    if (entry.GetAsString(&spki_b64)) {
      // This handles the case that the store contained a plain list of base64
      // and DER-encoded SPKIs from an older version of ChromeOS.
      KeyEntry new_entry(spki_b64);
      new_entry.sign_once = true;
      state_store_entries_.push_back(new_entry);
    } else if (entry.GetAsDictionary(&dict_entry)) {
      dict_entry->GetStringWithoutPathExpansion(kStateStoreSPKI, &spki_b64);
      KeyEntry new_entry(spki_b64);
      dict_entry->GetBooleanWithoutPathExpansion(kStateStoreSignOnce,
                                                 &new_entry.sign_once);
      dict_entry->GetBooleanWithoutPathExpansion(kStateStoreSignUnlimited,
                                                 &new_entry.sign_unlimited);
      state_store_entries_.push_back(new_entry);
    } else {
      LOG(ERROR) << "Found invalid entry of type " << entry.type()
                 << " in PlatformKeys state store.";
      continue;
    }
  }
}

std::unique_ptr<base::Value>
ExtensionKeyPermissionsService::KeyEntriesToState() {
  std::unique_ptr<base::ListValue> new_state(new base::ListValue);
  for (const KeyEntry& entry : state_store_entries_) {
    // Drop entries that the extension doesn't have any permissions for anymore.
    if (!entry.sign_once && !entry.sign_unlimited)
      continue;

    std::unique_ptr<base::DictionaryValue> new_entry(new base::DictionaryValue);
    new_entry->SetKey(kStateStoreSPKI, base::Value(entry.spki_b64));
    // Omit writing default values, namely |false|.
    if (entry.sign_once) {
      new_entry->SetKey(kStateStoreSignOnce, base::Value(entry.sign_once));
    }
    if (entry.sign_unlimited) {
      new_entry->SetKey(kStateStoreSignUnlimited,
                        base::Value(entry.sign_unlimited));
    }
    new_state->Append(std::move(new_entry));
  }
  return std::move(new_state);
}

// static
std::vector<std::string>
ExtensionKeyPermissionsService::GetCorporateKeyUsageAllowedAppIds(
    policy::PolicyService* const profile_policies) {
  std::vector<std::string> permissions;

  const base::DictionaryValue* key_permissions_service_map =
      GetKeyPermissionsMap(profile_policies);
  if (!key_permissions_service_map)
    return permissions;

  for (const auto& item : key_permissions_service_map->DictItems()) {
    const auto& app_id = item.first;
    const auto& key_permission = item.second;
    const base::DictionaryValue* key_permissions_service_for_app = nullptr;
    if (!key_permission.GetAsDictionary(&key_permissions_service_for_app) ||
        !key_permissions_service_for_app) {
      continue;
    }
    if (GetCorporateKeyUsageFromPref(key_permissions_service_for_app))
      permissions.push_back(app_id);
  }
  return permissions;
}

}  // namespace platform_keys
}  // namespace chromeos
