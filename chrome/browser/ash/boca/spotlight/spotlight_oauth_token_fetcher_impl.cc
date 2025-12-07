// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/spotlight/spotlight_oauth_token_fetcher_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"

namespace ash::boca {

namespace {

constexpr char kCloudDevicesOAuth2Scope[] =
    "https://www.googleapis.com/auth/clouddevices";
constexpr char kChromotingRemoteSupportOAuth2Scope[] =
    "https://www.googleapis.com/auth/chromoting.remote.support";
constexpr char kTachyonOAuth2Scope[] =
    "https://www.googleapis.com/auth/tachyon";

}  // namespace

SpotlightOAuthTokenFetcherImpl::SpotlightOAuthTokenFetcherImpl(
    DeviceOAuth2TokenService& oauth_service)
    : OAuth2AccessTokenManager::Consumer("boca_spotlight"),
      oauth_service_(oauth_service) {}

SpotlightOAuthTokenFetcherImpl::~SpotlightOAuthTokenFetcherImpl() = default;

void SpotlightOAuthTokenFetcherImpl::Start(OAuthTokenCallback done_callback) {
  VLOG(1) << "[Boca] Fetching OAuth access token";

  done_callback_ = std::move(done_callback);

  OAuth2AccessTokenManager::ScopeSet scopes{
      GaiaConstants::kGoogleUserInfoEmail, kCloudDevicesOAuth2Scope,
      kChromotingRemoteSupportOAuth2Scope, kTachyonOAuth2Scope};
  oauth_request_ = oauth_service_->StartAccessTokenRequest(scopes, this);
}

std::string SpotlightOAuthTokenFetcherImpl::GetDeviceRobotEmail() {
  return oauth_service_->GetRobotAccountId().ToString();
}

void SpotlightOAuthTokenFetcherImpl::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  VLOG(1) << "[Boca] Received OAuth access token";
  oauth_request_.reset();
  std::move(done_callback_).Run(token_response.access_token);
}

void SpotlightOAuthTokenFetcherImpl::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  LOG(WARNING) << "[Boca] Failed to get OAuth access token: "
               << error.ToString();
  oauth_request_.reset();
  std::move(done_callback_).Run(std::nullopt);
}
}  // namespace ash::boca
