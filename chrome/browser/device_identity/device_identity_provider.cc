// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/device_identity_provider.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"

namespace {

// An implementation of ActiveAccountAccessTokenFetcher that is backed by
// DeviceOAuth2TokenService.
class ActiveAccountAccessTokenFetcherImpl
    : public invalidation::ActiveAccountAccessTokenFetcher,
      OAuth2AccessTokenManager::Consumer {
 public:
  ActiveAccountAccessTokenFetcherImpl(
      const std::string& oauth_consumer_name,
      DeviceOAuth2TokenService* token_service,
      const OAuth2AccessTokenManager::ScopeSet& scopes,
      invalidation::ActiveAccountAccessTokenCallback callback);

  ActiveAccountAccessTokenFetcherImpl(
      const ActiveAccountAccessTokenFetcherImpl&) = delete;
  ActiveAccountAccessTokenFetcherImpl& operator=(
      const ActiveAccountAccessTokenFetcherImpl&) = delete;

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
};

}  // namespace

ActiveAccountAccessTokenFetcherImpl::ActiveAccountAccessTokenFetcherImpl(
    const std::string& oauth_consumer_name,
    DeviceOAuth2TokenService* token_service,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    invalidation::ActiveAccountAccessTokenCallback callback)
    : OAuth2AccessTokenManager::Consumer(oauth_consumer_name),
      callback_(std::move(callback)) {
  access_token_request_ = token_service->StartAccessTokenRequest(scopes, this);
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
    DeviceOAuth2TokenService* token_service)
    : token_service_(token_service) {
  // TODO(blundell): Can |token_service_| ever actually be non-null?
  if (token_service_) {
    token_service->SetRefreshTokenAvailableCallback(
        base::BindRepeating(&DeviceIdentityProvider::OnRefreshTokenAvailable,
                            base::Unretained(this)));
  }
}

DeviceIdentityProvider::~DeviceIdentityProvider() {
  // TODO(blundell): Can |token_service_| ever actually be non-null?
  if (token_service_) {
    token_service_->SetRefreshTokenAvailableCallback(base::NullCallback());
  }
}

CoreAccountId DeviceIdentityProvider::GetActiveAccountId() {
  return token_service_->GetRobotAccountId();
}

bool DeviceIdentityProvider::IsActiveAccountWithRefreshToken() {
  if (GetActiveAccountId().empty() || !token_service_ ||
      !token_service_->RefreshTokenIsAvailable())
    return false;

  return true;
}

std::unique_ptr<invalidation::ActiveAccountAccessTokenFetcher>
DeviceIdentityProvider::FetchAccessToken(
    const std::string& oauth_consumer_name,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    invalidation::ActiveAccountAccessTokenCallback callback) {
  return std::make_unique<ActiveAccountAccessTokenFetcherImpl>(
      oauth_consumer_name, token_service_, scopes, std::move(callback));
}

void DeviceIdentityProvider::InvalidateAccessToken(
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  token_service_->InvalidateAccessToken(scopes, access_token);
}

void DeviceIdentityProvider::OnRefreshTokenAvailable() {
  ProcessRefreshTokenUpdateForAccount(GetActiveAccountId());
}
