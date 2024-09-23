// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_identity/device_oauth2_token_service.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/device_identity/device_oauth2_token_store.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

struct DeviceOAuth2TokenService::PendingRequest {
  PendingRequest(
      const base::WeakPtr<OAuth2AccessTokenManager::RequestImpl>& request,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2AccessTokenManager::ScopeSet& scopes)
      : request(request),
        client_id(client_id),
        client_secret(client_secret),
        scopes(scopes) {}

  const base::WeakPtr<OAuth2AccessTokenManager::RequestImpl> request;
  const std::string client_id;
  const std::string client_secret;
  const OAuth2AccessTokenManager::ScopeSet scopes;
};

DeviceOAuth2TokenService::DeviceOAuth2TokenService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<DeviceOAuth2TokenStore> store)
    : url_loader_factory_(url_loader_factory),
      state_(STATE_LOADING),
      max_refresh_token_validation_retries_(3),
      validation_requested_(false),
      store_(std::move(store)) {
  token_manager_ = std::make_unique<OAuth2AccessTokenManager>(
      this /* OAuth2AccessTokenManager::Delegate* */);
  store_->SetObserver(this);
  store_->Init(base::BindOnce(&DeviceOAuth2TokenService::OnInitComplete,
                              weak_ptr_factory_.GetWeakPtr()));
}

DeviceOAuth2TokenService::~DeviceOAuth2TokenService() {
  FlushPendingRequests(false, GoogleServiceAuthError::REQUEST_CANCELED);
}

void DeviceOAuth2TokenService::SetAndSaveRefreshToken(
    const std::string& refresh_token,
    StatusCallback result_callback) {
  ReportServiceError(GoogleServiceAuthError::REQUEST_CANCELED);

  state_ = STATE_VALIDATION_PENDING;
  store_->SetAndSaveRefreshToken(refresh_token, std::move(result_callback));
}

CoreAccountId DeviceOAuth2TokenService::GetRobotAccountId() const {
  if (!robot_account_id_for_testing_.empty()) {
    return robot_account_id_for_testing_;
  }

  return store_->GetAccountId();
}

void DeviceOAuth2TokenService::set_robot_account_id_for_testing(
    const CoreAccountId& account_id) {
  robot_account_id_for_testing_ = account_id;
}

void DeviceOAuth2TokenService::SetRefreshTokenAvailableCallback(
    RefreshTokenAvailableCallback callback) {
  on_refresh_token_available_callback_ = std::move(callback);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
DeviceOAuth2TokenService::StartAccessTokenRequest(
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    OAuth2AccessTokenManager::Consumer* consumer) {
  // Note: It is fine to pass an empty account id to |token_manager_| as this
  // will just return a request that will always fail.
  return token_manager_->StartRequest(GetRobotAccountId(), scopes, consumer);
}

void DeviceOAuth2TokenService::InvalidateAccessToken(
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  if (GetRobotAccountId().empty())
    return;

  token_manager_->InvalidateAccessToken(GetRobotAccountId(), scopes,
                                        access_token);
}

bool DeviceOAuth2TokenService::RefreshTokenIsAvailable() const {
  switch (state_) {
    case STATE_NO_TOKEN:
    case STATE_TOKEN_INVALID:
      return false;
    case STATE_LOADING:
    case STATE_VALIDATION_PENDING:
    case STATE_VALIDATION_STARTED:
    case STATE_TOKEN_VALID:
      return !GetRobotAccountId().empty();
  }

  NOTREACHED_IN_MIGRATION() << "Unhandled state " << state_;
  return false;
}

OAuth2AccessTokenManager* DeviceOAuth2TokenService::GetAccessTokenManager() {
  return token_manager_.get();
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void DeviceOAuth2TokenService::SetServiceAccountEmail(
    const std::string& account_email) {
  store_->SetAccountEmail(account_email);
}
#endif

void DeviceOAuth2TokenService::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  gaia_oauth_client_->GetTokenInfo(access_token,
                                   max_refresh_token_validation_retries_, this);
}

void DeviceOAuth2TokenService::OnGetTokenInfoResponse(
    const base::Value::Dict& token_info) {
  // For robot accounts email id is the account id.
  const std::string* robot_email = token_info.FindString("email");
  gaia_oauth_client_.reset();

  store_->PrepareTrustedAccountId(base::BindRepeating(
      &DeviceOAuth2TokenService::OnPrepareTrustedAccountIdFinished,
      weak_ptr_factory_.GetWeakPtr(),
      CoreAccountId::FromRobotEmail(robot_email ? *robot_email : "")));
}

void DeviceOAuth2TokenService::OnOAuthError() {
  gaia_oauth_client_.reset();
  state_ = STATE_TOKEN_INVALID;
  ReportServiceError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
}

void DeviceOAuth2TokenService::OnNetworkError(int response_code) {
  gaia_oauth_client_.reset();

  // Go back to pending validation state. That'll allow a retry on subsequent
  // token minting requests.
  state_ = STATE_VALIDATION_PENDING;
  ReportServiceError(GoogleServiceAuthError::CONNECTION_FAILED);
}

void DeviceOAuth2TokenService::OnInitComplete(bool init_result,
                                              bool validation_required) {
  if (!init_result) {
    state_ = STATE_NO_TOKEN;
    return;
  }

  // If the refresh token was set while waiting for the init, it was already
  // validated and |validation_required| here will be false. In that case, no
  // point validating it again; bail out.
  if (!validation_required)
    return;

  state_ = STATE_VALIDATION_PENDING;

  // If there are pending requests, start a validation.
  if (validation_requested_)
    StartValidation();

  // Announce the token.
  if (!GetRobotAccountId().empty())
    FireRefreshTokenAvailable();
}

void DeviceOAuth2TokenService::OnPrepareTrustedAccountIdFinished(
    const CoreAccountId& gaia_robot_id,
    bool check_passed) {
  if (!check_passed) {
    state_ = STATE_NO_TOKEN;
    ReportServiceError(GoogleServiceAuthError::USER_NOT_SIGNED_UP);
    return;
  }

  CoreAccountId policy_robot_id = GetRobotAccountId();
  if (policy_robot_id == gaia_robot_id) {
    state_ = STATE_TOKEN_VALID;
    ReportServiceError(GoogleServiceAuthError::NONE);
  } else {
    if (gaia_robot_id.empty()) {
      LOG(WARNING) << "Device service account owner in policy is empty.";
    } else {
      LOG(WARNING) << "Device service account owner in policy does not match "
                   << "refresh token owner \"" << gaia_robot_id << "\".";
    }
    state_ = STATE_TOKEN_INVALID;
    ReportServiceError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  }
}

void DeviceOAuth2TokenService::OnRefreshTokenAvailable() {
  FireRefreshTokenAvailable();
}

std::unique_ptr<OAuth2AccessTokenFetcher>
DeviceOAuth2TokenService::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer,
    const std::string& token_binding_challenge) {
  std::string refresh_token = GetRefreshToken();
  DCHECK(!refresh_token.empty());
  return GaiaAccessTokenFetcher::
      CreateExchangeRefreshTokenForAccessTokenInstance(
          consumer, url_loader_factory, refresh_token);
}

bool DeviceOAuth2TokenService::HasRefreshToken(
    const CoreAccountId& account_id) const {
  if (account_id.empty())
    return false;

  if (GetRobotAccountId() != account_id)
    return false;

  return RefreshTokenIsAvailable();
}

scoped_refptr<network::SharedURLLoaderFactory>
DeviceOAuth2TokenService::GetURLLoaderFactory() const {
  return url_loader_factory_;
}

void DeviceOAuth2TokenService::FireRefreshTokenAvailable() {
  if (!on_refresh_token_available_callback_)
    return;

  DCHECK(!GetRobotAccountId().empty());
  on_refresh_token_available_callback_.Run();
}

bool DeviceOAuth2TokenService::HandleAccessTokenFetch(
    OAuth2AccessTokenManager::RequestImpl* request,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2AccessTokenManager::ScopeSet& scopes) {
  if (!HasValidationResult()) {
    // Add a pending request that will be satisfied once validation completes.
    // This must happen before the call to |StartValidation()| because the
    // latter can be synchronous on some platforms.
    pending_requests_.push_back(new PendingRequest(
        request->AsWeakPtr(), client_id, client_secret, scopes));
    RequestValidation();
  }

  switch (state_) {
    case STATE_VALIDATION_PENDING:
      // If this is the first request for a token, start validation.
      StartValidation();
      return true;
    case STATE_LOADING:
    case STATE_VALIDATION_STARTED:
      return true;
    case STATE_NO_TOKEN:
      FailRequest(request, GoogleServiceAuthError::USER_NOT_SIGNED_UP);
      return true;
    case STATE_TOKEN_INVALID:
      FailRequest(request, GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
      return true;
    case STATE_TOKEN_VALID:
      // Let OAuth2AccessTokenManager handle the request.
      return false;
  }

  NOTREACHED_IN_MIGRATION() << "Unexpected state " << state_;
  return false;
}

void DeviceOAuth2TokenService::FlushPendingRequests(
    bool token_is_valid,
    GoogleServiceAuthError::State error) {
  std::vector<raw_ptr<PendingRequest, VectorExperimental>> requests;
  requests.swap(pending_requests_);
  for (std::vector<raw_ptr<PendingRequest, VectorExperimental>>::iterator
           request(requests.begin());
       request != requests.end(); ++request) {
    std::unique_ptr<PendingRequest> scoped_request(*request);
    if (!scoped_request->request)
      continue;

    if (token_is_valid) {
      token_manager_->FetchOAuth2Token(
          scoped_request->request.get(),
          scoped_request->request->GetAccountId(), GetURLLoaderFactory(),
          scoped_request->client_id, scoped_request->client_secret,
          scoped_request->request->GetConsumerId(), scoped_request->scopes);
    } else {
      FailRequest(scoped_request->request.get(), error);
    }
  }
}

void DeviceOAuth2TokenService::FailRequest(
    OAuth2AccessTokenManager::RequestImpl* request,
    GoogleServiceAuthError::State error) {
  GoogleServiceAuthError auth_error =
      (error == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)
          ? GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                    CREDENTIALS_REJECTED_BY_SERVER)
          : GoogleServiceAuthError(error);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&OAuth2AccessTokenManager::RequestImpl::InformConsumer,
                     request->AsWeakPtr(), auth_error,
                     OAuth2AccessTokenConsumer::TokenResponse()));
}

std::string DeviceOAuth2TokenService::GetRefreshToken() const {
  switch (state_) {
    case STATE_LOADING:
    case STATE_NO_TOKEN:
    case STATE_TOKEN_INVALID:
      // This shouldn't happen: GetRefreshToken() is only called for actual
      // token minting operations. In above states, requests are either queued
      // or short-circuited to signal error immediately, so no actual token
      // minting via OAuth2AccessTokenManager::FetchOAuth2Token should be
      // triggered.
      NOTREACHED_IN_MIGRATION();
      return std::string();
    case STATE_VALIDATION_PENDING:
    case STATE_VALIDATION_STARTED:
    case STATE_TOKEN_VALID:
      return store_->GetRefreshToken();
  }

  NOTREACHED_IN_MIGRATION() << "Unhandled state " << state_;
  return std::string();
}

void DeviceOAuth2TokenService::StartValidation() {
  DCHECK_EQ(state_, STATE_VALIDATION_PENDING);
  DCHECK(!gaia_oauth_client_);

  state_ = STATE_VALIDATION_STARTED;

  gaia_oauth_client_ =
      std::make_unique<gaia::GaiaOAuthClient>(url_loader_factory_);

  GaiaUrls* gaia_urls = GaiaUrls::GetInstance();
  gaia::OAuthClientInfo client_info;
  client_info.client_id = gaia_urls->oauth2_chrome_client_id();
  client_info.client_secret = gaia_urls->oauth2_chrome_client_secret();

  gaia_oauth_client_->RefreshToken(
      client_info, store_->GetRefreshToken(),
      std::vector<std::string>(1, GaiaConstants::kGoogleUserInfoEmail),
      max_refresh_token_validation_retries_, this);
}

void DeviceOAuth2TokenService::RequestValidation() {
  validation_requested_ = true;
}

void DeviceOAuth2TokenService::ReportServiceError(
    GoogleServiceAuthError::State error) {
  if (error == GoogleServiceAuthError::NONE)
    FlushPendingRequests(true, GoogleServiceAuthError::NONE);
  else
    FlushPendingRequests(false, error);
}

bool DeviceOAuth2TokenService::HasValidationResult() const {
  return state_ == STATE_NO_TOKEN || state_ == STATE_TOKEN_INVALID ||
         state_ == STATE_TOKEN_VALID;
}
