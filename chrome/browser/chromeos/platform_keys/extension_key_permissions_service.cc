// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_impl.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "extensions/browser/state_store.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/profiles/profile.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_factory_ash.h"
#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

namespace chromeos::platform_keys {

namespace {
const char kStateStoreSPKI[] = "SPKI";
const char kStateStoreSignOnce[] = "signOnce";
const char kStateStoreSignUnlimited[] = "signUnlimited";

const char kPolicyAllowCorporateKeyUsage[] = "allowCorporateKeyUsage";

bool ContainsTag(uint64_t tags, crosapi::mojom::KeyTag value) {
  return tags & static_cast<uint64_t>(value);
}

const base::Value::Dict* GetKeyPermissionsMap(
    policy::PolicyService* const profile_policies) {
  if (!profile_policies)
    return nullptr;

  const policy::PolicyMap& policies = profile_policies->GetPolicies(
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  const base::Value* policy_value =
      policies.GetValue(policy::key::kKeyPermissions, base::Value::Type::DICT);
  if (!policy_value) {
    DVLOG(1) << "KeyPermissions policy is not set";
    return nullptr;
  }
  return policy_value->GetIfDict();
}

bool GetCorporateKeyUsageFromPref(
    const base::Value::Dict* key_permissions_for_ext) {
  if (!key_permissions_for_ext)
    return false;

  const base::Value* allow_corporate_key_usage =
      key_permissions_for_ext->Find(kPolicyAllowCorporateKeyUsage);
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

  const base::Value::Dict* key_permissions_map =
      GetKeyPermissionsMap(profile_policies);
  if (!key_permissions_map)
    return false;

  const base::Value::Dict* key_permissions_for_ext =
      key_permissions_map->FindDict(extension_id);
  if (!key_permissions_for_ext)
    return false;

  bool allow_corporate_key_usage =
      GetCorporateKeyUsageFromPref(key_permissions_for_ext);

  VLOG_IF(2, allow_corporate_key_usage)
      << "Policy allows usage of corporate keys by extension " << extension_id;
  return allow_corporate_key_usage;
}

// Returns appropriate KeystoreService for |browser_context|.
//
// KeystoreService is expected to always outlive ExtensionKeyPermissionsService
// because ExtensionKeyPermissionsService instances are owned by
// ExtensionPlatformKeysService and:
//
// For Lacros-Chrome it returns a remote mojo implementation owned by
// LacrosService (that is created before the start of the main loop and should
// outlive ExtensionPlatformKeysService).
//
// For Ash-Chrome the factory can return:
// * an instance owned by CrosapiManager (that is created before profiles and
// should outlive ExtensionPlatformKeysService)
// * or an appropriate keyed service that will always exist
// during ExtensionPlatformKeysService lifetime (because of KeyedService
// dependencies).
crosapi::mojom::KeystoreService* GetKeystoreService(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/191958380): Lift the restriction when *.platformKeys.* APIs are
  // implemented for secondary profiles in Lacros.
  CHECK(Profile::FromBrowserContext(browser_context)->IsMainProfile())
      << "Attempted to use an incorrect profile. Please file a bug at "
         "https://bugs.chromium.org/ if this happens.";
  return chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::KeystoreService>()
      .get();
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::KeystoreServiceFactoryAsh::GetForBrowserContext(
      browser_context);
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace

ExtensionKeyPermissionsService::ExtensionKeyPermissionsService(
    const std::string& extension_id,
    extensions::StateStore* extensions_state_store,
    base::Value::List state_store_value,
    policy::PolicyService* profile_policies,
    content::BrowserContext* browser_context)
    : extension_id_(extension_id),
      extensions_state_store_(extensions_state_store),
      profile_policies_(profile_policies),
      keystore_service_(GetKeystoreService(browser_context)) {
  DCHECK(extensions_state_store_);
  DCHECK(profile_policies_);
  DCHECK(keystore_service_);
  KeyEntriesFromState(state_store_value);
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

  state_store_entries_.emplace_back(public_key_spki_der_b64);
  return &state_store_entries_.back();
}

void ExtensionKeyPermissionsService::CanUseKeyForSigning(
    const std::vector<uint8_t>& public_key_spki_der,
    CanUseKeyForSigningCallback callback) {
  KeyEntry* matching_entry =
      GetStateStoreEntry(base::Base64Encode(public_key_spki_der));

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
  keystore_service_->GetKeyTags(public_key_spki_der, std::move(bound_callback));
}

void ExtensionKeyPermissionsService::CanUseKeyForSigningWithFlags(
    CanUseKeyForSigningCallback callback,
    bool sign_unlimited_allowed,
    crosapi::mojom::GetKeyTagsResultPtr key_tags) {
  if (key_tags->is_error()) {
    LOG(ERROR) << "Failed to check if the key is corporate: "
               << KeystoreErrorToString(key_tags->get_error());
    std::move(callback).Run(/*allowed=*/false);
    return;
  }

  // Usage of corporate keys is solely determined by policy. The user must not
  // circumvent this decision.
  if (ContainsTag(key_tags->get_tags(), crosapi::mojom::KeyTag::kCorporate)) {
    std::move(callback).Run(/*allowed=*/PolicyAllowsCorporateKeyUsage());
    return;
  }

  // Only permissions for keys that are not designated for corporate usage are
  // determined by user decisions.
  std::move(callback).Run(sign_unlimited_allowed);
}

void ExtensionKeyPermissionsService::SetKeyUsedForSigning(
    const std::vector<uint8_t>& public_key_spki_der,
    SetKeyUsedForSigningCallback callback) {
  KeyEntry* matching_entry =
      GetStateStoreEntry(base::Base64Encode(public_key_spki_der));
  matching_entry->sign_once = false;
  WriteToStateStore();

  // Return success.
  std::move(callback).Run(/*is_error=*/false,
                          /*error=*/crosapi::mojom::KeystoreError::kUnknown);
}

void ExtensionKeyPermissionsService::RegisterKeyForCorporateUsage(
    const std::vector<uint8_t>& public_key_spki_der,
    RegisterKeyForCorporateUsageCallback callback) {
  KeyEntry* matching_entry =
      GetStateStoreEntry(base::Base64Encode(public_key_spki_der));

  if (matching_entry->sign_once) {
    VLOG(1) << "Key is already allowed for signing, skipping.";
    // Return success.
    std::move(callback).Run(/*is_error=*/false,
                            /*error=*/crosapi::mojom::KeystoreError::kUnknown);
    return;
  }

  matching_entry->sign_once = true;
  WriteToStateStore();

  keystore_service_->AddKeyTags(
      public_key_spki_der,
      static_cast<uint64_t>(crosapi::mojom::KeyTag::kCorporate),
      std::move(callback));
}

void ExtensionKeyPermissionsService::SetUserGrantedPermission(
    const std::vector<uint8_t>& public_key_spki_der,
    SetUserGrantedPermissionCallback callback) {
  keystore_service_->CanUserGrantPermissionForKey(
      public_key_spki_der,
      base::BindOnce(
          &ExtensionKeyPermissionsService::SetUserGrantedPermissionWithFlag,
          weak_factory_.GetWeakPtr(), public_key_spki_der,
          std::move(callback)));
}

void ExtensionKeyPermissionsService::SetUserGrantedPermissionWithFlag(
    const std::vector<uint8_t>& public_key_spki_der,
    SetUserGrantedPermissionCallback callback,
    bool can_user_grant_permission) {
  if (!can_user_grant_permission) {
    std::move(callback).Run(
        /*is_error=*/true,
        crosapi::mojom::KeystoreError::kGrantKeyPermissionForExtension);
    return;
  }

  KeyEntry* matching_entry =
      GetStateStoreEntry(base::Base64Encode(public_key_spki_der));

  if (matching_entry->sign_unlimited) {
    VLOG(1) << "Key is already allowed for signing, skipping.";
    // Return success.
    std::move(callback).Run(/*is_error=*/false,
                            /*error=*/crosapi::mojom::KeystoreError::kUnknown);
    return;
  }

  matching_entry->sign_unlimited = true;
  WriteToStateStore();
  // Return success.
  std::move(callback).Run(/*is_error=*/false,
                          /*error=*/crosapi::mojom::KeystoreError::kUnknown);
}

bool ExtensionKeyPermissionsService::PolicyAllowsCorporateKeyUsage() const {
  return PolicyAllowsCorporateKeyUsageForExtension(extension_id_,
                                                   profile_policies_);
}

void ExtensionKeyPermissionsService::WriteToStateStore() {
  extensions_state_store_->SetExtensionValue(
      extension_id_, kStateStorePlatformKeys, base::Value(KeyEntriesToState()));
}

void ExtensionKeyPermissionsService::KeyEntriesFromState(
    const base::Value::List& state) {
  state_store_entries_.clear();

  for (const auto& entry : state) {
    std::string spki_b64;
    if (entry.is_string()) {
      spki_b64 = entry.GetString();
      // This handles the case that the store contained a plain list of base64
      // and DER-encoded SPKIs from an older version of ChromeOS.
      KeyEntry new_entry(spki_b64);
      new_entry.sign_once = true;
      state_store_entries_.push_back(new_entry);
    } else if (entry.is_dict()) {
      const base::Value::Dict& dict_entry = entry.GetDict();
      const std::string* spki_b64_str = dict_entry.FindString(kStateStoreSPKI);
      if (spki_b64_str)
        spki_b64 = *spki_b64_str;
      KeyEntry new_entry(spki_b64);
      new_entry.sign_once = dict_entry.FindBool(kStateStoreSignOnce)
                                .value_or(new_entry.sign_once);
      new_entry.sign_unlimited = dict_entry.FindBool(kStateStoreSignUnlimited)
                                     .value_or(new_entry.sign_unlimited);
      state_store_entries_.push_back(new_entry);
    } else {
      LOG(ERROR) << "Found invalid entry of type " << entry.type()
                 << " in PlatformKeys state store.";
      continue;
    }
  }
}

base::Value::List ExtensionKeyPermissionsService::KeyEntriesToState() {
  base::Value::List new_state;
  for (const KeyEntry& entry : state_store_entries_) {
    // Drop entries that the extension doesn't have any permissions for anymore.
    if (!entry.sign_once && !entry.sign_unlimited)
      continue;

    base::Value::Dict new_entry;
    new_entry.Set(kStateStoreSPKI, entry.spki_b64);
    // Omit writing default values, namely |false|.
    if (entry.sign_once) {
      new_entry.Set(kStateStoreSignOnce, entry.sign_once);
    }
    if (entry.sign_unlimited) {
      new_entry.Set(kStateStoreSignUnlimited, entry.sign_unlimited);
    }
    new_state.Append(std::move(new_entry));
  }
  return new_state;
}

// static
std::vector<std::string>
ExtensionKeyPermissionsService::GetCorporateKeyUsageAllowedAppIds(
    policy::PolicyService* const profile_policies) {
  std::vector<std::string> permissions;

  const base::Value::Dict* key_permissions_service_map =
      GetKeyPermissionsMap(profile_policies);
  if (!key_permissions_service_map)
    return permissions;

  for (const auto item : *key_permissions_service_map) {
    const auto& app_id = item.first;
    const base::Value::Dict* key_permissions_service_for_app =
        item.second.GetIfDict();
    if (!key_permissions_service_for_app) {
      continue;
    }
    if (GetCorporateKeyUsageFromPref(key_permissions_service_for_app))
      permissions.push_back(app_id);
  }
  return permissions;
}

}  // namespace chromeos::platform_keys
