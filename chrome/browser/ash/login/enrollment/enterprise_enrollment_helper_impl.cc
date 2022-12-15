// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/login/enrollment/enrollment_uma.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_client_factory_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/policy_oauth2_token_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_handler.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

// The OAuth token consumer name.
const char kOAuthConsumerName[] = "enterprise_enrollment";

// A helper class that takes care of asynchronously revoking a given token.
class TokenRevoker : public GaiaAuthConsumer {
 public:
  TokenRevoker();

  TokenRevoker(const TokenRevoker&) = delete;
  TokenRevoker& operator=(const TokenRevoker&) = delete;

  ~TokenRevoker() override;

  void Start(const std::string& token);

  // GaiaAuthConsumer:
  void OnOAuth2RevokeTokenCompleted(
      GaiaAuthConsumer::TokenRevocationStatus status) override;

 private:
  GaiaAuthFetcher gaia_fetcher_;
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

}  // namespace

EnterpriseEnrollmentHelperImpl::EnterpriseEnrollmentHelperImpl() {
  // Init the TPM if it has not been done until now (in debug build we might
  // have not done that yet).
  chromeos::TpmManagerClient::Get()->TakeOwnership(
      ::tpm_manager::TakeOwnershipRequest(), base::DoNothing());
}

EnterpriseEnrollmentHelperImpl::~EnterpriseEnrollmentHelperImpl() {
  DCHECK(
      g_browser_process->IsShuttingDown() ||
      oauth_status_ == OAUTH_NOT_STARTED ||
      (oauth_status_ == OAUTH_FINISHED && (success_ || oauth_data_cleared_)));
}

void EnterpriseEnrollmentHelperImpl::Setup(
    policy::ActiveDirectoryJoinDelegate* ad_join_delegate,
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain,
    policy::LicenseType license_type) {
  ad_join_delegate_ = ad_join_delegate;
  enrollment_config_ = enrollment_config;
  enrolling_user_domain_ = enrolling_user_domain;
  license_type_ = license_type;
}

void EnterpriseEnrollmentHelperImpl::EnrollUsingAuthCode(
    const std::string& auth_code) {
  DCHECK(oauth_status_ == OAUTH_NOT_STARTED);
  oauth_status_ = OAUTH_STARTED_WITH_AUTH_CODE;
  oauth_fetcher_ =
      policy::PolicyOAuth2TokenFetcher::CreateInstance(kOAuthConsumerName);
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
  SigninProfileHandler::Get()->ClearSigninProfile(
      base::BindOnce(&EnterpriseEnrollmentHelperImpl::OnSigninProfileCleared,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void EnterpriseEnrollmentHelperImpl::DoEnroll(policy::DMAuth auth_data) {
  DCHECK(auth_data_.empty() || auth_data_ == auth_data);
  DCHECK(enrollment_config_.is_mode_attestation() ||
         oauth_status_ == OAUTH_STARTED_WITH_AUTH_CODE ||
         oauth_status_ == OAUTH_STARTED_WITH_TOKEN);
  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Enroll with token type: "
               << static_cast<int>(auth_data.token_type());
  auth_data_ = std::move(auth_data);
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
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

  DCHECK(!enrollment_handler_);
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  // Obtain attestation_flow from the connector because it can be fake
  // attestation flow for testing.
  attestation::AttestationFlow* attestation_flow =
      connector->GetAttestationFlow();
  // DeviceDMToken callback is empty here because for device policies this
  // DMToken is already provided in the policy fetch requests.
  auto client = policy::CreateDeviceCloudPolicyClientAsh(
      chromeos::system::StatisticsProvider::GetInstance(),
      connector->device_management_service(),
      g_browser_process->shared_url_loader_factory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());

  enrollment_handler_ = std::make_unique<policy::EnrollmentHandler>(
      policy_manager->device_store(), InstallAttributes::Get(),
      connector->GetStateKeysBroker(), attestation_flow, std::move(client),
      policy::BrowserPolicyConnectorAsh::CreateBackgroundTaskRunner(),
      ad_join_delegate_, enrollment_config_, license_type_, auth_data_.Clone(),
      InstallAttributes::Get()->GetDeviceId(),
      policy::EnrollmentRequisitionManager::GetDeviceRequisition(),
      policy::EnrollmentRequisitionManager::GetSubOrganization(),
      base::BindOnce(&EnterpriseEnrollmentHelperImpl::OnEnrollmentFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  enrollment_handler_->StartEnrollment();
}

void EnterpriseEnrollmentHelperImpl::GetDeviceAttributeUpdatePermission() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  // Don't update device attributes for Active Directory management.
  if (connector->IsActiveDirectoryManaged()) {
    OnDeviceAttributeUpdatePermission(false);
    return;
  }
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  policy::CloudPolicyClient* client = policy_manager->core()->client();

  absl::optional<policy::DMAuth> auth =
      GetDMAuthForDeviceAttributeUpdate(client);
  if (!auth.has_value()) {
    // There's no information about the enrolling user or device identity so
    // device attributes update permission can't be fetched. Assume "no
    // permission".
    OnDeviceAttributeUpdatePermission(/*granted=*/false);
    return;
  }
  client->GetDeviceAttributeUpdatePermission(
      std::move(auth.value()),
      base::BindOnce(
          &EnterpriseEnrollmentHelperImpl::OnDeviceAttributeUpdatePermission,
          weak_ptr_factory_.GetWeakPtr()));
}

absl::optional<policy::DMAuth>
EnterpriseEnrollmentHelperImpl::GetDMAuthForDeviceAttributeUpdate(
    policy::CloudPolicyClient* device_cloud_policy_client) {
  // Checking whether the device attributes can be updated requires either
  // knowing which user is performing enterprise enrollment, or which device
  // is performing the attestation-based enrollment.
  if (auth_data_.has_oauth_token()) {
    return auth_data_.Clone();
  } else if (enrollment_config_.is_mode_initial_attestation_server_forced()) {
    return policy::DMAuth::FromDMToken(device_cloud_policy_client->dm_token());
  } else {
    return {};
  }
}

void EnterpriseEnrollmentHelperImpl::UpdateDeviceAttributes(
    const std::string& asset_id,
    const std::string& location) {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  policy::CloudPolicyClient* client = policy_manager->core()->client();

  absl::optional<policy::DMAuth> auth =
      GetDMAuthForDeviceAttributeUpdate(client);

  // If we got here, we must have successfully run GetDeviceAttributeUpdatePermission, which required a non-empty GetDMAuthForDeviceAttributeUpdate result.
  DCHECK(auth.has_value());

  client->UpdateDeviceAttributes(
      std::move(auth.value()), asset_id, location,
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
  // Regardless how enrollment has finished, |enrollment_handler_| is expired.
  // |client| might still be needed to connect the manager.
  auto client = enrollment_handler_->ReleaseClient();
  // Though enrollment handler calls this method, it's "fine" do delete the
  // handler here as it does not do anyhting after that callback in
  // |EnrollmentHandler::ReportResult()|.
  enrollment_handler_.reset();

  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Enrollment finished, status: " << status.status();
  ReportEnrollmentStatus(status);
  if (oauth_status_ != OAUTH_NOT_STARTED)
    oauth_status_ = OAUTH_FINISHED;

  if (status.status() != policy::EnrollmentStatus::SUCCESS) {
    status_consumer()->OnEnrollmentError(status);
    return;
  }

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();

  // Connect policy manager if necessary. If it has already been connected, no
  // reconnection needed.
  if (!policy_manager->IsConnected()) {
    policy_manager->StartConnection(std::move(client),
                                    InstallAttributes::Get());
  }

  success_ = true;
  StartupUtils::MarkOobeCompleted();
  status_consumer()->OnDeviceEnrolled();
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
    case GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR:
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
        case policy::DM_STATUS_SERVICE_DEVICE_NEEDS_RESET:
          NOTREACHED();
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
        case policy::DM_STATUS_SERVICE_INVALID_PACKAGED_DEVICE_FOR_KIOSK:
          UMA(policy::kMetricEnrollmentInvalidPackagedDeviceForKIOSK);
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
    case policy::EnrollmentStatus::MAY_NOT_BLOCK_DEV_MODE:
      UMA(policy::kMetricEnrollmentMayNotBlockDevMode);
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

}  // namespace ash
