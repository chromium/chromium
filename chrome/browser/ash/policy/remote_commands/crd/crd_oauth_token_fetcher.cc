// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/crd/crd_oauth_token_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "chrome/browser/ash/policy/remote_commands/crd/crd_logging.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace policy {

namespace {

constexpr char kCloudDevicesOAuth2Scope[] =
    "https://www.googleapis.com/auth/clouddevices";
constexpr char kChromotingRemoteSupportOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromoting.remote.support";
constexpr char kTachyonOAuth2Scope[] =
    "https://www.googleapis.com/auth/tachyon";

}  // namespace

RealCrdOAuthTokenFetcher::RealCrdOAuthTokenFetcher(
    DeviceOAuth2TokenService& oauth_service)
    : OAuth2AccessTokenManager::Consumer("crd_remote_command"),
      oauth_service_(oauth_service) {}

RealCrdOAuthTokenFetcher::~RealCrdOAuthTokenFetcher() = default;

void RealCrdOAuthTokenFetcher::Start(OAuthTokenCallback done_callback) {
  CRD_VLOG(1) << "Fetching OAuth access token";

  done_callback_ = std::move(done_callback);

  OAuth2AccessTokenManager::ScopeSet scopes{
      GaiaConstants::kGoogleUserInfoEmail, kCloudDevicesOAuth2Scope,
      kChromotingRemoteSupportOAuth2Scope, kTachyonOAuth2Scope};
  oauth_request_ = oauth_service_->StartAccessTokenRequest(scopes, this);
}

void RealCrdOAuthTokenFetcher::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  CRD_VLOG(1) << "Received OAuth access token";
  oauth_request_.reset();
  std::move(done_callback_).Run(token_response.access_token);
}

void RealCrdOAuthTokenFetcher::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  CRD_LOG(WARNING) << "Failed to get OAuth access token: " << error.ToString();
  oauth_request_.reset();
  std::move(done_callback_).Run(std::nullopt);
}

FakeCrdOAuthTokenFetcher::FakeCrdOAuthTokenFetcher(
    std::optional<std::string> oauth_token)
    : oauth_token_(std::move(oauth_token)) {}

FakeCrdOAuthTokenFetcher::~FakeCrdOAuthTokenFetcher() = default;

void FakeCrdOAuthTokenFetcher::Start(OAuthTokenCallback done_callback) {
  std::move(done_callback).Run(oauth_token_);
}

}  // namespace policy
