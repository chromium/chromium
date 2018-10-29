// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/oauth2_token_service_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/account_mapper_util.h"
#include "chromeos/account_manager/account_manager.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "content/public/browser/network_service_instance.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_immediate_error.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

namespace {

// Values used from |MutableProfileOAuth2TokenServiceDelegate|.
const net::BackoffEntry::Policy kBackoffPolicy = {
    0 /* int num_errors_to_ignore */,

    1000 /* int initial_delay_ms */,

    2.0 /* double multiply_factor */,

    0.2 /* double jitter_factor */,

    15 * 60 * 1000 /* int64_t maximum_backoff_ms */,

    -1 /* int64_t entry_lifetime_ms */,

    false /* bool always_use_initial_delay */,
};

}  // namespace

class ChromeOSOAuth2TokenServiceDelegate::AccountErrorStatus
    : public SigninErrorController::AuthStatusProvider {
 public:
  AccountErrorStatus(SigninErrorController* signin_error_controller,
                     const std::string& account_id)
      : signin_error_controller_(signin_error_controller),
        account_id_(account_id) {
    signin_error_controller_->AddProvider(this);
  }

  ~AccountErrorStatus() override {
    signin_error_controller_->RemoveProvider(this);
  }

  // SigninErrorController::AuthStatusProvider overrides.
  std::string GetAccountId() const override { return account_id_; }

  GoogleServiceAuthError GetAuthStatus() const override {
    return last_auth_error_;
  }

  void SetLastAuthError(const GoogleServiceAuthError& error) {
    last_auth_error_ = error;
    signin_error_controller_->AuthStatusChanged();
  }

 private:
  // A non-owning pointer to |SigninErrorController|.
  SigninErrorController* const signin_error_controller_;

  // The account id being tracked by |this| instance of |AccountErrorStatus|.
  const std::string account_id_;

  // The last auth error seen for |account_id_|.
  GoogleServiceAuthError last_auth_error_;

  DISALLOW_COPY_AND_ASSIGN(AccountErrorStatus);
};

ChromeOSOAuth2TokenServiceDelegate::ChromeOSOAuth2TokenServiceDelegate(
    AccountTrackerService* account_tracker_service,
    chromeos::AccountManager* account_manager,
    SigninErrorController* signin_error_controller)
    : account_mapper_util_(
          std::make_unique<AccountMapperUtil>(account_tracker_service)),
      account_manager_(account_manager),
      signin_error_controller_(signin_error_controller),
      backoff_entry_(&kBackoffPolicy),
      backoff_error_(GoogleServiceAuthError::NONE),
      weak_factory_(this) {
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
}

ChromeOSOAuth2TokenServiceDelegate::~ChromeOSOAuth2TokenServiceDelegate() {
  account_manager_->RemoveObserver(this);
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

OAuth2AccessTokenFetcher*
ChromeOSOAuth2TokenServiceDelegate::CreateAccessTokenFetcher(
    const std::string& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS, load_credentials_state());

  ValidateAccountId(account_id);

  // Check if we need to reject the request.
  // We will reject the request if we are facing a persistent error for this
  // account.
  auto it = errors_.find(account_id);
  if (it != errors_.end() && it->second->GetAuthStatus().IsPersistentError()) {
    VLOG(1) << "Request for token has been rejected due to persistent error #"
            << it->second->GetAuthStatus().state();
    // |OAuth2TokenService| will manage the lifetime of this pointer.
    return new OAuth2AccessTokenFetcherImmediateError(
        consumer, it->second->GetAuthStatus());
  }
  // Or when we need to backoff.
  if (backoff_entry_.ShouldRejectRequest()) {
    VLOG(1) << "Request for token has been rejected due to backoff rules from"
            << " previous error #" << backoff_error_.state();
    // |OAuth2TokenService| will manage the lifetime of this pointer.
    return new OAuth2AccessTokenFetcherImmediateError(consumer, backoff_error_);
  }

  const AccountManager::AccountKey& account_key =
      account_mapper_util_->OAuthAccountIdToAccountKey(account_id);

  // |OAuth2TokenService| will manage the lifetime of the released pointer.
  return account_manager_
      ->CreateAccessTokenFetcher(account_key, url_loader_factory, consumer)
      .release();
}

bool ChromeOSOAuth2TokenServiceDelegate::RefreshTokenIsAvailable(
    const std::string& account_id) const {
  if (load_credentials_state() != LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS) {
    return false;
  }

  return account_manager_->IsTokenAvailable(
      account_mapper_util_->OAuthAccountIdToAccountKey(account_id));
}

void ChromeOSOAuth2TokenServiceDelegate::UpdateAuthError(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  backoff_entry_.InformOfRequest(!error.IsTransientError());
  ValidateAccountId(account_id);
  if (error.IsTransientError()) {
    backoff_error_ = error;
    return;
  }

  auto it = errors_.find(account_id);
  if (error.state() == GoogleServiceAuthError::NONE) {
    // If the error status is NONE, and we were not tracking any error anyways,
    // just ignore the update.
    // Otherwise, delete the error tracking for this account.
    if (it != errors_.end()) {
      errors_.erase(it);
      FireAuthErrorChanged(account_id, error);
    }
  } else if ((it == errors_.end()) || (it->second->GetAuthStatus() != error)) {
    // Error status is not NONE. We need to start tracking the account / update
    // the last seen error, if it is different.
    if (it == errors_.end()) {
      errors_.emplace(account_id, std::make_unique<AccountErrorStatus>(
                                      signin_error_controller_, account_id));
    }
    errors_[account_id]->SetLastAuthError(error);
    FireAuthErrorChanged(account_id, error);
  }
}

GoogleServiceAuthError ChromeOSOAuth2TokenServiceDelegate::GetAuthError(
    const std::string& account_id) const {
  auto it = errors_.find(account_id);
  if (it != errors_.end()) {
    return it->second->GetAuthStatus();
  }

  return GoogleServiceAuthError::AuthErrorNone();
}

std::vector<std::string> ChromeOSOAuth2TokenServiceDelegate::GetAccounts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS, load_credentials_state());

  std::vector<std::string> accounts;
  for (auto& account_key : account_keys_) {
    std::string account_id =
        account_mapper_util_->AccountKeyToOAuthAccountId(account_key);
    if (!account_id.empty()) {
      accounts.emplace_back(account_id);
    }
  }

  return accounts;
}

void ChromeOSOAuth2TokenServiceDelegate::LoadCredentials(
    const std::string& primary_account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (load_credentials_state() != LOAD_CREDENTIALS_NOT_STARTED) {
    return;
  }

  set_load_credentials_state(LOAD_CREDENTIALS_IN_PROGRESS);

  DCHECK(account_manager_);
  account_manager_->AddObserver(this);
  account_manager_->GetAccounts(
      base::BindOnce(&ChromeOSOAuth2TokenServiceDelegate::GetAccountsCallback,
                     weak_factory_.GetWeakPtr()));
}

void ChromeOSOAuth2TokenServiceDelegate::UpdateCredentials(
    const std::string& account_id,
    const std::string& refresh_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS, load_credentials_state());
  DCHECK(!account_id.empty());
  DCHECK(!refresh_token.empty());

  ValidateAccountId(account_id);

  const AccountManager::AccountKey& account_key =
      account_mapper_util_->OAuthAccountIdToAccountKey(account_id);

  // Will result in AccountManager calling
  // |ChromeOSOAuth2TokenServiceDelegate::OnTokenUpserted|.
  account_manager_->UpsertToken(account_key, refresh_token);
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeOSOAuth2TokenServiceDelegate::GetURLLoaderFactory() const {
  return account_manager_->GetUrlLoaderFactory();
}

void ChromeOSOAuth2TokenServiceDelegate::GetAccountsCallback(
    std::vector<AccountManager::AccountKey> account_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // This callback should only be triggered during |LoadCredentials|, which
  // implies that |load_credentials_state())| should in
  // |LOAD_CREDENTIALS_IN_PROGRESS| state.
  DCHECK_EQ(LOAD_CREDENTIALS_IN_PROGRESS, load_credentials_state());

  set_load_credentials_state(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS);

  // The typical order of |OAuth2TokenService::Observer| callbacks is:
  // 1. OnStartBatchChanges
  // 2. OnRefreshTokenAvailable
  // 3. OnEndBatchChanges
  // 4. OnRefreshTokensLoaded
  {
    ScopedBatchChange batch(this);
    for (const auto& account_key : account_keys) {
      OnTokenUpserted(account_key);
    }
  }
  FireRefreshTokensLoaded();
}

void ChromeOSOAuth2TokenServiceDelegate::OnTokenUpserted(
    const AccountManager::AccountKey& account_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  account_keys_.insert(account_key);

  std::string account_id =
      account_mapper_util_->AccountKeyToOAuthAccountId(account_key);
  if (account_id.empty()) {
    return;
  }

  ScopedBatchChange batch(this);
  FireRefreshTokenAvailable(account_id);

  // We cannot directly use |UpdateAuthError| because it does not invoke
  // |FireAuthErrorChanged| if |account_id|'s error state was already
  // |GoogleServiceAuthError::State::NONE|, but |FireAuthErrorChanged| must be
  // invoked here, regardless. See the comment below.
  errors_.erase(account_id);
  // See |OAuth2TokenService::Observer::OnAuthErrorChanged|.
  // |OnAuthErrorChanged| must be always called after
  // |OnRefreshTokenAvailable|, when refresh token is updated.
  FireAuthErrorChanged(account_id, GoogleServiceAuthError::AuthErrorNone());
}

void ChromeOSOAuth2TokenServiceDelegate::OnAccountRemoved(
    const AccountManager::AccountKey& account_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(LOAD_CREDENTIALS_FINISHED_WITH_SUCCESS, load_credentials_state());

  auto it = account_keys_.find(account_key);
  if (it == account_keys_.end()) {
    return;
  }

  account_keys_.erase(it);
  std::string account_id =
      account_mapper_util_->AccountKeyToOAuthAccountId(account_key);
  if (account_id.empty()) {
    return;
  }

  ScopedBatchChange batch(this);

  // ProfileOAuth2TokenService will clear its cache for |account_id| when this
  // is called. See |ProfileOAuth2TokenService::OnRefreshTokenRevoked|.
  FireRefreshTokenRevoked(account_id);
}

void ChromeOSOAuth2TokenServiceDelegate::RevokeCredentials(
    const std::string& account_id) {
  // Signing out of Chrome is not possible on Chrome OS.
  NOTREACHED();
}

void ChromeOSOAuth2TokenServiceDelegate::RevokeAllCredentials() {
  // Signing out of Chrome is not possible on Chrome OS.
  NOTREACHED();
}

const net::BackoffEntry* ChromeOSOAuth2TokenServiceDelegate::BackoffEntry()
    const {
  return &backoff_entry_;
}

void ChromeOSOAuth2TokenServiceDelegate::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  backoff_entry_.Reset();
}

}  // namespace chromeos
