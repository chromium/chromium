// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_service.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace ash {

TokenHandleService::TokenHandleService(Profile* profile) : profile_(profile) {
  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
  // We expect identity_manager_ to be non-null, since we declare an explicit
  // dependency in `TokenHandleStoreFactory`.
  CHECK(identity_manager_);
  StartObserving();
}

// `TokenHandleService` listens to LST refreshes, marking the corresponding
// user's token handle as stale.
void TokenHandleService::StartObserving() {
  identity_manager_->AddObserver(this);
}

void TokenHandleService::MaybeFetchForExistingUser(
    const AccountId& account_id) {
  // TODO(emaamari): implement
}

void TokenHandleService::MaybeFetchForNewUser(const AccountId& account_id,
                                              const std::string& access_token) {
  // TODO(emaamari): implement
}

void TokenHandleService::OnRefreshTokensLoaded() {
  // TODO(emaamari): implement
}

void TokenHandleService::Shutdown() {
  identity_manager_->RemoveObserver(this);
  identity_manager_ = nullptr;
  profile_ = nullptr;
}

}  // namespace ash
