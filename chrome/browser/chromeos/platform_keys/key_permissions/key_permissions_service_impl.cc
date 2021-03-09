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
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace chromeos {
namespace platform_keys {

KeyPermissionsServiceImpl::KeyPermissionsServiceImpl(
    bool is_regular_user_profile,
    bool profile_is_managed,
    PlatformKeysService* platform_keys_service,
    KeyPermissionsManager* profile_key_permissions_manager)
    : is_regular_user_profile_(is_regular_user_profile),
      profile_is_managed_(profile_is_managed),
      platform_keys_service_(platform_keys_service),
      profile_key_permissions_manager_(profile_key_permissions_manager) {
  DCHECK(platform_keys_service_);
  DCHECK(profile_key_permissions_manager || !is_regular_user_profile);
}

KeyPermissionsServiceImpl::~KeyPermissionsServiceImpl() = default;

void KeyPermissionsServiceImpl::CanUserGrantPermissionForKey(
    const std::string& public_key_spki_der,
    CanUserGrantPermissionForKeyCallback callback) {
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
    Status key_locations_retrieval_status) {
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
    IsCorporateKeyCallback callback) {
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
    Status status) {
  if (status != Status::kSuccess) {
    LOG(ERROR) << "Key locations retrieval failed: " << StatusToString(status);
    std::move(callback).Run(/*corporate=*/false);
    return;
  }

  bool key_on_user_token_only = false;
  for (const auto key_location : key_locations) {
    switch (key_location) {
      case TokenId::kUser:
        key_on_user_token_only = true;
        break;
      case TokenId::kSystem:
        KeyPermissionsManagerImpl::GetSystemTokenKeyPermissionsManager()
            ->IsKeyAllowedForUsage(
                base::BindOnce(
                    &KeyPermissionsServiceImpl::IsCorporateKeyWithKpmResponse,
                    weak_factory_.GetWeakPtr(), std::move(callback)),
                KeyUsage::kCorporate, public_key_spki_der);
        return;
    }
  }

  if (key_on_user_token_only) {
    DCHECK(is_regular_user_profile_);
    profile_key_permissions_manager_->IsKeyAllowedForUsage(
        base::BindOnce(
            &KeyPermissionsServiceImpl::IsCorporateKeyWithKpmResponse,
            weak_factory_.GetWeakPtr(), std::move(callback)),
        KeyUsage::kCorporate, public_key_spki_der);
    return;
  }

  std::move(callback).Run(/*corporate=*/false);
}

void KeyPermissionsServiceImpl::IsCorporateKeyWithKpmResponse(
    IsCorporateKeyCallback callback,
    base::Optional<bool> allowed,
    Status status) {
  if (allowed.has_value()) {
    std::move(callback).Run(allowed.value());
    return;
  }

  LOG(ERROR) << "Checking corporate flag via KeyPermissionsManager failed: "
             << StatusToString(status);
  std::move(callback).Run(/*corporate=*/false);
}

void KeyPermissionsServiceImpl::SetCorporateKey(
    const std::string& public_key_spki_der,
    SetCorporateKeyCallback callback) {
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
    Status key_locations_retrieval_status) {
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
      KeyPermissionsManagerImpl::GetSystemTokenKeyPermissionsManager()
          ->AllowKeyForUsage(std::move(callback), KeyUsage::kCorporate,
                             public_key_spki_der);
      return;
    case TokenId::kUser: {
      DCHECK(is_regular_user_profile_);

      profile_key_permissions_manager_->AllowKeyForUsage(
          std::move(callback), KeyUsage::kCorporate, public_key_spki_der);
      return;
    }
  }
}

}  // namespace platform_keys
}  // namespace chromeos
