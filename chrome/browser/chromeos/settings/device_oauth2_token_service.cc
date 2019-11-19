// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/settings/token_encryptor.h"
#include "chrome/common/pref_names.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "google_apis/gaia/oauth2_access_token_fetcher_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromeos {

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
    PrefService* local_state)
    : url_loader_factory_(url_loader_factory),
      local_state_(local_state),
      state_(STATE_LOADING),
      max_refresh_token_validation_retries_(3),
      validation_requested_(false),
      service_account_identity_subscription_(
          CrosSettings::Get()->AddSettingsObserver(
              kServiceAccountIdentity,
              base::Bind(
                  &DeviceOAuth2TokenService::OnServiceAccountIdentityChanged,
                  base::Unretained(this)))) {
  token_manager_ = std::make_unique<OAuth2AccessTokenManager>(
      this /* OAuth2AccessTokenManager::Delegate* */);
  // Pull in the system salt.
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&DeviceOAuth2TokenService::DidGetSystemSalt,
                 weak_ptr_factory_.GetWeakPtr()));
}

DeviceOAuth2TokenService::~DeviceOAuth2TokenService() {
  FlushTokenSaveCallbacks(false);
  FlushPendingRequests(false, GoogleServiceAuthError::REQUEST_CANCELED);
}

// static
void DeviceOAuth2TokenService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceRobotAnyApiRefreshToken,
                               std::string());
}

void DeviceOAuth2TokenService::SetAndSaveRefreshToken(
    const std::string& refresh_token,
    const StatusCallback& result_callback) {
  ReportServiceError(GoogleServiceAuthError::REQUEST_CANCELED);

  bool waiting_for_salt = state_ == STATE_LOADING;
  refresh_token_ = refresh_token;
  state_ = STATE_VALIDATION_PENDING;

  // If the robot account ID is not available yet, do not announce the token. It
  // will be done from OnServiceAccountIdentityChanged() once the robot account
  // ID becomes available as well.
  if (!GetRobotAccountId().empty())
    FireRefreshTokenAvailable(GetRobotAccountId());

  token_save_callbacks_.push_back(result_callback);
  if (!waiting_for_salt) {
    if (system_salt_.empty())
      FlushTokenSaveCallbacks(false);
    else
      EncryptAndSaveToken();
  }
}

CoreAccountId DeviceOAuth2TokenService::GetRobotAccountId() const {
  if (!robot_account_id_for_testing_.empty()) {
    return robot_account_id_for_testing_;
  }

  std::string account_id;
  CrosSettings::Get()->GetString(kServiceAccountIdentity, &account_id);
  return CoreAccountId(account_id);
}

void DeviceOAuth2TokenService::set_robot_account_id_for_testing(
    const CoreAccountId& account_id) {
  robot_account_id_for_testing_ = account_id;
}

void DeviceOAuth2TokenService::SetRefreshTokenAvailableCallback(
    RefreshTokenAvailableCallback callback) {
  on_refresh_token_available_callback_ = std::move(callback);
}

void DeviceOAuth2TokenService::SetRefreshTokenRevokedCallback(
    RefreshTokenRevokedCallback callback) {
  on_refresh_token_revoked_callback_ = std::move(callback);
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
DeviceOAuth2TokenService::StartAccessTokenRequest(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    OAuth2AccessTokenManager::Consumer* consumer) {
  return token_manager_->StartRequest(account_id, scopes, consumer);
}

void DeviceOAuth2TokenService::InvalidateAccessToken(
    const CoreAccountId& account_id,
    const OAuth2AccessTokenManager::ScopeSet& scopes,
    const std::string& access_token) {
  token_manager_->InvalidateAccessToken(account_id, scopes, access_token);
}

bool DeviceOAuth2TokenService::RefreshTokenIsAvailable(
    const CoreAccountId& account_id) const {
  auto accounts = GetAccounts();
  return std::find(accounts.begin(), accounts.end(), account_id) !=
         accounts.end();
}

OAuth2AccessTokenManager* DeviceOAuth2TokenService::GetAccessTokenManager() {
  return token_manager_.get();
}

void DeviceOAuth2TokenService::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  gaia_oauth_client_->GetTokenInfo(access_token,
                                   max_refresh_token_validation_retries_, this);
}

void DeviceOAuth2TokenService::OnGetTokenInfoResponse(
    std::unique_ptr<base::DictionaryValue> token_info) {
  std::string gaia_robot_id;
  // For robot accounts email id is the account id.
  token_info->GetString("email", &gaia_robot_id);
  gaia_oauth_client_.reset();

  CheckRobotAccountId(CoreAccountId(gaia_robot_id));
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

std::unique_ptr<OAuth2AccessTokenFetcher>
DeviceOAuth2TokenService::CreateAccessTokenFetcher(
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    OAuth2AccessTokenConsumer* consumer) {
  std::string refresh_token = GetRefreshToken();
  DCHECK(!refresh_token.empty());
  return std::make_unique<OAuth2AccessTokenFetcherImpl>(
      consumer, url_loader_factory, refresh_token);
}

bool DeviceOAuth2TokenService::HasRefreshToken(
    const CoreAccountId& account_id) const {
  return RefreshTokenIsAvailable(account_id);
}

scoped_refptr<network::SharedURLLoaderFactory>
DeviceOAuth2TokenService::GetURLLoaderFactory() const {
  return url_loader_factory_;
}

void DeviceOAuth2TokenService::FireRefreshTokenAvailable(
    const CoreAccountId& account_id) {
  if (on_refresh_token_available_callback_)
    on_refresh_token_available_callback_.Run(account_id);
}

void DeviceOAuth2TokenService::FireRefreshTokenRevoked(
    const CoreAccountId& account_id) {
  if (on_refresh_token_revoked_callback_)
    on_refresh_token_revoked_callback_.Run(account_id);
}

bool DeviceOAuth2TokenService::HandleAccessTokenFetch(
    OAuth2AccessTokenManager::RequestImpl* request,
    const CoreAccountId& account_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& client_id,
    const std::string& client_secret,
    const OAuth2AccessTokenManager::ScopeSet& scopes) {
  switch (state_) {
    case STATE_VALIDATION_PENDING:
      // If this is the first request for a token, start validation.
      StartValidation();
      FALLTHROUGH;
    case STATE_LOADING:
    case STATE_VALIDATION_STARTED:
      // Add a pending request that will be satisfied once validation completes.
      pending_requests_.push_back(new PendingRequest(
          request->AsWeakPtr(), client_id, client_secret, scopes));
      RequestValidation();
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

  NOTREACHED() << "Unexpected state " << state_;
  return false;
}

void DeviceOAuth2TokenService::FlushPendingRequests(
    bool token_is_valid,
    GoogleServiceAuthError::State error) {
  std::vector<PendingRequest*> requests;
  requests.swap(pending_requests_);
  for (std::vector<PendingRequest*>::iterator request(requests.begin());
       request != requests.end();
       ++request) {
    std::unique_ptr<PendingRequest> scoped_request(*request);
    if (!scoped_request->request)
      continue;

    if (token_is_valid) {
      token_manager_->FetchOAuth2Token(
          scoped_request->request.get(),
          scoped_request->request->GetAccountId(), GetURLLoaderFactory(),
          scoped_request->client_id, scoped_request->client_secret,
          scoped_request->scopes);
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
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&OAuth2AccessTokenManager::RequestImpl::InformConsumer,
                     request->AsWeakPtr(), auth_error,
                     OAuth2AccessTokenConsumer::TokenResponse()));
}

std::vector<CoreAccountId> DeviceOAuth2TokenService::GetAccounts() const {
  std::vector<CoreAccountId> accounts;
  switch (state_) {
    case STATE_NO_TOKEN:
    case STATE_TOKEN_INVALID:
      return accounts;
    case STATE_LOADING:
    case STATE_VALIDATION_PENDING:
    case STATE_VALIDATION_STARTED:
    case STATE_TOKEN_VALID:
      if (!GetRobotAccountId().empty())
        accounts.push_back(GetRobotAccountId());
      return accounts;
  }

  NOTREACHED() << "Unhandled state " << state_;
  return accounts;
}

void DeviceOAuth2TokenService::OnServiceAccountIdentityChanged() {
  if (!GetRobotAccountId().empty() && !refresh_token_.empty())
    FireRefreshTokenAvailable(GetRobotAccountId());
}

void DeviceOAuth2TokenService::CheckRobotAccountId(
    const CoreAccountId& gaia_robot_id) {
  // Make sure the value returned by GetRobotAccountId has been validated
  // against current device settings.
  switch (CrosSettings::Get()->PrepareTrustedValues(
      base::Bind(&DeviceOAuth2TokenService::CheckRobotAccountId,
                 weak_ptr_factory_.GetWeakPtr(), gaia_robot_id))) {
    case CrosSettingsProvider::TRUSTED:
      // All good, compare account ids below.
      break;
    case CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // The callback passed to PrepareTrustedValues above will trigger a
      // re-check eventually.
      return;
    case CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      // There's no trusted account id, which is equivalent to no token present.
      LOG(WARNING) << "Device settings permanently untrusted.";
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
      NOTREACHED();
      return std::string();
    case STATE_VALIDATION_PENDING:
    case STATE_VALIDATION_STARTED:
    case STATE_TOKEN_VALID:
      return refresh_token_;
  }

  NOTREACHED() << "Unhandled state " << state_;
  return std::string();
}

void DeviceOAuth2TokenService::DidGetSystemSalt(
    const std::string& system_salt) {
  system_salt_ = system_salt;

  // Bail out if system salt is not available.
  if (system_salt_.empty()) {
    LOG(ERROR) << "Failed to get system salt.";
    FlushTokenSaveCallbacks(false);
    state_ = STATE_NO_TOKEN;
    return;
  }

  // If the token has been set meanwhile, write it to |local_state_|.
  if (!refresh_token_.empty()) {
    EncryptAndSaveToken();
    return;
  }

  // Otherwise, load the refresh token from |local_state_|.
  std::string encrypted_refresh_token =
      local_state_->GetString(prefs::kDeviceRobotAnyApiRefreshToken);
  if (!encrypted_refresh_token.empty()) {
    CryptohomeTokenEncryptor encryptor(system_salt_);
    refresh_token_ = encryptor.DecryptWithSystemSalt(encrypted_refresh_token);
    if (refresh_token_.empty()) {
      LOG(ERROR) << "Failed to decrypt refresh token.";
      state_ = STATE_NO_TOKEN;
      return;
    }
  }

  state_ = STATE_VALIDATION_PENDING;

  // If there are pending requests, start a validation.
  if (validation_requested_)
    StartValidation();

  // Announce the token.
  if (!GetRobotAccountId().empty()) {
    FireRefreshTokenAvailable(GetRobotAccountId());
  }
}

void DeviceOAuth2TokenService::EncryptAndSaveToken() {
  DCHECK_NE(state_, STATE_LOADING);

  CryptohomeTokenEncryptor encryptor(system_salt_);
  std::string encrypted_refresh_token =
      encryptor.EncryptWithSystemSalt(refresh_token_);
  bool result = true;
  if (encrypted_refresh_token.empty()) {
    LOG(ERROR) << "Failed to encrypt refresh token; save aborted.";
    result = false;
  } else {
    local_state_->SetString(prefs::kDeviceRobotAnyApiRefreshToken,
                            encrypted_refresh_token);
  }

  FlushTokenSaveCallbacks(result);
}

void DeviceOAuth2TokenService::FlushTokenSaveCallbacks(bool result) {
  std::vector<StatusCallback> callbacks;
  callbacks.swap(token_save_callbacks_);
  for (std::vector<StatusCallback>::iterator callback(callbacks.begin());
       callback != callbacks.end(); ++callback) {
    if (!callback->is_null())
      callback->Run(result);
  }
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
      client_info, refresh_token_,
      std::vector<std::string>(1, GaiaConstants::kOAuthWrapBridgeUserInfoScope),
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

}  // namespace chromeos
