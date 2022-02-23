// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_backend_sync_delegate_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

PasswordStoreBackendSyncDelegateImpl::PasswordStoreBackendSyncDelegateImpl(
    Profile* profile)
    : profile_(profile) {}

PasswordStoreBackendSyncDelegateImpl::~PasswordStoreBackendSyncDelegateImpl() =
    default;

bool PasswordStoreBackendSyncDelegateImpl::IsSyncingPasswordsEnabled() {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  return sync_service && sync_service->IsSyncFeatureEnabled() &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPasswords);
}

absl::optional<std::string>
PasswordStoreBackendSyncDelegateImpl::GetSyncingAccount() {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (!sync_service)
    return absl::nullopt;
  return sync_service->GetAccountInfo().email;
}
