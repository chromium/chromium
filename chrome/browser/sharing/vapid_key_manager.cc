// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/vapid_key_manager.h"

#include "base/feature_list.h"
#include "chrome/browser/sharing/features.h"
#include "chrome/browser/sharing/sharing_metrics.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "components/sync/driver/sync_service.h"
#include "crypto/ec_private_key.h"

VapidKeyManager::VapidKeyManager(SharingSyncPreference* sharing_sync_preference,
                                 syncer::SyncService* sync_service)
    : sharing_sync_preference_(sharing_sync_preference),
      sync_service_(sync_service) {}

VapidKeyManager::~VapidKeyManager() = default;

crypto::ECPrivateKey* VapidKeyManager::GetOrCreateKey() {
  if (!vapid_key_)
    RefreshCachedKey();

  return vapid_key_.get();
}

bool VapidKeyManager::RefreshCachedKey() {
  if (base::FeatureList::IsEnabled(kSharingDeriveVapidKey)) {
    auto derived_key = sync_service_->GetExperimentalAuthenticationKey();
    if (!derived_key)
      return InitWithPreference();

    return UpdateCachedKey(std::move(derived_key));
  } else {
    if (InitWithPreference())
      return true;

    if (vapid_key_)
      return false;

    auto generated_key = crypto::ECPrivateKey::Create();
    if (!generated_key) {
      LogSharingVapidKeyCreationResult(
          SharingVapidKeyCreationResult::kGenerateECKeyFailed);
      return false;
    }

    return UpdateCachedKey(std::move(generated_key));
  }
}

bool VapidKeyManager::UpdateCachedKey(
    std::unique_ptr<crypto::ECPrivateKey> new_key) {
  std::vector<uint8_t> new_key_info;
  if (!new_key->ExportPrivateKey(&new_key_info)) {
    LogSharingVapidKeyCreationResult(
        SharingVapidKeyCreationResult::kExportPrivateKeyFailed);
    return false;
  }

  if (vapid_key_info_ == new_key_info)
    return false;

  vapid_key_ = std::move(new_key);
  vapid_key_info_ = std::move(new_key_info);
  sharing_sync_preference_->SetVapidKey(vapid_key_info_);
  LogSharingVapidKeyCreationResult(SharingVapidKeyCreationResult::kSuccess);
  return true;
}

bool VapidKeyManager::InitWithPreference() {
  base::Optional<std::vector<uint8_t>> preference_key_info =
      sharing_sync_preference_->GetVapidKey();
  if (!preference_key_info || vapid_key_info_ == *preference_key_info)
    return false;

  vapid_key_ =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(*preference_key_info);
  vapid_key_info_ = std::move(*preference_key_info);
  return true;
}
