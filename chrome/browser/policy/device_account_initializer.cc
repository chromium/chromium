// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/device_account_initializer.h"

#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {

DeviceAccountInitializer::DeviceAccountInitializer(CloudPolicyClient* client,
                                                   Delegate* delegate)
    : client_(client), delegate_(delegate), handling_request_(false) {
  client_->AddObserver(this);
}

DeviceAccountInitializer::~DeviceAccountInitializer() {
  Stop();
  client_->RemoveObserver(this);
}

void DeviceAccountInitializer::FetchToken() {
  CHECK(client_->is_registered());
  handling_request_ = true;
  client_->FetchRobotAuthCodes(
      DMAuth::FromDMToken(client_->dm_token()),
      delegate_->GetRobotAuthCodeDeviceType(), delegate_->GetRobotOAuthScopes(),
      base::BindOnce(&DeviceAccountInitializer::OnRobotAuthCodesFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceAccountInitializer::OnRobotAuthCodesFetched(
    DeviceManagementStatus status,
    const std::string& auth_code) {
  if (status != DM_STATUS_SUCCESS) {
    handling_request_ = false;
    delegate_->OnDeviceAccountTokenFetchError(status);
    return;
  }
  if (auth_code.empty()) {
    // If the server doesn't provide an auth code, skip the robot auth setup.
    // This allows clients running against the test server to transparently skip
    // robot auth.
    handling_request_ = false;
    delegate_->OnDeviceAccountTokenFetched(true);
    return;
  }

  gaia::OAuthClientInfo client_info;
  client_info.client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  client_info.client_secret =
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret();
  client_info.redirect_uri = "oob";

  DCHECK(delegate_->GetURLLoaderFactory());

  // Use the system request context to avoid sending user cookies.
  gaia_oauth_client_ =
      std::make_unique<gaia::GaiaOAuthClient>(delegate_->GetURLLoaderFactory());
  gaia_oauth_client_->GetTokensFromAuthCode(client_info, auth_code,
                                            0 /* max_retries */, this);
}

// GaiaOAuthClient::Delegate callback for OAuth2 refresh token fetched.
void DeviceAccountInitializer::OnGetTokensResponse(
    const std::string& refresh_token,
    const std::string& access_token,
    int expires_in_seconds) {
  robot_refresh_token_ = refresh_token;
  handling_request_ = false;
  delegate_->OnDeviceAccountTokenFetched(false);
}

// GaiaOAuthClient::Delegate
void DeviceAccountInitializer::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  // We never use the code that should trigger this callback.
  handling_request_ = false;
  LOG(FATAL) << "Unexpected callback invoked.";
}

// GaiaOAuthClient::Delegate OAuth2 error when fetching refresh token request.
void DeviceAccountInitializer::OnOAuthError() {
  // OnOAuthError is only called if the request is bad (malformed) or the
  // response is bad (empty access token returned).
  LOG(ERROR) << "OAuth protocol error while fetching API refresh token.";
  handling_request_ = false;
  delegate_->OnDeviceAccountTokenFetchError(/*dm_status=*/std::nullopt);
}

// GaiaOAuthClient::Delegate network error when fetching refresh token.
void DeviceAccountInitializer::OnNetworkError(int response_code) {
  LOG(ERROR) << "Network error while fetching API refresh token: "
             << response_code;
  handling_request_ = false;
  delegate_->OnDeviceAccountTokenFetchError(/*dm_status=*/std::nullopt);
}

void DeviceAccountInitializer::StoreToken() {
  handling_request_ = true;
  DeviceOAuth2TokenServiceFactory::Get()->SetAndSaveRefreshToken(
      robot_refresh_token_,
      base::BindOnce(&DeviceAccountInitializer::HandleStoreRobotAuthTokenResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceAccountInitializer::HandleStoreRobotAuthTokenResult(bool result) {
  handling_request_ = false;
  if (!result) {
    LOG(ERROR) << "Failed to store API refresh token.";
    delegate_->OnDeviceAccountTokenStoreError();
    return;
  }
  delegate_->OnDeviceAccountTokenStored();
}

void DeviceAccountInitializer::Stop() {
  handling_request_ = false;
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void DeviceAccountInitializer::OnPolicyFetched(CloudPolicyClient* client) {}

void DeviceAccountInitializer::OnRegistrationStateChanged(
    CloudPolicyClient* client) {}

void DeviceAccountInitializer::OnClientError(CloudPolicyClient* client) {
  if (!handling_request_)
    return;
  DCHECK_EQ(client_, client);
  handling_request_ = false;
  delegate_->OnDeviceAccountClientError(client->last_dm_status());
}

}  // namespace policy
