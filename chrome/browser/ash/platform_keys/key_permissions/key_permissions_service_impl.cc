// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_impl.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash::platform_keys {

using ::chromeos::platform_keys::Status;
using ::chromeos::platform_keys::TokenId;

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
    std::vector<uint8_t> public_key_spki_der,
    CanUserGrantPermissionForKeyCallback callback) {
  auto key_locations_callback = base::BindOnce(
      &KeyPermissionsServiceImpl::CanUserGrantPermissionForKeyWithLocations,
      weak_factory_.GetWeakPtr(), public_key_spki_der, std::move(callback));
  platform_keys_service_->GetKeyLocations(std::move(public_key_spki_der),
                                          std::move(key_locations_callback));
}

void KeyPermissionsServiceImpl::CanUserGrantPermissionForKeyWithLocations(
    std::vector<uint8_t> public_key_spki_der,
    CanUserGrantPermissionForKeyCallback callback,
    const std::vector<TokenId>& key_locations,
    Status key_locations_retrieval_status) {
  if (key_locations_retrieval_status != Status::kSuccess) {
    LOG(ERROR) << "Key locations retrieval failed: "
               << StatusToString(key_locations_retrieval_status);
    std::move(callback).Run(/*allowed=*/false);
    return;
  }

  // It only makes sense to store the sign_unlimited flag for a key if it is on
  // a user slot. Currently, system-slot keys are implicitly corporate, so
  // CanUserGrantPermissionForKey should return false for them.
  if ((key_locations.size() != 1) || key_locations.front() != TokenId::kUser) {
    std::move(callback).Run(/*allowed=*/false);
    return;
  }

  auto bound_callback =
      base::BindOnce(&KeyPermissionsServiceImpl::
                         CanUserGrantPermissionForKeyWithLocationsAndFlag,
                     weak_factory_.GetWeakPtr(), public_key_spki_der,
                     std::move(callback), key_locations);
  IsCorporateKeyWithLocations(std::move(public_key_spki_der),
                              std::move(bound_callback), key_locations,
                              key_locations_retrieval_status);
}

void KeyPermissionsServiceImpl::
    CanUserGrantPermissionForKeyWithLocationsAndFlag(
        std::vector<uint8_t> public_key_spki_der,
        CanUserGrantPermissionForKeyCallback callback,
        const std::vector<TokenId>& key_locations,
        std::optional<bool> corporate_key,
        Status status) {
  if (status != Status::kSuccess) {
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
  std::move(callback).Run(/*allowed=*/!corporate_key.value());
}

void KeyPermissionsServiceImpl::IsCorporateKey(
    std::vector<uint8_t> public_key_spki_der,
    IsCorporateKeyCallback callback) {
  auto key_locations_callback = base::BindOnce(
      &KeyPermissionsServiceImpl::IsCorporateKeyWithLocations,
      weak_factory_.GetWeakPtr(), public_key_spki_der, std::move(callback));
  platform_keys_service_->GetKeyLocations(std::move(public_key_spki_der),
                                          std::move(key_locations_callback));
}

void KeyPermissionsServiceImpl::IsCorporateKeyWithLocations(
    std::vector<uint8_t> public_key_spki_der,
    IsCorporateKeyCallback callback,
    const std::vector<TokenId>& key_locations,
    Status status) {
  if (status != Status::kSuccess) {
    LOG(ERROR) << "Key locations retrieval failed: " << StatusToString(status);
    std::move(callback).Run(/*corporate=*/std::nullopt, status);
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
                KeyUsage::kCorporate, std::move(public_key_spki_der));
        return;
    }
  }

  if (key_on_user_token_only) {
    DCHECK(is_regular_user_profile_);
    profile_key_permissions_manager_->IsKeyAllowedForUsage(
        base::BindOnce(
            &KeyPermissionsServiceImpl::IsCorporateKeyWithKpmResponse,
            weak_factory_.GetWeakPtr(), std::move(callback)),
        KeyUsage::kCorporate, std::move(public_key_spki_der));
    return;
  }

  std::move(callback).Run(/*corporate=*/false, Status::kSuccess);
}

void KeyPermissionsServiceImpl::IsCorporateKeyWithKpmResponse(
    IsCorporateKeyCallback callback,
    std::optional<bool> allowed,
    Status status) {
  if (allowed.has_value()) {
    std::move(callback).Run(allowed.value(), Status::kSuccess);
    return;
  }

  LOG(ERROR) << "Checking corporate flag via KeyPermissionsManager failed: "
             << StatusToString(status);
  std::move(callback).Run(/*corporate=*/std::nullopt, status);
}

void KeyPermissionsServiceImpl::SetCorporateKey(
    std::vector<uint8_t> public_key_spki_der,
    SetCorporateKeyCallback callback) {
  auto key_locations_callback = base::BindOnce(
      &KeyPermissionsServiceImpl::SetCorporateKeyWithLocations,
      weak_factory_.GetWeakPtr(), public_key_spki_der, std::move(callback));
  platform_keys_service_->GetKeyLocations(std::move(public_key_spki_der),
                                          std::move(key_locations_callback));
}

void KeyPermissionsServiceImpl::SetCorporateKeyWithLocations(
    std::vector<uint8_t> public_key_spki_der,
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
                             std::move(public_key_spki_der));
      return;
    case TokenId::kUser: {
      DCHECK(is_regular_user_profile_);

      profile_key_permissions_manager_->AllowKeyForUsage(
          std::move(callback), KeyUsage::kCorporate,
          std::move(public_key_spki_der));
      return;
    }
  }
}

}  // namespace ash::platform_keys
