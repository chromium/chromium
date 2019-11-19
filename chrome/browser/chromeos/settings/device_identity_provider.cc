// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_identity_provider.h"

#include "base/bind_helpers.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

namespace chromeos {

namespace {

// An implementation of ActiveAccountAccessTokenFetcher that is backed by
// DeviceOAuth2TokenService.
class ActiveAccountAccessTokenFetcherImpl
    : public invalidation::ActiveAccountAccessTokenFetcher,
      OAuth2AccessTokenManager::Consumer {
 public:
  ActiveAccountAccessTokenFetcherImpl(
      const std::string& active_account_id,
      const std::string& oauth_consumer_name,
      DeviceOAuth2TokenService* token_service,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      invalidation::ActiveAccountAccessTokenCallback callback);
  ~ActiveAccountAccessTokenFetcherImpl() override;

 private:
  // OAuth2AccessTokenManager::Consumer implementation.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;
  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

  // Invokes |callback_| with (|access_token|, |error|).
  void HandleTokenRequestCompletion(
      const OAuth2AccessTokenManager::Request* request,
      const GoogleServiceAuthError& error,
      const std::string& access_token);

  invalidation::ActiveAccountAccessTokenCallback callback_;
  std::unique_ptr<OAuth2AccessTokenManager::Request> access_token_request_;

  DISALLOW_COPY_AND_ASSIGN(ActiveAccountAccessTokenFetcherImpl);
};

}  // namespace

ActiveAccountAccessTokenFetcherImpl::ActiveAccountAccessTokenFetcherImpl(
    const std::string& active_account_id,
    const std::string& oauth_consumer_name,
    DeviceOAuth2TokenService* token_service,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    invalidation::ActiveAccountAccessTokenCallback callback)
    : OAuth2AccessTokenManager::Consumer(oauth_consumer_name),
      callback_(std::move(callback)) {
  access_token_request_ =
      token_service->StartAccessTokenRequest(active_account_id, scopes, this);
}

ActiveAccountAccessTokenFetcherImpl::~ActiveAccountAccessTokenFetcherImpl() {}

void ActiveAccountAccessTokenFetcherImpl::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  HandleTokenRequestCompletion(request, GoogleServiceAuthError::AuthErrorNone(),
                               token_response.access_token);
}

void ActiveAccountAccessTokenFetcherImpl::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  HandleTokenRequestCompletion(request, error, std::string());
}

void ActiveAccountAccessTokenFetcherImpl::HandleTokenRequestCompletion(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error,
    const std::string& access_token) {
  DCHECK_EQ(request, access_token_request_.get());
  std::unique_ptr<OAuth2AccessTokenManager::Request> request_deleter(
      std::move(access_token_request_));

  std::move(callback_).Run(error, access_token);
}

DeviceIdentityProvider::DeviceIdentityProvider(
    chromeos::DeviceOAuth2TokenService* token_service)
    : token_service_(token_service) {
  // TODO(blundell): Can |token_service_| ever actually be non-null?
  if (token_service_) {
    token_service->SetRefreshTokenAvailableCallback(
        base::BindRepeating(&DeviceIdentityProvider::OnRefreshTokenAvailable,
                            base::Unretained(this)));
    token_service->SetRefreshTokenRevokedCallback(
        base::BindRepeating(&DeviceIdentityProvider::OnRefreshTokenRevoked,
                            base::Unretained(this)));
  }
}

DeviceIdentityProvider::~DeviceIdentityProvider() {
  // TODO(blundell): Can |token_service_| ever actually be non-null?
  if (token_service_) {
    token_service_->SetRefreshTokenAvailableCallback(base::NullCallback());
    token_service_->SetRefreshTokenRevokedCallback(base::NullCallback());
  }
}

CoreAccountId DeviceIdentityProvider::GetActiveAccountId() {
  return token_service_->GetRobotAccountId();
}

void DeviceIdentityProvider::SetActiveAccountId(
    const CoreAccountId& account_id) {
  // On ChromeOs, the account shouldn't change during runtime, so no need to
  // alert observers here.
  if (!account_id.empty()) {
    auto robot_account_id = token_service_->GetRobotAccountId();
    // When |account_id| and |robot_account_id| mismatch, it means that sync is
    // using a different account than the one that's registered for
    // invalidations. Given that we're in Kiosk mode, sync shouldn't be running
    // anyways. Therefore, this shouldn't be a problem in practice.
    // TODO(crbug.com/919788): Change the sync code to only call this method
    // when sync is actually running.
    LOG_IF(WARNING, account_id != robot_account_id) << "Account ids mismatch.";
  }
  return;
}

bool DeviceIdentityProvider::IsActiveAccountWithRefreshToken() {
  if (GetActiveAccountId().empty() || !token_service_ ||
      !token_service_->RefreshTokenIsAvailable(GetActiveAccountId()))
    return false;

  return true;
}

std::unique_ptr<invalidation::ActiveAccountAccessTokenFetcher>
DeviceIdentityProvider::FetchAccessToken(
    const std::string& oauth_consumer_name,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    invalidation::ActiveAccountAccessTokenCallback callback) {
  return std::make_unique<ActiveAccountAccessTokenFetcherImpl>(
      GetActiveAccountId(), oauth_consumer_name, token_service_, scopes,
      std::move(callback));
}

void DeviceIdentityProvider::InvalidateAccessToken(
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  token_service_->InvalidateAccessToken(GetActiveAccountId(), scopes,
                                        access_token);
}

void DeviceIdentityProvider::OnRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  ProcessRefreshTokenUpdateForAccount(account_id);
}

void DeviceIdentityProvider::OnRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  ProcessRefreshTokenRemovalForAccount(account_id);
}

}  // namespace chromeos
