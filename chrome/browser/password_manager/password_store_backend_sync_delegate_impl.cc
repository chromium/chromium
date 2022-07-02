// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_store_backend_sync_delegate_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/driver/sync_service.h"

using password_manager::sync_util::IsPasswordSyncEnabled;

PasswordStoreBackendSyncDelegateImpl::PasswordStoreBackendSyncDelegateImpl(
    Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
}

PasswordStoreBackendSyncDelegateImpl::~PasswordStoreBackendSyncDelegateImpl() =
    default;

bool PasswordStoreBackendSyncDelegateImpl::IsSyncingPasswordsEnabled() {
  DCHECK(SyncServiceFactory::HasSyncService(profile_));
  return IsPasswordSyncEnabled(SyncServiceFactory::GetForProfile(profile_));
}
