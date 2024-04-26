// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager_factory.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"

namespace ash {

InSessionPasswordSyncManager::InSessionPasswordSyncManager(
    Profile* primary_profile)
    : primary_profile_(primary_profile),
      primary_user_(ProfileHelper::Get()->GetUserByProfile(primary_profile)) {
  DCHECK(primary_user_);
}

InSessionPasswordSyncManager::~InSessionPasswordSyncManager() = default;

void InSessionPasswordSyncManager::CreateTokenAsync() {
  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      primary_profile_->GetURLLoaderFactory(), primary_profile_, this);
  password_sync_token_fetcher_->StartTokenCreate();
}

void InSessionPasswordSyncManager::OnTokenCreated(const std::string& token) {
  password_sync_token_fetcher_.reset();

  // Set token value in local state.
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetPasswordSyncToken(primary_user_->GetAccountId(), token);
  ResetReauthRequiredBySamlTokenDismatch();
}

void InSessionPasswordSyncManager::FetchTokenAsync() {
  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      primary_profile_->GetURLLoaderFactory(), primary_profile_, this);
  password_sync_token_fetcher_->StartTokenGet();
}

void InSessionPasswordSyncManager::OnTokenFetched(const std::string& token) {
  password_sync_token_fetcher_.reset();
  if (!token.empty()) {
    // Set token fetched from the endpoint in local state.
    user_manager::KnownUser known_user(g_browser_process->local_state());
    known_user.SetPasswordSyncToken(primary_user_->GetAccountId(), token);
    ResetReauthRequiredBySamlTokenDismatch();
  } else {
    // This is the first time a sync token is created for the user: we need to
    // initialize its value by calling the API and store it locally.
    CreateTokenAsync();
  }
}

void InSessionPasswordSyncManager::OnTokenVerified(bool is_valid) {
  // InSessionPasswordSyncManager does not verify the sync token.
}

void InSessionPasswordSyncManager::OnApiCallFailed(
    PasswordSyncTokenFetcher::ErrorType error_type) {
  // If error_type == kGetNoList || kGetNoToken the token API is not
  // initialized yet and we can fix it by creating a new token on lock
  // screen re-authentication.
  // All other API errors will be ignored since they are logged by
  // TokenFetcher and will be re-tried.
  password_sync_token_fetcher_.reset();
  if (error_type == PasswordSyncTokenFetcher::ErrorType::kGetNoList ||
      error_type == PasswordSyncTokenFetcher::ErrorType::kGetNoToken) {
    CreateTokenAsync();
  }
}

void InSessionPasswordSyncManager::ResetReauthRequiredBySamlTokenDismatch() {
  LockScreenReauthManager& lock_screen_reauth_manager = CHECK_DEREF(
      LockScreenReauthManagerFactory::GetForProfile(primary_profile_));
  lock_screen_reauth_manager.ResetReauthRequiredBySamlTokenDismatch();
}
}  // namespace ash
