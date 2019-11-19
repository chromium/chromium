// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_account_initializer.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/policy/active_directory_join_delegate.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service.h"
#include "chrome/browser/chromeos/settings/device_oauth2_token_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/attestation/attestation_flow.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/auth_policy/auth_policy_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#include "components/policy/core/common/cloud/dm_auth.h"

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
      base::BindOnce(&DeviceAccountInitializer::OnRobotAuthCodesFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceAccountInitializer::OnRobotAuthCodesFetched(
    DeviceManagementStatus status,
    const std::string& auth_code) {
  if (status != DM_STATUS_SUCCESS) {
    handling_request_ = false;
    delegate_->OnDeviceAccountTokenError(
        EnrollmentStatus::ForRobotAuthFetchError(status));
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

  // Use the system request context to avoid sending user cookies.
  gaia_oauth_client_.reset(new gaia::GaiaOAuthClient(
      g_browser_process->shared_url_loader_factory()));
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
  delegate_->OnDeviceAccountTokenError(
      EnrollmentStatus::ForRobotRefreshFetchError(net::HTTP_BAD_REQUEST));
}

// GaiaOAuthClient::Delegate network error when fetching refresh token.
void DeviceAccountInitializer::OnNetworkError(int response_code) {
  LOG(ERROR) << "Network error while fetching API refresh token: "
             << response_code;
  handling_request_ = false;
  delegate_->OnDeviceAccountTokenError(
      EnrollmentStatus::ForRobotRefreshFetchError(response_code));
}

void DeviceAccountInitializer::StoreToken() {
  handling_request_ = true;
  chromeos::DeviceOAuth2TokenServiceFactory::Get()->SetAndSaveRefreshToken(
      robot_refresh_token_,
      base::AdaptCallbackForRepeating(base::BindOnce(
          &DeviceAccountInitializer::HandleStoreRobotAuthTokenResult,
          weak_ptr_factory_.GetWeakPtr())));
}

void DeviceAccountInitializer::HandleStoreRobotAuthTokenResult(bool result) {
  handling_request_ = false;
  if (!result) {
    LOG(ERROR) << "Failed to store API refresh token.";
    delegate_->OnDeviceAccountTokenError(EnrollmentStatus::ForStatus(
        EnrollmentStatus::ROBOT_REFRESH_STORE_FAILED));
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
  delegate_->OnDeviceAccountClientError(client->status());
}

}  // namespace policy
