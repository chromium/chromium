// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/auth/arc_active_directory_enrollment_token_fetcher.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/arc/arc_optin_uma.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/dm_token_storage.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/tpm/install_attributes.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace em = enterprise_management;

namespace {

constexpr char kSamlAuthErrorMessage[] = "SAML authentication failed. ";

policy::BrowserPolicyConnectorChromeOS* GetConnector() {
  return g_browser_process->platform_part()
      ->browser_policy_connector_chromeos();
}

policy::DeviceManagementService* GetDeviceManagementService() {
  return GetConnector()->device_management_service();
}

std::string GetClientId() {
  return GetConnector()->GetInstallAttributes()->GetDeviceId();
}

std::string GetDmServerUrl() {
  return GetDeviceManagementService()->configuration()->GetDMServerUrl();
}

}  // namespace

namespace arc {

ArcActiveDirectoryEnrollmentTokenFetcher::
    ArcActiveDirectoryEnrollmentTokenFetcher(ArcSupportHost* support_host)
    : support_host_(support_host) {
  DCHECK(support_host_);
  support_host_->SetAuthDelegate(this);
}

ArcActiveDirectoryEnrollmentTokenFetcher::
    ~ArcActiveDirectoryEnrollmentTokenFetcher() {
  support_host_->SetAuthDelegate(nullptr);
}

void ArcActiveDirectoryEnrollmentTokenFetcher::Fetch(FetchCallback callback) {
  DCHECK(callback_.is_null());
  DCHECK(auth_session_id_.empty());
  callback_ = std::move(callback);
  dm_token_storage_ = std::make_unique<policy::DMTokenStorage>(
      g_browser_process->local_state());
  dm_token_storage_->RetrieveDMToken(base::BindOnce(
      &ArcActiveDirectoryEnrollmentTokenFetcher::OnDMTokenAvailable,
      weak_ptr_factory_.GetWeakPtr()));
}

void ArcActiveDirectoryEnrollmentTokenFetcher::OnDMTokenAvailable(
    const std::string& dm_token) {
  if (dm_token.empty()) {
    LOG(ERROR) << "Retrieving the DMToken failed.";
    std::move(callback_).Run(Status::FAILURE, std::string(), std::string());
    return;
  }

  DCHECK(dm_token_.empty());
  dm_token_ = dm_token;
  DoFetchEnrollmentToken();
}

void ArcActiveDirectoryEnrollmentTokenFetcher::DoFetchEnrollmentToken() {
  DCHECK(!dm_token_.empty());
  DCHECK(!fetch_request_job_);
  VLOG(1) << "Fetching enrollment token";

  policy::DeviceManagementService* service = GetDeviceManagementService();
  std::unique_ptr<policy::DMServerJobConfiguration> config =
      std::make_unique<policy::DMServerJobConfiguration>(
          service,
          policy::DeviceManagementService::JobConfiguration::
              TYPE_ACTIVE_DIRECTORY_ENROLL_PLAY_USER,
          GetClientId(), /*critical=*/false,
          policy::DMAuth::FromDMToken(dm_token_), /*oauth_token=*/base::nullopt,
          url_loader_factory_for_testing()
              ? url_loader_factory_for_testing()
              : g_browser_process->system_network_context_manager()
                    ->GetSharedURLLoaderFactory(),
          base::Bind(&ArcActiveDirectoryEnrollmentTokenFetcher::
                         OnEnrollmentTokenResponseReceived,
                     weak_ptr_factory_.GetWeakPtr()));

  em::ActiveDirectoryEnrollPlayUserRequest* enroll_request =
      config->request()->mutable_active_directory_enroll_play_user_request();
  if (!auth_session_id_.empty()) {
    // Happens after going through SAML flow. Call DM server again with the
    // given |auth_session_id_|.
    enroll_request->set_auth_session_id(auth_session_id_);
    auth_session_id_.clear();
  }

  fetch_request_job_ = service->CreateJob(std::move(config));
}

void ArcActiveDirectoryEnrollmentTokenFetcher::
    OnEnrollmentTokenResponseReceived(
        policy::DeviceManagementService::Job* job,
        policy::DeviceManagementStatus dm_status,
        int net_error,
        const em::DeviceManagementResponse& response) {
  VLOG(1) << "Enrollment token response received. DM Status: " << dm_status;
  fetch_request_job_.reset();

  Status fetch_status;
  std::string enrollment_token;
  std::string user_id;

  switch (dm_status) {
    case policy::DM_STATUS_SUCCESS: {
      if (!response.has_active_directory_enroll_play_user_response()) {
        LOG(WARNING) << "Invalid Active Directory enroll Play user response.";
        fetch_status = Status::FAILURE;
        break;
      }
      const em::ActiveDirectoryEnrollPlayUserResponse& enroll_response =
          response.active_directory_enroll_play_user_response();

      if (enroll_response.has_saml_parameters()) {
        // SAML authentication required.
        const em::SamlParametersProto& saml_params =
            enroll_response.saml_parameters();
        auth_session_id_ = saml_params.auth_session_id();
        InitiateSamlFlow(saml_params.auth_redirect_url());
        return;  // SAML flow eventually calls |callback_| or this function.
      }

      DCHECK(enroll_response.has_enrollment_token());
      fetch_status = Status::SUCCESS;
      enrollment_token = enroll_response.enrollment_token();
      user_id = enroll_response.user_id();
      break;
    }
    case policy::DM_STATUS_SERVICE_ARC_DISABLED:
    case policy::DM_STATUS_SERVICE_POLICY_NOT_FOUND: {
      // POLICY_NOT_FOUND is the first error encountered when the domain is not
      // set up yet in CPanel, so just treat it the same as ARC_DISABLED.
      fetch_status = Status::ARC_DISABLED;
      break;
    }
    default: {  // All other error cases
      LOG(ERROR) << "Fetching an enrollment token failed. DM Status: "
                 << dm_status;
      fetch_status = Status::FAILURE;
      break;
    }
  }

  VLOG(1) << "Enrollment token fetch finished. Status: "
          << static_cast<int>(fetch_status);
  dm_token_.clear();
  auth_session_id_.clear();
  std::move(callback_).Run(fetch_status, enrollment_token, user_id);
}

void ArcActiveDirectoryEnrollmentTokenFetcher::InitiateSamlFlow(
    const std::string& auth_redirect_url) {
  VLOG(1) << "Initiating SAML flow. Auth redirect URL: " << auth_redirect_url;

  // We must have an auth session id. Otherwise, we might end up in a loop.
  if (auth_session_id_.empty()) {
    LOG(ERROR) << kSamlAuthErrorMessage << "No auth session id.";
    CancelSamlFlow();
    return;
  }

  // Check if URL is valid.
  const GURL redirect_url(auth_redirect_url);
  if (!redirect_url.is_valid()) {
    LOG(ERROR) << kSamlAuthErrorMessage << "Redirect URL invalid.";
    CancelSamlFlow();
    return;
  }

  // Send the URL to the support host to display it in a web view inside the
  // Active Directory auth page.
  support_host_->ShowActiveDirectoryAuth(redirect_url, GetDmServerUrl());
}

void ArcActiveDirectoryEnrollmentTokenFetcher::CancelSamlFlow() {
  VLOG(1) << "Cancelling SAML flow.";
  dm_token_.clear();
  auth_session_id_.clear();
  DCHECK(!callback_.is_null());
  std::move(callback_).Run(Status::FAILURE, std::string(), std::string());
}

void ArcActiveDirectoryEnrollmentTokenFetcher::OnAuthSucceeded() {
  VLOG(1) << "SAML auth succeeded.";
  DCHECK(!auth_session_id_.empty());
  DoFetchEnrollmentToken();
  support_host_->ShowArcLoading();
}

void ArcActiveDirectoryEnrollmentTokenFetcher::OnAuthFailed(
    const std::string& error_msg) {
  LOG(ERROR) << "SAML auth failed: " << error_msg;

  // Don't call callback here, allow user to retry.
  support_host_->ShowError(ArcSupportHost::Error::SERVER_COMMUNICATION_ERROR,
                           true /* should_show_send_feedback */);
  UpdateOptInCancelUMA(OptInCancelReason::NETWORK_ERROR);
}

void ArcActiveDirectoryEnrollmentTokenFetcher::OnAuthRetryClicked() {
  VLOG(1) << "Retrying token fetch.";

  // Retry the full flow (except DM token fetch), not just the SAML part, in
  // case DM server returned bad data.
  auth_session_id_.clear();
  DCHECK(!callback_.is_null());
  support_host_->ShowArcLoading();
  DoFetchEnrollmentToken();
}

}  // namespace arc
