// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/state_store.h"

namespace chromeos {

namespace {

// The key at which platform key specific data is stored in each extension's
// state store.
//
// From older versions of ChromeOS, this key can hold a list of base64 and
// DER-encoded SPKIs. A key can be used for signing at most once if it is part
// of that list.
//
// The current format of data that is written to the PlatformKeys field is a
// list of serialized KeyEntry objects:
//   { 'SPKI': string,
//     'signOnce': bool,  // if not present, defaults to false
//     'signUnlimited': bool  // if not present, defaults to false
//   }
//
// Do not change this constant as clients will lose their existing state.
const char kStateStorePlatformKeys[] = "PlatformKeys";
const char kStateStoreSPKI[] = "SPKI";
const char kStateStoreSignOnce[] = "signOnce";
const char kStateStoreSignUnlimited[] = "signUnlimited";

// The profile pref prefs::kPlatformKeys stores a dictionary mapping from
// public key (base64 encoding of an DER-encoded SPKI) to key properties. The
// currently only key property is the key usage, which can either be undefined
// or "corporate". If a key is not present in the pref, the default for the key
// usage is undefined, which in particular means "not for corporate usage".
// E.g. the entry in the profile pref might look like:
// "platform_keys" : {
//   "ABCDEF123" : {
//     "keyUsage" : "corporate"
//   },
//   "abcdef567" : {
//     "keyUsage" : "corporate"
//   }
// }
const char kPrefKeyUsage[] = "keyUsage";
const char kPrefKeyUsageCorporate[] = "corporate";

const char kPolicyAllowCorporateKeyUsage[] = "allowCorporateKeyUsage";

const base::DictionaryValue* GetPrefsEntry(
    const std::string& public_key_spki_der_b64,
    const PrefService* const profile_prefs) {
  if (!profile_prefs)
    return nullptr;

  const base::DictionaryValue* platform_keys =
      profile_prefs->GetDictionary(prefs::kPlatformKeys);
  if (!platform_keys)
    return nullptr;

  const base::Value* key_entry_value =
      platform_keys->FindKey(public_key_spki_der_b64);
  if (!key_entry_value)
    return nullptr;

  const base::DictionaryValue* key_entry = nullptr;
  key_entry_value->GetAsDictionary(&key_entry);
  return key_entry;
}

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

bool IsKeyOnUserSlot(
    const std::vector<KeyPermissions::KeyLocation>& key_locations) {
  return base::Contains(key_locations, KeyPermissions::KeyLocation::kUserSlot);
}

}  // namespace

struct KeyPermissions::PermissionsForExtension::KeyEntry {
  explicit KeyEntry(const std::string& public_key_spki_der_b64)
      : spki_b64(public_key_spki_der_b64) {}

  // The base64-encoded DER of a X.509 Subject Public Key Info.
  std::string spki_b64;

  // True if the key can be used once for singing.
  // This permission is granted if an extension generated a key using the
  // enterprise.platformKeys API, so that it can build a certification request.
  // After the first signing operation this permission will be revoked.
  bool sign_once = false;

  // True if the key can be used for signing an unlimited number of times.
  // This permission is granted by the user to allow the extension to use the
  // key for signing through the enterprise.platformKeys or platformKeys API.
  // This permission is granted until revoked by the user or the policy.
  bool sign_unlimited = false;
};

KeyPermissions::PermissionsForExtension::PermissionsForExtension(
    const std::string& extension_id,
    std::unique_ptr<base::Value> state_store_value,
    PrefService* profile_prefs,
    policy::PolicyService* profile_policies,
    KeyPermissions* key_permissions)
    : extension_id_(extension_id),
      profile_prefs_(profile_prefs),
      profile_policies_(profile_policies),
      key_permissions_(key_permissions) {
  DCHECK(profile_prefs_);
  DCHECK(profile_policies_);
  DCHECK(key_permissions_);
  if (state_store_value)
    KeyEntriesFromState(*state_store_value);
}

KeyPermissions::PermissionsForExtension::~PermissionsForExtension() {
}

bool KeyPermissions::PermissionsForExtension::CanUseKeyForSigning(
    const std::string& public_key_spki_der,
    const std::vector<KeyLocation>& key_locations) {
  if (key_locations.empty())
    return false;

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
  if (matching_entry->sign_once)
    return true;

  // Usage of corporate keys is solely determined by policy. The user must not
  // circumvent this decision.
  if (key_permissions_->IsCorporateKey(public_key_spki_der_b64,
                                       key_locations)) {
    return PolicyAllowsCorporateKeyUsage();
  }

  // Only permissions for keys that are not designated for corporate usage are
  // determined by user decisions.
  return matching_entry->sign_unlimited;
}

void KeyPermissions::PermissionsForExtension::SetKeyUsedForSigning(
    const std::string& public_key_spki_der,
    const std::vector<KeyLocation>& key_locations) {
  if (key_locations.empty())
    return;

  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  KeyEntry* matching_entry = GetStateStoreEntry(public_key_spki_der_b64);

  if (!matching_entry->sign_once) {
    if (!CanUseKeyForSigning(public_key_spki_der, key_locations))
      LOG(ERROR) << "Key was not allowed for signing.";
    return;
  }

  matching_entry->sign_once = false;
  WriteToStateStore();
}

void KeyPermissions::PermissionsForExtension::RegisterKeyForCorporateUsage(
    const std::string& public_key_spki_der,
    const std::vector<KeyLocation>& key_locations) {
  if (key_locations.empty()) {
    NOTREACHED();
    return;
  }

  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  KeyEntry* matching_entry = GetStateStoreEntry(public_key_spki_der_b64);

  if (matching_entry->sign_once) {
    VLOG(1) << "Key is already allowed for signing, skipping.";
    return;
  }

  matching_entry->sign_once = true;
  WriteToStateStore();

  // Only register the key as corporate in the profile prefs if it is on the
  // user slot. Keys on the system slot are implicitly corporate. We have still
  // stored the sign_once permission, so the enrolling extension in the same
  // profile can use the key for signing once in order to build a CSR even if it
  // doesn't have permission to use corporate keys.
  if (!IsKeyOnUserSlot(key_locations))
    return;

  DictionaryPrefUpdate update(profile_prefs_, prefs::kPlatformKeys);

  std::unique_ptr<base::DictionaryValue> new_pref_entry(
      new base::DictionaryValue);
  new_pref_entry->SetKey(kPrefKeyUsage, base::Value(kPrefKeyUsageCorporate));

  update->SetWithoutPathExpansion(public_key_spki_der_b64,
                                  std::move(new_pref_entry));
}

void KeyPermissions::PermissionsForExtension::SetUserGrantedPermission(
    const std::string& public_key_spki_der,
    const std::vector<KeyLocation>& key_locations) {
  if (!key_permissions_->CanUserGrantPermissionFor(public_key_spki_der,
                                                   key_locations)) {
    LOG(WARNING) << "Tried to grant permission for a key although prohibited "
                    "(either key is a corporate key or this account is "
                    "managed).";
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
    return;
  }

  matching_entry->sign_unlimited = true;
  WriteToStateStore();
}

bool KeyPermissions::PermissionsForExtension::PolicyAllowsCorporateKeyUsage()
    const {
  return PolicyAllowsCorporateKeyUsageForExtension(extension_id_,
                                                   profile_policies_);
}

void KeyPermissions::PermissionsForExtension::WriteToStateStore() {
  key_permissions_->SetPlatformKeysOfExtension(extension_id_,
                                               KeyEntriesToState());
}

void KeyPermissions::PermissionsForExtension::KeyEntriesFromState(
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
KeyPermissions::PermissionsForExtension::KeyEntriesToState() {
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

KeyPermissions::PermissionsForExtension::KeyEntry*
KeyPermissions::PermissionsForExtension::GetStateStoreEntry(
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

KeyPermissions::KeyPermissions(bool profile_is_managed,
                               PrefService* profile_prefs,
                               policy::PolicyService* profile_policies,
                               extensions::StateStore* extensions_state_store)
    : profile_is_managed_(profile_is_managed),
      profile_prefs_(profile_prefs),
      profile_policies_(profile_policies),
      extensions_state_store_(extensions_state_store) {
  DCHECK(profile_prefs_);
  DCHECK(extensions_state_store_);
  DCHECK(!profile_is_managed_ || profile_policies_);
}

KeyPermissions::~KeyPermissions() {
}

void KeyPermissions::GetPermissionsForExtension(
    const std::string& extension_id,
    const PermissionsCallback& callback) {
  extensions_state_store_->GetExtensionValue(
      extension_id, kStateStorePlatformKeys,
      base::Bind(&KeyPermissions::CreatePermissionObjectAndPassToCallback,
                 weak_factory_.GetWeakPtr(), extension_id, callback));
}

bool KeyPermissions::CanUserGrantPermissionFor(
    const std::string& public_key_spki_der,
    const std::vector<KeyLocation>& key_locations) const {
  if (key_locations.empty())
    return false;

  // As keys cannot be tagged for non-corporate usage, the user can currently
  // not grant any permissions if the profile is managed.
  if (profile_is_managed_)
    return false;

  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  // If this profile is not managed but we find a corporate key, don't allow
  // the user to grant permissions.
  return !IsCorporateKey(public_key_spki_der_b64, key_locations);
}

// static
bool KeyPermissions::IsCorporateKeyForProfile(
    const std::string& public_key_spki_der_b64,
    const PrefService* const profile_prefs) {
  const base::DictionaryValue* prefs_entry =
      GetPrefsEntry(public_key_spki_der_b64, profile_prefs);
  if (prefs_entry) {
    const base::Value* key_usage = prefs_entry->FindKey(kPrefKeyUsage);
    if (!key_usage || !key_usage->is_string())
      return false;
    return key_usage->GetString() == kPrefKeyUsageCorporate;
  }
  return false;
}

// static
std::vector<std::string> KeyPermissions::GetCorporateKeyUsageAllowedAppIds(
    policy::PolicyService* const profile_policies) {
  std::vector<std::string> permissions;

  const base::DictionaryValue* key_permissions_map =
      GetKeyPermissionsMap(profile_policies);
  if (!key_permissions_map)
    return permissions;

  for (const auto& item : key_permissions_map->DictItems()) {
    const auto& app_id = item.first;
    const auto& key_permission = item.second;
    const base::DictionaryValue* key_permissions_for_app = nullptr;
    if (!key_permission.GetAsDictionary(&key_permissions_for_app) ||
        !key_permissions_for_app) {
      continue;
    }
    if (GetCorporateKeyUsageFromPref(key_permissions_for_app))
      permissions.push_back(app_id);
  }
  return permissions;
}

bool KeyPermissions::IsCorporateKey(
    const std::string& public_key_spki_der_b64,
    const std::vector<KeyPermissions::KeyLocation>& key_locations) const {
  for (const KeyLocation key_location : key_locations) {
    switch (key_location) {
      case KeyLocation::kUserSlot:
        if (IsCorporateKeyForProfile(public_key_spki_der_b64, profile_prefs_))
          return true;
        break;
      case KeyLocation::kSystemSlot:
        return true;
      default:
        NOTREACHED();
    }
  }
  return false;
}

void KeyPermissions::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // For the format of the dictionary see the documentation at kPrefKeyUsage.
  registry->RegisterDictionaryPref(prefs::kPlatformKeys);
}

void KeyPermissions::CreatePermissionObjectAndPassToCallback(
    const std::string& extension_id,
    const PermissionsCallback& callback,
    std::unique_ptr<base::Value> value) {
  callback.Run(std::make_unique<PermissionsForExtension>(
      extension_id, std::move(value), profile_prefs_, profile_policies_, this));
}

void KeyPermissions::SetPlatformKeysOfExtension(
    const std::string& extension_id,
    std::unique_ptr<base::Value> value) {
  extensions_state_store_->SetExtensionValue(
      extension_id, kStateStorePlatformKeys, std::move(value));
}

}  // namespace chromeos
