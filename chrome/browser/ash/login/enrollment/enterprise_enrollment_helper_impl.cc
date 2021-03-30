// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/enrollment/enrollment_uma.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/policy_oauth2_token_fetcher.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/enrollment_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// A helper class that takes care of asynchronously revoking a given token.
class TokenRevoker : public GaiaAuthConsumer {
 public:
  TokenRevoker();
  ~TokenRevoker() override;

  void Start(const std::string& token);

  // GaiaAuthConsumer:
  void OnOAuth2RevokeTokenCompleted(
      GaiaAuthConsumer::TokenRevocationStatus status) override;

 private:
  GaiaAuthFetcher gaia_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(TokenRevoker);
};

TokenRevoker::TokenRevoker()
    : gaia_fetcher_(this,
                    gaia::GaiaSource::kChromeOS,
                    g_browser_process->system_network_context_manager()
                        ->GetSharedURLLoaderFactory()) {}

TokenRevoker::~TokenRevoker() {}

void TokenRevoker::Start(const std::string& token) {
  gaia_fetcher_.StartRevokeOAuth2Token(token);
}

void TokenRevoker::OnOAuth2RevokeTokenCompleted(
    GaiaAuthConsumer::TokenRevocationStatus status) {
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

}  // namespace

namespace chromeos {

EnterpriseEnrollmentHelperImpl::EnterpriseEnrollmentHelperImpl() {
  // Init the TPM if it has not been done until now (in debug build we might
  // have not done that yet).
  TpmManagerClient::Get()->TakeOwnership(::tpm_manager::TakeOwnershipRequest(),
                                         base::DoNothing());
}

EnterpriseEnrollmentHelperImpl::~EnterpriseEnrollmentHelperImpl() {
  DCHECK(
      g_browser_process->IsShuttingDown() ||
      oauth_status_ == OAUTH_NOT_STARTED ||
      (oauth_status_ == OAUTH_FINISHED && (success_ || oauth_data_cleared_)));
}

void EnterpriseEnrollmentHelperImpl::Setup(
    ActiveDirectoryJoinDelegate* ad_join_delegate,
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain) {
  ad_join_delegate_ = ad_join_delegate;
  enrollment_config_ = enrollment_config;
  enrolling_user_domain_ = enrolling_user_domain;
}

void EnterpriseEnrollmentHelperImpl::EnrollUsingAuthCode(
    const std::string& auth_code) {
  DCHECK(oauth_status_ == OAUTH_NOT_STARTED);
  oauth_status_ = OAUTH_STARTED_WITH_AUTH_CODE;
  oauth_fetcher_ = policy::PolicyOAuth2TokenFetcher::CreateInstance();
  oauth_fetcher_->StartWithAuthCode(
      auth_code,
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      base::BindOnce(&EnterpriseEnrollmentHelperImpl::OnTokenFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnterpriseEnrollmentHelperImpl::EnrollUsingToken(
    const std::string& token) {
  DCHECK(oauth_status_ != OAUTH_STARTED_WITH_TOKEN);
  if (oauth_status_ == OAUTH_NOT_STARTED)
    oauth_status_ = OAUTH_STARTED_WITH_TOKEN;
  DoEnroll(policy::DMAuth::FromOAuthToken(token));
}

void EnterpriseEnrollmentHelperImpl::EnrollUsingAttestation() {
  CHECK(enrollment_config_.is_mode_attestation());
  // The tokens are not used in attestation mode.
  DoEnroll(policy::DMAuth::NoAuth());
}

void EnterpriseEnrollmentHelperImpl::EnrollUsingEnrollmentToken(
    const std::string& token) {
  CHECK_EQ(enrollment_config_.mode,
           policy::EnrollmentConfig::MODE_ATTESTATION_ENROLLMENT_TOKEN);
  DoEnroll(policy::DMAuth::FromEnrollmentToken(token));
}

void EnterpriseEnrollmentHelperImpl::EnrollForOfflineDemo() {
  CHECK_EQ(enrollment_config_.mode,
           policy::EnrollmentConfig::MODE_OFFLINE_DEMO);
  // The tokens are not used in offline demo mode.
  DoEnroll(policy::DMAuth::NoAuth());
}

void EnterpriseEnrollmentHelperImpl::RestoreAfterRollback() {
  CHECK_EQ(enrollment_config_.mode,
           policy::EnrollmentConfig::MODE_ENROLLED_ROLLBACK);
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  DCHECK(connector->IsCloudManaged());

  auto* manager = connector->GetDeviceCloudPolicyManager();

  if (manager->core()->client()) {
    RestoreAfterRollbackInitialized();
  } else {
    manager->AddDeviceCloudPolicyManagerObserver(this);
  }
}

void EnterpriseEnrollmentHelperImpl::OnDeviceCloudPolicyManagerConnected() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  connector->GetDeviceCloudPolicyManager()
      ->RemoveDeviceCloudPolicyManagerObserver(this);
  RestoreAfterRollbackInitialized();
}

void EnterpriseEnrollmentHelperImpl::OnDeviceCloudPolicyManagerDisconnected() {}

void EnterpriseEnrollmentHelperImpl::RestoreAfterRollbackInitialized() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  auto* manager = connector->GetDeviceCloudPolicyManager();
  auto* client = manager->core()->client();
  DCHECK(client);

  device_account_initializer_ =
      std::make_unique<policy::DeviceAccountInitializer>(client, this);
  device_account_initializer_->FetchToken();
}

void EnterpriseEnrollmentHelperImpl::OnDeviceAccountTokenFetched(
    bool empty_token) {
  if (empty_token) {
    status_consumer()->OnRestoreAfterRollbackCompleted();
    return;
  }
  device_account_initializer_->StoreToken();
}

void EnterpriseEnrollmentHelperImpl::OnDeviceAccountTokenStored() {
  status_consumer()->OnRestoreAfterRollbackCompleted();
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, device_account_initializer_.release());
}

void EnterpriseEnrollmentHelperImpl::OnDeviceAccountTokenError(
    policy::EnrollmentStatus status) {
  OnEnrollmentFinished(status);
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, device_account_initializer_.release());
}

void EnterpriseEnrollmentHelperImpl::OnDeviceAccountClientError(
    policy::DeviceManagementStatus status) {
  OnEnrollmentFinished(
      policy::EnrollmentStatus::ForRobotAuthFetchError(status));
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(
      FROM_HERE, device_account_initializer_.release());
}

enterprise_management::DeviceServiceApiAccessRequest::DeviceType
EnterpriseEnrollmentHelperImpl::GetRobotAuthCodeDeviceType() {
  return enterprise_management::DeviceServiceApiAccessRequest::CHROME_OS;
}

std::set<std::string> EnterpriseEnrollmentHelperImpl::GetRobotOAuthScopes() {
  return {GaiaConstants::kAnyApiOAuth2Scope};
}

scoped_refptr<network::SharedURLLoaderFactory>
EnterpriseEnrollmentHelperImpl::GetURLLoaderFactory() {
  return g_browser_process->system_network_context_manager()
      ->GetSharedURLLoaderFactory();
}

void EnterpriseEnrollmentHelperImpl::ClearAuth(base::OnceClosure callback) {
  if (oauth_status_ != OAUTH_NOT_STARTED) {
    if (oauth_fetcher_) {
      if (!oauth_fetcher_->OAuth2AccessToken().empty())
        (new TokenRevoker())->Start(oauth_fetcher_->OAuth2AccessToken());

      if (!oauth_fetcher_->OAuth2RefreshToken().empty())
        (new TokenRevoker())->Start(oauth_fetcher_->OAuth2RefreshToken());

      oauth_fetcher_.reset();
    } else if (auth_data_.has_oauth_token()) {
      // EnrollUsingToken was called.
      (new TokenRevoker())->Start(auth_data_.oauth_token());
    }
  }
  auth_data_ = policy::DMAuth::NoAuth();
  chromeos::ProfileHelper::Get()->ClearSigninProfile(
      base::BindOnce(&EnterpriseEnrollmentHelperImpl::OnSigninProfileCleared,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void EnterpriseEnrollmentHelperImpl::DoEnroll(policy::DMAuth auth_data) {
  DCHECK(auth_data_.empty() || auth_data_ == auth_data);
  DCHECK(enrollment_config_.is_mode_attestation() ||
         enrollment_config_.mode ==
             policy::EnrollmentConfig::MODE_OFFLINE_DEMO ||
         oauth_status_ == OAUTH_STARTED_WITH_AUTH_CODE ||
         oauth_status_ == OAUTH_STARTED_WITH_TOKEN);
  VLOG(1) << "Enroll with token type: "
          << static_cast<int>(auth_data.token_type());
  auth_data_ = std::move(auth_data);
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  // Re-enrollment is not implemented for Active Directory.
  // If an enrollment domain is already fixed in install attributes and
  // re-enrollment happens via login, domains need to be equal.
  // If there is a mismatch between domain set in install attributes and
  // auto re-enrollment domain provided by the server, policy validation will
  // fail later in the process.
  if (connector->IsCloudManaged() && !enrolling_user_domain_.empty() &&
      !enrollment_config_.is_mode_attestation() &&
      connector->GetEnterpriseEnrollmentDomain() != enrolling_user_domain_) {
    LOG(ERROR) << "Trying to re-enroll to a different domain than "
               << connector->GetEnterpriseEnrollmentDomain();
    UMA(policy::kMetricEnrollmentPrecheckDomainMismatch);
    if (oauth_status_ != OAUTH_NOT_STARTED)
      oauth_status_ = OAUTH_FINISHED;
    status_consumer()->OnOtherError(OTHER_ERROR_DOMAIN_MISMATCH);
    return;
  }

  connector->ScheduleServiceInitialization(0);
  policy::DeviceCloudPolicyInitializer* dcp_initializer =
      connector->GetDeviceCloudPolicyInitializer();
  CHECK(dcp_initializer);
  dcp_initializer->PrepareEnrollment(
      connector->device_management_service(), ad_join_delegate_,
      enrollment_config_, auth_data_.Clone(),
      base::BindOnce(&EnterpriseEnrollmentHelperImpl::OnEnrollmentFinished,
                     weak_ptr_factory_.GetWeakPtr()));
  dcp_initializer->StartEnrollment();
}

void EnterpriseEnrollmentHelperImpl::GetDeviceAttributeUpdatePermission() {
  if (!auth_data_.has_oauth_token()) {
    // Checking whether the device attributes can be updated requires knowning
    // which user is performing enterprise enrollment, because the permission is
    // tied to a user.
    // For enterprise enrollment authorized by attestation or an enrollment
    // token, the current user is unknown.
    // A possible follow-up (tracked in https://crbug.com/942013) will be to
    // allow the first affiliated user that signs in and has the permission to
    // edit device attributes.
    OnDeviceAttributeUpdatePermission(/*granted=*/false);
    return;
  }

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  // Don't update device attributes for Active Directory management.
  if (connector->IsActiveDirectoryManaged()) {
    OnDeviceAttributeUpdatePermission(false);
    return;
  }
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  policy::CloudPolicyClient* client = policy_manager->core()->client();

  client->GetDeviceAttributeUpdatePermission(
      auth_data_.Clone(),
      base::BindOnce(
          &EnterpriseEnrollmentHelperImpl::OnDeviceAttributeUpdatePermission,
          weak_ptr_factory_.GetWeakPtr()));
}

void EnterpriseEnrollmentHelperImpl::UpdateDeviceAttributes(
    const std::string& asset_id,
    const std::string& location) {
  DCHECK(!auth_data_.empty());
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  policy::CloudPolicyClient* client = policy_manager->core()->client();

  client->UpdateDeviceAttributes(
      auth_data_.Clone(), asset_id, location,
      base::BindOnce(
          &EnterpriseEnrollmentHelperImpl::OnDeviceAttributeUploadCompleted,
          weak_ptr_factory_.GetWeakPtr()));
}

void EnterpriseEnrollmentHelperImpl::OnTokenFetched(
    const std::string& token,
    const GoogleServiceAuthError& error) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    ReportAuthStatus(error);
    oauth_status_ = OAUTH_FINISHED;
    status_consumer()->OnAuthError(error);
    return;
  }

  EnrollUsingToken(token);
}

void EnterpriseEnrollmentHelperImpl::OnEnrollmentFinished(
    policy::EnrollmentStatus status) {
  VLOG(1) << "Enrollment finished, status: " << status.status();
  ReportEnrollmentStatus(status);
  if (oauth_status_ != OAUTH_NOT_STARTED)
    oauth_status_ = OAUTH_FINISHED;
  if (status.status() == policy::EnrollmentStatus::SUCCESS) {
    success_ = true;
    StartupUtils::MarkOobeCompleted();
    status_consumer()->OnDeviceEnrolled();
  } else {
    status_consumer()->OnEnrollmentError(status);
  }
}

void EnterpriseEnrollmentHelperImpl::OnDeviceAttributeUpdatePermission(
    bool granted) {
  status_consumer()->OnDeviceAttributeUpdatePermission(granted);
}

void EnterpriseEnrollmentHelperImpl::OnDeviceAttributeUploadCompleted(
    bool success) {
  status_consumer()->OnDeviceAttributeUploadCompleted(success);
}

void EnterpriseEnrollmentHelperImpl::ReportAuthStatus(
    const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
    case GoogleServiceAuthError::SERVICE_ERROR:
      UMA(policy::kMetricEnrollmentLoginFailed);
      LOG(ERROR) << "Auth error " << error.state();
      break;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
      UMA(policy::kMetricEnrollmentAccountNotSignedUp);
      LOG(ERROR) << "Account not signed up " << error.state();
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      UMA(policy::kMetricEnrollmentNetworkFailed);
      LOG(WARNING) << "Network error " << error.state();
      break;
    case GoogleServiceAuthError::NUM_STATES:
      NOTREACHED();
      break;
  }
}

void EnterpriseEnrollmentHelperImpl::ReportEnrollmentStatus(
    policy::EnrollmentStatus status) {
  switch (status.status()) {
    case policy::EnrollmentStatus::SUCCESS:
      UMA(policy::kMetricEnrollmentOK);
      return;
    case policy::EnrollmentStatus::REGISTRATION_FAILED:
    case policy::EnrollmentStatus::POLICY_FETCH_FAILED:
      switch (status.client_status()) {
        case policy::DM_STATUS_SUCCESS:
          NOTREACHED();
          break;
        case policy::DM_STATUS_REQUEST_INVALID:
          UMA(policy::kMetricEnrollmentRegisterPolicyPayloadInvalid);
          break;
        case policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
          UMA(policy::kMetricEnrollmentRegisterPolicyDeviceNotFound);
          break;
        case policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
          UMA(policy::kMetricEnrollmentRegisterPolicyDMTokenInvalid);
          break;
        case policy::DM_STATUS_SERVICE_ACTIVATION_PENDING:
          UMA(policy::kMetricEnrollmentRegisterPolicyActivationPending);
          break;
        case policy::DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
          UMA(policy::kMetricEnrollmentRegisterPolicyDeviceIdConflict);
          break;
        case policy::DM_STATUS_SERVICE_TOO_MANY_REQUESTS:
          UMA(policy::kMetricEnrollmentTooManyRequests);
          break;
        case policy::DM_STATUS_SERVICE_POLICY_NOT_FOUND:
          UMA(policy::kMetricEnrollmentRegisterPolicyNotFound);
          break;
        case policy::DM_STATUS_REQUEST_FAILED:
          UMA(policy::kMetricEnrollmentRegisterPolicyRequestFailed);
          break;
        case policy::DM_STATUS_TEMPORARY_UNAVAILABLE:
          UMA(policy::kMetricEnrollmentRegisterPolicyTempUnavailable);
          break;
        case policy::DM_STATUS_HTTP_STATUS_ERROR:
        case policy::DM_STATUS_REQUEST_TOO_LARGE:
          UMA(policy::kMetricEnrollmentRegisterPolicyHttpError);
          break;
        case policy::DM_STATUS_RESPONSE_DECODING_ERROR:
          UMA(policy::kMetricEnrollmentRegisterPolicyResponseInvalid);
          break;
        case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
          UMA(policy::kMetricEnrollmentNotSupported);
          break;
        case policy::DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
          UMA(policy::kMetricEnrollmentRegisterPolicyInvalidSerial);
          break;
        case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
          UMA(policy::kMetricEnrollmentRegisterPolicyMissingLicenses);
          break;
        case policy::DM_STATUS_SERVICE_DEPROVISIONED:
          UMA(policy::kMetricEnrollmentRegisterPolicyDeprovisioned);
          break;
        case policy::DM_STATUS_SERVICE_DOMAIN_MISMATCH:
          UMA(policy::kMetricEnrollmentRegisterPolicyDomainMismatch);
          break;
        case policy::DM_STATUS_CANNOT_SIGN_REQUEST:
          UMA(policy::kMetricEnrollmentRegisterCannotSignRequest);
          break;
        case policy::DM_STATUS_SERVICE_ARC_DISABLED:
          NOTREACHED();
          break;
        case policy::DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE:
          UMA(policy::
                  kMetricEnrollmentRegisterConsumerAccountWithPackagedLicense);
          break;
        case policy::
            DM_STATUS_SERVICE_ENTERPRISE_ACCOUNT_IS_NOT_ELIGIBLE_TO_ENROLL:
          UMA(policy::
                  kMetricEnrollmentRegisterEnterpriseAccountIsNotEligibleToEnroll);
          break;
        case policy::DM_STATUS_SERVICE_ENTERPRISE_TOS_HAS_NOT_BEEN_ACCEPTED:
          UMA(policy::kMetricEnrollmentRegisterEnterpriseTosHasNotBeenAccepted);
          break;
        case policy::DM_STATUS_SERVICE_ILLEGAL_ACCOUNT_FOR_PACKAGED_EDU_LICENSE:
          UMA(policy::kMetricEnrollmentIllegalAccountForPackagedEDULicense);
          break;
      }
      break;
    case policy::EnrollmentStatus::REGISTRATION_BAD_MODE:
      UMA(policy::kMetricEnrollmentInvalidEnrollmentMode);
      break;
    case policy::EnrollmentStatus::NO_STATE_KEYS:
      UMA(policy::kMetricEnrollmentNoStateKeys);
      break;
    case policy::EnrollmentStatus::VALIDATION_FAILED:
      UMA(policy::kMetricEnrollmentPolicyValidationFailed);
      break;
    case policy::EnrollmentStatus::STORE_ERROR:
      UMA(policy::kMetricEnrollmentCloudPolicyStoreError);
      break;
    case policy::EnrollmentStatus::LOCK_ERROR:
      switch (status.lock_status()) {
        case InstallAttributes::LOCK_SUCCESS:
        case InstallAttributes::LOCK_NOT_READY:
          NOTREACHED();
          break;
        case InstallAttributes::LOCK_TIMEOUT:
          UMA(policy::kMetricEnrollmentLockboxTimeoutError);
          break;
        case InstallAttributes::LOCK_BACKEND_INVALID:
          UMA(policy::kMetricEnrollmentLockBackendInvalid);
          break;
        case InstallAttributes::LOCK_ALREADY_LOCKED:
          UMA(policy::kMetricEnrollmentLockAlreadyLocked);
          break;
        case InstallAttributes::LOCK_SET_ERROR:
          UMA(policy::kMetricEnrollmentLockSetError);
          break;
        case InstallAttributes::LOCK_FINALIZE_ERROR:
          UMA(policy::kMetricEnrollmentLockFinalizeError);
          break;
        case InstallAttributes::LOCK_READBACK_ERROR:
          UMA(policy::kMetricEnrollmentLockReadbackError);
          break;
        case InstallAttributes::LOCK_WRONG_DOMAIN:
          UMA(policy::kMetricEnrollmentLockDomainMismatch);
          break;
        case InstallAttributes::LOCK_WRONG_MODE:
          UMA(policy::kMetricEnrollmentLockModeMismatch);
          break;
      }
      break;
    case policy::EnrollmentStatus::ROBOT_AUTH_FETCH_FAILED:
      UMA(policy::kMetricEnrollmentRobotAuthCodeFetchFailed);
      break;
    case policy::EnrollmentStatus::ROBOT_REFRESH_FETCH_FAILED:
      UMA(policy::kMetricEnrollmentRobotRefreshTokenFetchFailed);
      break;
    case policy::EnrollmentStatus::ROBOT_REFRESH_STORE_FAILED:
      UMA(policy::kMetricEnrollmentRobotRefreshTokenStoreFailed);
      break;
    case policy::EnrollmentStatus::ATTRIBUTE_UPDATE_FAILED:
      UMA(policy::kMetricEnrollmentAttributeUpdateFailed);
      break;
    case policy::EnrollmentStatus::REGISTRATION_CERT_FETCH_FAILED:
      UMA(policy::kMetricEnrollmentRegistrationCertificateFetchFailed);
      break;
    case policy::EnrollmentStatus::NO_MACHINE_IDENTIFICATION:
      UMA(policy::kMetricEnrollmentNoDeviceIdentification);
      break;
    case policy::EnrollmentStatus::ACTIVE_DIRECTORY_POLICY_FETCH_FAILED:
      UMA(policy::kMetricEnrollmentActiveDirectoryPolicyFetchFailed);
      break;
    case policy::EnrollmentStatus::DM_TOKEN_STORE_FAILED:
      UMA(policy::kMetricEnrollmentStoreDMTokenFailed);
      break;
    case policy::EnrollmentStatus::OFFLINE_POLICY_LOAD_FAILED:
    case policy::EnrollmentStatus::OFFLINE_POLICY_DECODING_FAILED:
      UMA(policy::kMetricEnrollmentRegisterPolicyResponseInvalid);
      break;
  }
}

void EnterpriseEnrollmentHelperImpl::UMA(policy::MetricEnrollment sample) {
  EnrollmentUMA(sample, enrollment_config_.mode);
}

void EnterpriseEnrollmentHelperImpl::OnSigninProfileCleared(
    base::OnceClosure callback) {
  oauth_data_cleared_ = true;
  std::move(callback).Run();
}

}  // namespace chromeos
