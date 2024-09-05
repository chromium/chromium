// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager_impl.h"

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/components/kcer/key_permissions.pb.h"
#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_manager.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_util.h"
#include "chrome/browser/ash/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service.h"
#include "chrome/browser/ash/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/platform_keys/extension_key_permissions_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace ash::platform_keys {

namespace {

using ::chromeos::platform_keys::KeyAttributeType;
using ::chromeos::platform_keys::Status;
using ::chromeos::platform_keys::TokenId;
using MigrationStatus =
    KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::MigrationStatus;

bool g_one_time_migration_enabled_for_testing = true;

// Owned by `ChromeBrowserMainPartsAsh`.
KeyPermissionsManager* g_system_token_key_permissions_manager = nullptr;

KeyPermissionsManager* g_system_token_kpm_for_testing = nullptr;

chaps::KeyPermissions CreateKeyPermissions(bool corporate_usage_allowed,
                                           bool arc_usage_allowed) {
  chaps::KeyPermissions key_permissions;
  key_permissions.mutable_key_usages()->set_corporate(corporate_usage_allowed);
  key_permissions.mutable_key_usages()->set_arc(arc_usage_allowed);
  return key_permissions;
}

// Parses `permissions_from_chaps` as a KeyPermissions proto message and
// returns true if it contains the flag that the corporate usage is allowed.
bool IsCorporateUsageAllowedByChaps(
    const std::optional<std::vector<uint8_t>>& permissions_from_chaps) {
  if (!permissions_from_chaps.has_value()) {
    return false;
  }
  chaps::KeyPermissions key_permissions;
  if (!key_permissions.ParseFromArray(permissions_from_chaps->data(),
                                      permissions_from_chaps->size())) {
    LOG(WARNING) << "Failed to parse KeyPermissions";
    return false;
  }
  return (key_permissions.has_key_usages() &&
          key_permissions.key_usages().corporate());
}

void OnArcKeyPermissionsInChapsUpdated(bool /*migration_was_necessary*/,
                                       Status update_status) {
  if (update_status != Status::kSuccess) {
    LOG(ERROR) << "Updating arc key permissions in chaps failed.";
  }
}

}  // namespace

KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::
    KeyPermissionsInChapsUpdater(
        Mode mode,
        KeyPermissionsManagerImpl* key_permissions_manager)
    : mode_(mode), key_permissions_manager_(key_permissions_manager) {
  DCHECK(key_permissions_manager_);
}

KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::
    ~KeyPermissionsInChapsUpdater() = default;

bool KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::
    IsCorporateUsageAllowedByPrefs(
        const std::vector<uint8_t>& public_key_spki_der) const {
  return (key_permissions_manager_->token_id_ == TokenId::kSystem) ||
         internal::IsUserKeyMarkedCorporateInPref(
             public_key_spki_der, key_permissions_manager_->pref_service_);
}

void KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::Update(
    UpdateCallback callback) {
  DCHECK(!update_started_) << "Update called more than once for the same "
                              "updater instance.";

  update_started_ = true;
  callback_ = std::move(callback);

  key_permissions_manager_->platform_keys_service_->GetAllKeys(
      key_permissions_manager_->token_id_,
      base::BindOnce(&KeyPermissionsInChapsUpdater::UpdateWithAllKeys,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::UpdateWithAllKeys(
    std::vector<std::vector<uint8_t>> public_key_spki_der_list,
    Status keys_retrieval_status) {
  DCHECK(public_key_spki_der_queue_.empty());

  for (std::vector<uint8_t>& public_key : public_key_spki_der_list) {
    // For the keys that are not on the system token or don't have a record in
    // the preferences there's nothing to migrate, filter them out.
    if (IsCorporateUsageAllowedByPrefs(public_key)) {
      public_key_spki_der_queue_.emplace(std::move(public_key));
    }
  }

  UpdateNextKey();
}

// This will be called repeatedly until `public_key_spki_der_queue_` is empty.
void KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::UpdateNextKey() {
  if (public_key_spki_der_queue_.empty()) {
    std::move(callback_).Run(migration_was_necessary_, Status::kSuccess);
    return;
  }

  std::vector<uint8_t> public_key =
      std::move(public_key_spki_der_queue_.front());
  public_key_spki_der_queue_.pop();

  auto attributes_callback = base::BindOnce(
      &KeyPermissionsInChapsUpdater::UpdateNextKeyWithExistingPermissions,
      weak_ptr_factory_.GetWeakPtr(), public_key);

  key_permissions_manager_->platform_keys_service_->GetAttributeForKey(
      key_permissions_manager_->token_id_, std::move(public_key),
      chromeos::platform_keys::KeyAttributeType::kKeyPermissions,
      std::move(attributes_callback));
}

void KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::
    UpdateNextKeyWithExistingPermissions(
        std::vector<uint8_t> public_key,
        std::optional<std::vector<uint8_t>> permissions,
        Status permissions_retrieval_status) {
  if (IsCorporateUsageAllowedByChaps(permissions)) {
    // Chaps already knowns about the permissions for the current key, nothing
    // to do, continue to the next key.
    return UpdateNextKey();
  }

  migration_was_necessary_ = true;
  return UpdatePermissionsForKey(public_key);
}

void KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::
    UpdatePermissionsForKey(std::vector<uint8_t> public_key_spki_der) {
  switch (mode_) {
    case Mode::kMigratePermissionsFromPrefs: {
      bool corporate_usage_allowed =
          IsCorporateUsageAllowedByPrefs(public_key_spki_der);
      UpdatePermissionsForKeyWithCorporateFlag(
          std::move(public_key_spki_der), corporate_usage_allowed,
          /*corporate_usage_retrieval_status=*/Status::kSuccess);
      return;
    }
    case Mode::kUpdateArcUsageFlag: {
      auto cb =
          base::BindOnce(&KeyPermissionsInChapsUpdater::
                             UpdatePermissionsForKeyWithCorporateFlag,
                         weak_ptr_factory_.GetWeakPtr(), public_key_spki_der);
      key_permissions_manager_->IsKeyAllowedForUsage(
          std::move(cb), KeyUsage::kCorporate, std::move(public_key_spki_der));
      return;
    }
  }
}

void KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::
    UpdatePermissionsForKeyWithCorporateFlag(
        std::vector<uint8_t> public_key_spki_der,
        std::optional<bool> corporate_usage_allowed,
        Status corporate_usage_retrieval_status) {
  if (corporate_usage_retrieval_status != Status::kSuccess) {
    LOG(ERROR) << "Couldn't retrieve corporate usage flag for a key.";
    std::move(callback_).Run(migration_was_necessary_,
                             corporate_usage_retrieval_status);
    return;
  }

  DCHECK(corporate_usage_allowed.has_value());

  bool arc_usage_allowed =
      corporate_usage_allowed.value() &&
      key_permissions_manager_->AreCorporateKeysAllowedForArcUsage();

  chaps::KeyPermissions key_permissions =
      CreateKeyPermissions(corporate_usage_allowed.value(), arc_usage_allowed);

  key_permissions_manager_->platform_keys_service_->SetAttributeForKey(
      key_permissions_manager_->token_id_, std::move(public_key_spki_der),
      KeyAttributeType::kKeyPermissions,
      internal::KeyPermissionsProtoToBytes(key_permissions),
      base::BindOnce(&KeyPermissionsInChapsUpdater::OnKeyPermissionsUpdated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KeyPermissionsManagerImpl::KeyPermissionsInChapsUpdater::
    OnKeyPermissionsUpdated(Status permissions_update_status) {
  if (permissions_update_status == Status::kErrorKeyNotFound) {
    // Some public keys are not removed from chaps although their corresponding
    // private keys are removed. We continue the migration process if we
    // received kKeyNotFound as a workaround until the keys-clean-up problem is
    // solved (crbug.com/1096051).
    LOG(WARNING) << "Corresponding private key not found. Continuing the "
                    "migration process...";
  } else if (permissions_update_status != Status::kSuccess) {
    LOG(ERROR) << "Couldn't update permissions for a key: "
               << StatusToString(permissions_update_status);
    base::UmaHistogramEnumeration(kMigrationStatusHistogramName,
                                  MigrationStatus::kFailedToUpdatePermissions);
    std::move(callback_).Run(migration_was_necessary_,
                             permissions_update_status);
    return;
  }

  UpdateNextKey();
}

// static
KeyPermissionsManager*
KeyPermissionsManagerImpl::GetSystemTokenKeyPermissionsManager() {
  if (g_system_token_kpm_for_testing) {
    return g_system_token_kpm_for_testing;
  }

  return g_system_token_key_permissions_manager;
}

// static
KeyPermissionsManager*
KeyPermissionsManagerImpl::GetUserPrivateTokenKeyPermissionsManager(
    Profile* profile) {
  auto* const user_private_token_kpm_service =
      UserPrivateTokenKeyPermissionsManagerServiceFactory::GetInstance()
          ->GetForBrowserContext(profile);

  if (!user_private_token_kpm_service) {
    DCHECK(!ProfileHelper::IsUserProfile(profile));
    return nullptr;
  }

  return user_private_token_kpm_service->key_permissions_manager();
}

// static
void KeyPermissionsManagerImpl::SetSystemTokenKeyPermissionsManagerForTesting(
    KeyPermissionsManager* system_token_kpm_for_testing) {
  g_system_token_kpm_for_testing = system_token_kpm_for_testing;
}

std::unique_ptr<KeyPermissionsManager>
KeyPermissionsManagerImpl::CreateSystemTokenKeyPermissionsManager() {
  DCHECK(!g_system_token_key_permissions_manager);

  auto system_token_key_permissions_manager =
      std::make_unique<KeyPermissionsManagerImpl>(
          TokenId::kSystem, std::make_unique<SystemTokenArcKpmDelegate>(),
          PlatformKeysServiceFactory::GetInstance()->GetDeviceWideService(),
          g_browser_process->local_state());
  g_system_token_key_permissions_manager =
      system_token_key_permissions_manager.get();
  return std::move(system_token_key_permissions_manager);
}

// static
void KeyPermissionsManagerImpl::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kKeyPermissionsOneTimeMigrationDone,
                                /*default_value=*/false);
}

// static
void KeyPermissionsManagerImpl::SetOneTimeMigrationEnabledForTesting(
    bool enabled) {
  g_one_time_migration_enabled_for_testing = enabled;
}

KeyPermissionsManagerImpl::KeyPermissionsManagerImpl(
    TokenId token_id,
    std::unique_ptr<ArcKpmDelegate> arc_usage_manager_delegate,
    PlatformKeysService* platform_keys_service,
    PrefService* pref_service)
    : token_id_(token_id),
      arc_usage_manager_delegate_(std::move(arc_usage_manager_delegate)),
      platform_keys_service_(platform_keys_service),
      pref_service_(pref_service) {
  DCHECK(arc_usage_manager_delegate_);
  DCHECK(platform_keys_service_);
  DCHECK(pref_service_);

  arc_usage_manager_delegate_observation_.Observe(
      arc_usage_manager_delegate_.get());

  // This waits until the token this KPM is responsible for is available.
  platform_keys_service_->GetTokens(base::BindOnce(
      &KeyPermissionsManagerImpl::OnGotTokens, weak_ptr_factory_.GetWeakPtr()));
}

KeyPermissionsManagerImpl::~KeyPermissionsManagerImpl() = default;

void KeyPermissionsManagerImpl::OnGotTokens(
    const std::vector<TokenId> token_ids,
    Status status) {
  if (status != Status::kSuccess) {
    LOG(ERROR) << "Error while waiting for token to be ready: "
               << StatusToString(status);
    return;
  }

  if (!base::Contains(token_ids, token_id_)) {
    LOG(ERROR) << "KeyPermissionsManager doesn't have access to token: "
               << static_cast<int>(token_id_);
    return;
  }

  if (!IsOneTimeMigrationDone()) {
    StartOneTimeMigration();
  } else {
    OnReadyForQueries();
    // On initialization, ARC usage allowance for corporate keys may be
    // different than after the one-time migration ends, so we trigger an update
    // in chaps.
    UpdateArcKeyPermissionsInChaps();
  }
}

void KeyPermissionsManagerImpl::AllowKeyForUsage(
    AllowKeyForUsageCallback callback,
    KeyUsage usage,
    std::vector<uint8_t> public_key_spki_der) {
  if (!ready_for_queries_) {
    queries_waiting_list_.push_back(
        base::BindOnce(&KeyPermissionsManagerImpl::AllowKeyForUsage,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       usage, std::move(public_key_spki_der)));
    return;
  }

  switch (usage) {
    case KeyUsage::kArc:
      LOG(ERROR) << "ARC usage of corporate keys is managed internally by "
                    "ArcKpmDelegate.";
      std::move(callback).Run(Status::kErrorInternal);
      break;
    case KeyUsage::kCorporate: {
      AllowKeyForCorporateUsage(std::move(callback),
                                std::move(public_key_spki_der));
      break;
    }
  }
}

void KeyPermissionsManagerImpl::IsKeyAllowedForUsage(
    IsKeyAllowedForUsageCallback callback,
    KeyUsage usage,
    std::vector<uint8_t> public_key_spki_der) {
  if (!ready_for_queries_) {
    queries_waiting_list_.push_back(
        base::BindOnce(&KeyPermissionsManagerImpl::IsKeyAllowedForUsage,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       usage, std::move(public_key_spki_der)));
    return;
  }

  // All system token keys are allowed for corporate usage by default.
  if (usage == KeyUsage::kCorporate && token_id_ == TokenId::kSystem) {
    std::move(callback).Run(/*allowed=*/true, Status::kSuccess);
    return;
  }

  platform_keys_service_->GetAttributeForKey(
      token_id_, std::move(public_key_spki_der),
      KeyAttributeType::kKeyPermissions,
      base::BindOnce(
          &KeyPermissionsManagerImpl::IsKeyAllowedForUsageWithPermissions,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), usage));
}

void KeyPermissionsManagerImpl::AllowKeyForCorporateUsage(
    AllowKeyForUsageCallback callback,
    std::vector<uint8_t> public_key_spki_der) {
  chaps::KeyPermissions key_permissions = CreateKeyPermissions(
      /*corporate_usage_allowed=*/true, AreCorporateKeysAllowedForArcUsage());

  platform_keys_service_->SetAttributeForKey(
      token_id_, std::move(public_key_spki_der),
      KeyAttributeType::kKeyPermissions,
      internal::KeyPermissionsProtoToBytes(key_permissions),
      std::move(callback));
}

void KeyPermissionsManagerImpl::IsKeyAllowedForUsageWithPermissions(
    IsKeyAllowedForUsageCallback callback,
    KeyUsage usage,
    std::optional<std::vector<uint8_t>> serialized_key_permissions,
    Status key_attribute_retrieval_status) {
  if (key_attribute_retrieval_status != Status::kSuccess) {
    LOG(ERROR) << "Error while retrieving key permissions: "
               << StatusToString(key_attribute_retrieval_status);
    std::move(callback).Run(/*allowed=*/false, key_attribute_retrieval_status);
    return;
  }

  if (!serialized_key_permissions.has_value()) {
    std::move(callback).Run(/*allowed=*/false, Status::kSuccess);
    return;
  }

  chaps::KeyPermissions key_permissions;
  if (!internal::KeyPermissionsProtoFromBytes(
          serialized_key_permissions.value(), key_permissions)) {
    LOG(ERROR) << "Couldn't deserialize key permissions proto message.";
    std::move(callback).Run(/*allowed=*/false, Status::kErrorInternal);
    return;
  }

  bool allowed = false;
  switch (usage) {
    case KeyUsage::kArc:
      allowed = key_permissions.key_usages().arc();
      break;
    case KeyUsage::kCorporate:
      allowed = key_permissions.key_usages().corporate();
      break;
  }
  std::move(callback).Run(allowed, Status::kSuccess);
}

bool KeyPermissionsManagerImpl::AreCorporateKeysAllowedForArcUsage() const {
  return arc_usage_manager_delegate_->AreCorporateKeysAllowedForArcUsage();
}

void KeyPermissionsManagerImpl::Shutdown() {
  arc_usage_manager_delegate_->Shutdown();
  platform_keys_service_ = nullptr;
  pref_service_ = nullptr;
}

void KeyPermissionsManagerImpl::UpdateArcKeyPermissionsInChaps() {
  if (!IsOneTimeMigrationDone()) {
    // This function will always be called after the one-time migration is done.
    return;
  }

  key_permissions_in_chaps_updater_ =
      std::make_unique<KeyPermissionsInChapsUpdater>(
          KeyPermissionsInChapsUpdater::Mode::kUpdateArcUsageFlag, this);
  key_permissions_in_chaps_updater_->Update(
      base::BindOnce(&OnArcKeyPermissionsInChapsUpdated));
}

void KeyPermissionsManagerImpl::StartOneTimeMigration() {
  DCHECK(!IsOneTimeMigrationDone());

  if (!g_one_time_migration_enabled_for_testing) {
    return;
  }

  VLOG(0) << "One-time key permissions migration started for token: "
          << static_cast<int>(token_id_) << ".";
  base::UmaHistogramEnumeration(kMigrationStatusHistogramName,
                                MigrationStatus::kStarted);

  DCHECK(!key_permissions_in_chaps_updater_);
  key_permissions_in_chaps_updater_ =
      std::make_unique<KeyPermissionsInChapsUpdater>(
          KeyPermissionsInChapsUpdater::Mode::kMigratePermissionsFromPrefs,
          this);
  key_permissions_in_chaps_updater_->Update(
      base::BindOnce(&KeyPermissionsManagerImpl::OnOneTimeMigrationDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void KeyPermissionsManagerImpl::OnOneTimeMigrationDone(
    bool migration_was_necessary,
    Status migration_status) {
  if (migration_status != Status::kSuccess) {
    VLOG(0) << "One-time key permissions migration failed for token: "
            << static_cast<int>(token_id_) << ".";
    base::UmaHistogramEnumeration(kMigrationStatusHistogramName,
                                  MigrationStatus::kFailed);
    return;
  }

  VLOG(0) << "One-time key permissions migration succeeded for token: "
          << static_cast<int>(token_id_) << ".";
  base::UmaHistogramEnumeration(kMigrationStatusHistogramName,
                                MigrationStatus::kSucceeded);
  if (migration_was_necessary) {
    base::UmaHistogramEnumeration(kMigrationStatusHistogramName,
                                  MigrationStatus::kNecessary);
  }

  pref_service_->SetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone, true);

  OnReadyForQueries();

  // Double-check keys permissions after the migration is done just in case any
  // ARC updates happened during the migration.
  UpdateArcKeyPermissionsInChaps();
}

bool KeyPermissionsManagerImpl::IsOneTimeMigrationDone() const {
  return pref_service_->GetBoolean(prefs::kKeyPermissionsOneTimeMigrationDone);
}

void KeyPermissionsManagerImpl::OnArcUsageAllowanceForCorporateKeysChanged(
    bool allowed) {
  if (allowed == arc_usage_allowed_for_corporate_keys_) {
    return;
  }

  if (allowed) {
    VLOG(0) << "ARC usage is allowed for corporate keys on token: "
            << static_cast<int>(token_id_) << ".";
  } else {
    VLOG(0) << "ARC usage is not allowed for corporate keys on token: "
            << static_cast<int>(token_id_) << ".";
  }

  arc_usage_allowed_for_corporate_keys_ = allowed;
  UpdateArcKeyPermissionsInChaps();
}

void KeyPermissionsManagerImpl::OnReadyForQueries() {
  ready_for_queries_ = true;
  for (auto& callback : queries_waiting_list_) {
    std::move(callback).Run();
  }
}

}  // namespace ash::platform_keys
