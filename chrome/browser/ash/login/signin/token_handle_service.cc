// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_service.h"

#include <string>

#include "chrome/browser/ash/login/signin/token_handle_store_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "components/account_id/account_id.h"
#include "components/account_manager_core/chromeos/account_manager.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/user_manager/user.h"
#include "google_apis/gaia/gaia_constants.h"

// Enable VLOG level 1.
// TODO(b/387248794): Remove after stabilizing, along with associated log
// statements.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {
namespace {

account_manager::AccountManager* GetAccountManager(Profile* profile) {
  return g_browser_process->platform_part()
      ->GetAccountManagerFactory()
      ->GetAccountManager(profile->GetPath().value());
}

}  // namespace

TokenHandleService::TokenHandleService(Profile* profile,
                                       TokenHandleStore* token_handle_store)
    : profile_(profile), token_handle_store_(token_handle_store) {
  identity_manager_ = IdentityManagerFactory::GetForProfile(profile_);
  // We expect identity_manager_ to be non-null, since we declare an explicit
  // dependency in `TokenHandleStoreFactory`.
  CHECK(identity_manager_);

  StartObserving();

  // It's possible we missed the notification, check now.
  if (identity_manager_->AreRefreshTokensLoaded()) {
    OnRefreshTokensLoaded();
  }
}

TokenHandleService::~TokenHandleService() = default;

// `TokenHandleService` listens to LST refreshes, marking the corresponding
// user's token handle as stale.
void TokenHandleService::StartObserving() {
  identity_manager_->AddObserver(this);
}

void TokenHandleService::MaybeFetchForExistingUser(
    const AccountId& account_id) {
  VLOG(1) << "TokenHandleService::MaybeFetchForExistingUser";
  if (account_id.GetAccountType() != AccountType::GOOGLE) {
    return;
  }
  FetchAccessToken(account_id);
}

void TokenHandleService::MaybeFetchForNewUser(
    const AccountId& account_id,
    const std::string& access_token,
    const std::string& refresh_token_hash) {
  VLOG(1) << "TokenHandleService::MaybeFetchForNewUser";
  MaybeFetchTokenHandle(account_id, access_token, refresh_token_hash);
}

void TokenHandleService::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {
  const AccountId& account_id =
      ProfileHelper::Get()->GetUserByProfile(profile_)->GetAccountId();

  if (account_info.email == account_id.GetUserEmail()) {
    token_handle_store_->SetTokenHandleStale(account_id);
  }
}

void TokenHandleService::FetchAccessToken(const AccountId& account_id) {
  VLOG(1) << "TokenHandleService::FetchAccessToken";
  if (signin::IdentityManager* const identity_manager =
          IdentityManagerFactory::GetForProfile(profile_);
      identity_manager) {
    access_token_fetcher_ =
        std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
            signin::OAuthConsumerId::kTokenHandleService, identity_manager,
            base::BindOnce(&TokenHandleService::OnAccessTokenFetchComplete,
                           weak_factory_.GetWeakPtr(), account_id),
            signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
            signin::ConsentLevel::kSignin);
  }
}

void TokenHandleService::OnAccessTokenFetchComplete(
    const AccountId& account_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  VLOG(1) << "TokenHandleService::OnAccessTokenFetchComplete";
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Could not get access token for fetching a new token handle: "
               << error.ToString();
    return;
  }

  GetRefreshTokenHash(account_id, token_info.token);
}

void TokenHandleService::GetRefreshTokenHash(const AccountId& account_id,
                                             const std::string& access_token) {
  VLOG(1) << "TokenHandleService::GetRefreshTokenHash";
  GetAccountManager(profile_)->GetTokenHash(
      account_manager::AccountKey::FromGaiaId(account_id.GetGaiaId()),
      base::BindOnce(&TokenHandleService::MaybeFetchTokenHandle,
                     weak_factory_.GetWeakPtr(), account_id, access_token));
}

void TokenHandleService::MaybeFetchTokenHandle(
    const AccountId account_id,
    const std::string& access_token,
    const std::string& refresh_token_hash) {
  VLOG(1) << "TokenHandleService::OnGetRefreshTokenHash";
  token_handle_store_->MaybeFetchTokenHandle(
      profile_->GetPrefs(), profile_->GetURLLoaderFactory(), account_id,
      access_token, refresh_token_hash);
}

void TokenHandleService::Shutdown() {
  identity_manager_->RemoveObserver(this);
  identity_manager_ = nullptr;
  profile_ = nullptr;
}

}  // namespace ash
