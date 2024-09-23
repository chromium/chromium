// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_sync_token_verifier.h"

#include <memory>
#include <string>

#include "ash/public/cpp/reauth_reason.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager.h"
#include "chrome/browser/ash/login/lock/online_reauth/lock_screen_reauth_manager_factory.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "net/base/backoff_entry.h"

namespace ash {
namespace {

const char fake_token[] = "fake-token";
}

const net::BackoffEntry::Policy
    PasswordSyncTokenVerifier::kFetchTokenRetryBackoffPolicy = {
        0,              // Number of initial errors to ignore.
        5 * 60 * 1000,  // Initial request delay in ms.
        2.0,            // Factor by which the waiting time will be multiplied.
        0.1,            // Fuzzing percentage.
        6 * 60 * 60 * 1000,  // Maximum request delay in ms.
        -1,                  // Never discard the entry.
        true,  // Don't use initial delay unless last request was an error.
};

PasswordSyncTokenVerifier::PasswordSyncTokenVerifier(Profile* primary_profile)
    : primary_profile_(primary_profile),
      primary_user_(ProfileHelper::Get()->GetUserByProfile(primary_profile)),
      retry_backoff_(&kFetchTokenRetryBackoffPolicy) {
  DCHECK(primary_profile_);
  DCHECK(primary_user_);
}

PasswordSyncTokenVerifier::~PasswordSyncTokenVerifier() = default;

void PasswordSyncTokenVerifier::RecheckAfter(base::TimeDelta delay) {
  CancelPendingChecks();
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasswordSyncTokenVerifier::CheckForPasswordNotInSync,
                     weak_ptr_factory_.GetWeakPtr()),
      delay);
}

void PasswordSyncTokenVerifier::CreateTokenAsync() {
  DCHECK(!password_sync_token_fetcher_);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      primary_profile_->GetURLLoaderFactory();
  if (!url_loader_factory.get()) {
    return;
  }

  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      url_loader_factory, primary_profile_, this);
  password_sync_token_fetcher_->StartTokenCreate();
}

void PasswordSyncTokenVerifier::CheckForPasswordNotInSync() {
  // In-session password change is as of now the only way to trigger the sync
  // token update. We do not need to poll if this feature is not enabled.
  PrefService* prefs = primary_profile_->GetPrefs();
  if (!prefs->GetBoolean(prefs::kSamlInSessionPasswordChangeEnabled)) {
    return;
  }
  DCHECK(!password_sync_token_fetcher_);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      primary_profile_->GetURLLoaderFactory();
  // url_loader_factory is nullptr in unit tests so constructing
  // PasswordSyncTokenFetcher does not make sense there.
  if (!url_loader_factory.get()) {
    return;
  }
  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      url_loader_factory, primary_profile_, this);

  // Get current sync token for primary_user_.
  std::string token_to_verify = fake_token;
  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string* sync_token =
      known_user.GetPasswordSyncToken(primary_user_->GetAccountId());
  // Local copy of the token exists on the device and will be used for
  // verification. Otherwise we will create it by sending user through the
  // online re-auth.
  if (sync_token && !sync_token->empty()) {
    token_to_verify = *sync_token;
  }

  password_sync_token_fetcher_->StartTokenVerify(token_to_verify);
}

void PasswordSyncTokenVerifier::FetchSyncTokenOnReauth() {
  PrefService* prefs = primary_profile_->GetPrefs();
  if (!prefs->GetBoolean(prefs::kSamlInSessionPasswordChangeEnabled)) {
    return;
  }

  DCHECK(!password_sync_token_fetcher_);
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      primary_profile_->GetURLLoaderFactory();
  // No url_loader_factory in unit tests.
  if (!url_loader_factory.get()) {
    return;
  }

  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      url_loader_factory, primary_profile_, this);
  password_sync_token_fetcher_->StartTokenGet();
}

void PasswordSyncTokenVerifier::CancelPendingChecks() {
  // We should not have any active request at this point. DCHECK makes sure it
  // is really the case for the dev build. In a release build InvalidateWeakPtrs
  // helps to recover by cancelling potential existing requests.
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void PasswordSyncTokenVerifier::RecordTokenPollingStart() {
  RecordStartOfSyncTokenPollingUMA(/*in_session=*/true);
}

void PasswordSyncTokenVerifier::OnTokenCreated(const std::string& sync_token) {
  DCHECK(!sync_token.empty());

  // Set token value in local state.
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetPasswordSyncToken(primary_user_->GetAccountId(), sync_token);
  password_sync_token_fetcher_.reset();
  RecordTokenPollingStart();
  RecheckAfter(retry_backoff_.GetTimeUntilRelease());
}

void PasswordSyncTokenVerifier::OnTokenFetched(const std::string& sync_token) {
  password_sync_token_fetcher_.reset();
  if (!sync_token.empty()) {
    // Set token fetched from the endpoint in local state.
    user_manager::KnownUser known_user(g_browser_process->local_state());
    known_user.SetPasswordSyncToken(primary_user_->GetAccountId(), sync_token);
    RecordTokenPollingStart();
    RecheckAfter(retry_backoff_.GetTimeUntilRelease());
  } else {
    // This is the first time a sync token is created for the user: we need to
    // initialize its value by calling the API and store it locally.
    CreateTokenAsync();
  }
}

void PasswordSyncTokenVerifier::OnTokenVerified(bool is_valid) {
  retry_backoff_.InformOfRequest(true);
  password_sync_token_fetcher_.reset();
  // Schedule next token check after base interval.
  RecheckAfter(retry_backoff_.GetTimeUntilRelease());
  if (is_valid) {
    return;
  }

  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      primary_user_->GetAccountId(), true);
  // Re-auth on lock.
  LockScreenReauthManager* lock_screen_reauth_manager =
      LockScreenReauthManagerFactory::GetForProfile(primary_profile_);
  if (lock_screen_reauth_manager) {
    lock_screen_reauth_manager->MaybeForceReauthOnLockScreen(
        ReauthReason::kSamlPasswordSyncTokenValidationFailed);
  }
}

void PasswordSyncTokenVerifier::OnApiCallFailed(
    PasswordSyncTokenFetcher::ErrorType error_type) {
  retry_backoff_.InformOfRequest(false);
  password_sync_token_fetcher_.reset();
  if (error_type == PasswordSyncTokenFetcher::ErrorType::kGetNoList ||
      error_type == PasswordSyncTokenFetcher::ErrorType::kGetNoToken) {
    // Token sync API has not been initialized yet. Create a sync token.
    CreateTokenAsync();
  } else {
    // Schedule next token check with interval calculated with exponential
    // backoff.
    RecheckAfter(retry_backoff_.GetTimeUntilRelease());
  }
}

}  // namespace ash
