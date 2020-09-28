// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_impl.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/browser/state_store.h"

namespace chromeos {
namespace platform_keys {

namespace {
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

}  // namespace

KeyPermissionsServiceImpl::KeyPermissionsServiceImpl(
    bool profile_is_managed,
    PrefService* profile_prefs,
    policy::PolicyService* profile_policies,
    extensions::StateStore* extensions_state_store,
    PlatformKeysService* platform_keys_service)
    : profile_is_managed_(profile_is_managed),
      profile_prefs_(profile_prefs),
      profile_policies_(profile_policies),
      extensions_state_store_(extensions_state_store),
      platform_keys_service_(platform_keys_service) {
  DCHECK(profile_prefs_);
  DCHECK(extensions_state_store_);
  DCHECK(platform_keys_service_);
  DCHECK(!profile_is_managed_ || profile_policies_);
}

KeyPermissionsServiceImpl::~KeyPermissionsServiceImpl() = default;

void KeyPermissionsServiceImpl::CanUserGrantPermissionForKey(
    const std::string& public_key_spki_der,
    CanUserGrantPermissionForKeyCallback callback) const {
  platform_keys_service_->GetKeyLocations(
      public_key_spki_der,
      base::BindOnce(
          &KeyPermissionsServiceImpl::CanUserGrantPermissionForKeyWithLocations,
          weak_factory_.GetWeakPtr(), public_key_spki_der,
          std::move(callback)));
}

void KeyPermissionsServiceImpl::CanUserGrantPermissionForKeyWithLocations(
    const std::string& public_key_spki_der,
    CanUserGrantPermissionForKeyCallback callback,
    const std::vector<TokenId>& key_locations,
    Status key_locations_retrieval_status) const {
  auto bound_callback = base::BindOnce(
      &KeyPermissionsServiceImpl::
          CanUserGrantPermissionForKeyWithLocationsAndFlag,
      weak_factory_.GetWeakPtr(), public_key_spki_der, std::move(callback),
      key_locations, key_locations_retrieval_status);
  IsCorporateKeyWithLocations(public_key_spki_der, std::move(bound_callback),
                              key_locations, key_locations_retrieval_status);
}

void KeyPermissionsServiceImpl::
    CanUserGrantPermissionForKeyWithLocationsAndFlag(
        const std::string& public_key_spki_der,
        CanUserGrantPermissionForKeyCallback callback,
        const std::vector<TokenId>& key_locations,
        Status status,
        bool corporate_key) {
  if (status != Status::kSuccess) {
    std::move(callback).Run(/*allowed=*/false);
    return;
  }

  if (key_locations.empty()) {
    std::move(callback).Run(/*allowed=*/false);
    return;
  }

  // As keys cannot be tagged for non-corporate usage, the user can currently
  // not grant any permissions if the profile is managed.
  if (profile_is_managed_) {
    std::move(callback).Run(/*allowed=*/false);
    return;
  }

  // If this profile is not managed but we find a corporate key, don't allow
  // the user to grant permissions.
  std::move(callback).Run(/*allowed=*/!corporate_key);
}

void KeyPermissionsServiceImpl::IsCorporateKey(
    const std::string& public_key_spki_der,
    IsCorporateKeyCallback callback) const {
  platform_keys_service_->GetKeyLocations(
      public_key_spki_der,
      base::BindOnce(&KeyPermissionsServiceImpl::IsCorporateKeyWithLocations,
                     weak_factory_.GetWeakPtr(), public_key_spki_der,
                     std::move(callback)));
}

void KeyPermissionsServiceImpl::IsCorporateKeyWithLocations(
    const std::string& public_key_spki_der,
    IsCorporateKeyCallback callback,
    const std::vector<TokenId>& key_locations,
    Status status) const {
  if (status != Status::kSuccess) {
    LOG(ERROR) << "Key locations retrieval failed: " << StatusToString(status);
    std::move(callback).Run(/*corporate=*/false);
  }

  std::string public_key_spki_der_b64;
  base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

  for (const auto key_location : key_locations) {
    switch (key_location) {
      case TokenId::kUser:
        if (IsUserKeyCorporate(public_key_spki_der_b64)) {
          std::move(callback).Run(/*corporate=*/true);
          return;
        }
        break;
      case TokenId::kSystem:
        std::move(callback).Run(/*corporate=*/true);
        return;
    }
  }
  std::move(callback).Run(/*corporate=*/false);
}

void KeyPermissionsServiceImpl::SetCorporateKey(
    const std::string& public_key_spki_der,
    SetCorporateKeyCallback callback) const {
  platform_keys_service_->GetKeyLocations(
      public_key_spki_der,
      base::BindOnce(&KeyPermissionsServiceImpl::SetCorporateKeyWithLocations,
                     weak_factory_.GetWeakPtr(), public_key_spki_der,
                     std::move(callback)));
}

void KeyPermissionsServiceImpl::SetCorporateKeyWithLocations(
    const std::string& public_key_spki_der,
    SetCorporateKeyCallback callback,
    const std::vector<TokenId>& key_locations,
    Status key_locations_retrieval_status) const {
  if (key_locations_retrieval_status != Status::kSuccess) {
    std::move(callback).Run(key_locations_retrieval_status);
    return;
  }

  if (key_locations.empty()) {
    std::move(callback).Run(Status::kErrorKeyNotFound);
    return;
  }

  // A single key location is expected because this is intended for usage after
  // key generation / import, when exactly one location is relevant.
  DCHECK_EQ(key_locations.size(), 1U);

  switch (key_locations[0]) {
    case TokenId::kSystem:
      // Nothing to do - all system-token keys are currently implicitly
      // corporate.
      std::move(callback).Run(Status::kSuccess);
      return;
    case TokenId::kUser: {
      std::string public_key_spki_der_b64;
      base::Base64Encode(public_key_spki_der, &public_key_spki_der_b64);

      DictionaryPrefUpdate update(profile_prefs_, prefs::kPlatformKeys);

      std::unique_ptr<base::DictionaryValue> new_pref_entry(
          new base::DictionaryValue);
      new_pref_entry->SetKey(kPrefKeyUsage,
                             base::Value(kPrefKeyUsageCorporate));

      update->SetWithoutPathExpansion(public_key_spki_der_b64,
                                      std::move(new_pref_entry));
      std::move(callback).Run(Status::kSuccess);
      return;
    }
  }
}

bool KeyPermissionsServiceImpl::IsUserKeyCorporate(
    const std::string& public_key_spki_der_b64) const {
  const base::DictionaryValue* prefs_entry =
      GetPrefsEntry(public_key_spki_der_b64, profile_prefs_);
  if (prefs_entry) {
    const base::Value* key_usage = prefs_entry->FindKey(kPrefKeyUsage);
    if (!key_usage || !key_usage->is_string())
      return false;
    return key_usage->GetString() == kPrefKeyUsageCorporate;
  }
  return false;
}

}  // namespace platform_keys
}  // namespace chromeos
