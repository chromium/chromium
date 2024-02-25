// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_OAUTH_TOKEN_FETCHER_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_OAUTH_TOKEN_FETCHER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

class DeviceOAuth2TokenService;

namespace policy {

// Helper class that asynchronously fetches an OAuth token that can be used to
// start and accept an incoming CRD session.
class CrdOAuthTokenFetcher {
 public:
  using OAuthTokenCallback =
      base::OnceCallback<void(std::optional<std::string>)>;

  virtual ~CrdOAuthTokenFetcher() = default;

  virtual void Start(OAuthTokenCallback done_callback) = 0;
};

// Real implementation of the OAuth token fetcher that will use the
// given `DeviceOAuth2TokenService` to fetch an OAuth token valid for an
// incoming CRD session.
class RealCrdOAuthTokenFetcher : public CrdOAuthTokenFetcher,
                                 public OAuth2AccessTokenManager::Consumer {
 public:
  using OAuthTokenCallback =
      base::OnceCallback<void(std::optional<std::string>)>;

  explicit RealCrdOAuthTokenFetcher(DeviceOAuth2TokenService& oauth_service);
  RealCrdOAuthTokenFetcher(const RealCrdOAuthTokenFetcher&) = delete;
  RealCrdOAuthTokenFetcher& operator=(const RealCrdOAuthTokenFetcher&) = delete;
  ~RealCrdOAuthTokenFetcher() override;

  void Start(OAuthTokenCallback done_callback) override;

 private:
  // `OAuth2AccessTokenManager::Consumer` implementation:
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

// Fake implementation of the OAuth token fetcher that will simply return
// the given `oauth_token`.
// Used during unittesting.
class FakeCrdOAuthTokenFetcher : public CrdOAuthTokenFetcher {
 public:
  explicit FakeCrdOAuthTokenFetcher(std::optional<std::string> oauth_token);
  ~FakeCrdOAuthTokenFetcher() override;

  void Start(OAuthTokenCallback done_callback) override;

 private:
  std::optional<std::string> oauth_token_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_CRD_CRD_OAUTH_TOKEN_FETCHER_H_
