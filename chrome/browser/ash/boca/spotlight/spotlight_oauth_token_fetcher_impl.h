// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_SPOTLIGHT_SPOTLIGHT_OAUTH_TOKEN_FETCHER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_SPOTLIGHT_SPOTLIGHT_OAUTH_TOKEN_FETCHER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_oauth_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

class DeviceOAuth2TokenService;

namespace ash::boca {

class SpotlightOAuthTokenFetcherImpl
    : public SpotlightOAuthTokenFetcher,
      public OAuth2AccessTokenManager::Consumer {
 public:
  explicit SpotlightOAuthTokenFetcherImpl(
      DeviceOAuth2TokenService& oauth_service);
  ~SpotlightOAuthTokenFetcherImpl() override;

  void Start(OAuthTokenCallback done_callback) override;

  std::string GetDeviceRobotEmail() override;

 private:
  // `OAuth2AccessTokenManager::Consumer` implementation.
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override;

  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override;

  const raw_ref<DeviceOAuth2TokenService> oauth_service_;
  OAuthTokenCallback done_callback_;
  // Handle for the OAuth access token request.
  std::unique_ptr<OAuth2AccessTokenManager::Request> oauth_request_;
};

}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_SPOTLIGHT_SPOTLIGHT_OAUTH_TOKEN_FETCHER_IMPL_H_
