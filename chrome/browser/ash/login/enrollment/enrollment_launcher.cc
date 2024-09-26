// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chrome/browser/ash/login/enrollment/enrollment_uma.h"
#include "chrome/browser/ash/login/enrollment/oauth2_token_revoker.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_client_factory_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/policy_oauth2_token_fetcher.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_handler.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/ash/profiles/signin_profile_handler.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/attestation_flow_adaptive.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

// The OAuth token consumer name.
const char kOAuthConsumerName[] = "enterprise_enrollment";

base::NoDestructor<EnrollmentLauncher::AttestationFlowFactory>
    g_testing_attestation_flow_factory;

std::unique_ptr<ash::attestation::AttestationFlow> CreateAttestationFlow() {
  if (!g_testing_attestation_flow_factory->is_null()) {
    CHECK_IS_TEST();
    return g_testing_attestation_flow_factory->Run();
  }

  return std::make_unique<ash::attestation::AttestationFlowAdaptive>(
      std::make_unique<ash::attestation::AttestationCAClient>());
}

base::NoDestructor<EnrollmentLauncher::Factory> g_testing_factory;

class EnrollmentLauncherImpl : public EnrollmentLauncher {
 public:
  explicit EnrollmentLauncherImpl(EnrollmentStatusConsumer* status_consumer);

  EnrollmentLauncherImpl(const EnrollmentLauncherImpl&) = delete;
  EnrollmentLauncherImpl& operator=(const EnrollmentLauncherImpl&) = delete;

  ~EnrollmentLauncherImpl() override;

  // EnrollmentLauncher:
  void EnrollUsingAuthCode(const std::string& auth_code) override;
  void EnrollUsingToken(const std::string& token) override;
  void EnrollUsingAttestation() override;
  void EnrollUsingEnrollmentToken() override;
  void ClearAuth(base::OnceClosure callback,
                 bool revoke_oauth2_tokens) override;
  void GetDeviceAttributeUpdatePermission() override;
  void UpdateDeviceAttributes(const std::string& asset_id,
                              const std::string& location) override;
  void Setup(const policy::EnrollmentConfig& enrollment_config,
             const std::string& enrolling_user_domain) override;
  bool InProgress() const override;
  std::string GetOAuth2RefreshToken() const override;

 private:
  // Attempt enrollment using `auth_data` for authentication.
  void DoEnroll(policy::DMAuth auth_data);

  // Handles completion of the OAuth2 token fetch attempt.
  void OnTokenFetched(const std::string& token,
                      const GoogleServiceAuthError& error);

  // Handles completion of the enrollment attempt.
  void OnEnrollmentFinished(policy::EnrollmentStatus status);

  // Handles completion of the device attribute update permission request.
  void OnDeviceAttributeUpdatePermission(bool granted);

  // Handles completion of the device attribute update attempt.
  void OnDeviceAttributeUploadCompleted(bool success);

  void ReportAuthStatus(const GoogleServiceAuthError& error);
  void ReportEnrollmentStatus(policy::EnrollmentStatus status);

  // Logs an UMA event in the kMetricEnrollment or the kMetricEnrollmentRecovery
  // histogram, depending on `enrollment_mode_`.
  void UMA(policy::MetricEnrollment sample);

  // Called by ProfileHelper when a signin profile clearance has finished.
  // `callback` is a callback, that was passed to ClearAuth() before.
  void OnSigninProfileCleared(base::OnceClosure callback);

  // Revokes OAuth2 tokens stored in the oauth_fetcher_ or auth_data_.
  void RevokeOAuth2Tokens();

  raw_ptr<EnrollmentStatusConsumer> status_consumer_;

  // Returns either OAuth token or DM token needed for the device attribute
  // update permission request.
  std::optional<policy::DMAuth> GetDMAuthForDeviceAttributeUpdate(
      policy::CloudPolicyClient* device_cloud_policy_client);

  policy::EnrollmentConfig enrollment_config_;
  std::string enrolling_user_domain_;
  policy::LicenseType license_type_;

  enum {
    OAUTH_NOT_STARTED,
    OAUTH_STARTED_WITH_AUTH_CODE,
    OAUTH_STARTED_WITH_TOKEN,
    OAUTH_FINISHED
  } oauth_status_ = OAUTH_NOT_STARTED;
  bool oauth_data_cleared_ = false;
  policy::DMAuth auth_data_;
  bool success_ = false;

  std::unique_ptr<policy::PolicyOAuth2TokenFetcher> oauth_fetcher_;

  // Non-nullptr from DoEnroll till OnEnrollmentFinished.
  std::unique_ptr<attestation::AttestationFlow> attestation_flow_;
  std::unique_ptr<policy::EnrollmentHandler> enrollment_handler_;

  base::WeakPtrFactory<EnrollmentLauncherImpl> weak_ptr_factory_{this};
};

EnrollmentLauncherImpl::EnrollmentLauncherImpl(
    EnrollmentStatusConsumer* status_consumer)
    : status_consumer_(status_consumer) {
  // Init the TPM if it has not been done until now (in debug build we might
  // have not done that yet).
  chromeos::TpmManagerClient::Get()->TakeOwnership(
      ::tpm_manager::TakeOwnershipRequest(), base::DoNothing());
}

EnrollmentLauncherImpl::~EnrollmentLauncherImpl() {
  DCHECK(
      g_browser_process->IsShuttingDown() ||
      oauth_status_ == OAUTH_NOT_STARTED ||
      (oauth_status_ == OAUTH_FINISHED && (success_ || oauth_data_cleared_)));
}

void EnrollmentLauncherImpl::Setup(
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain) {
  enrollment_config_ = enrollment_config;
  enrolling_user_domain_ = enrolling_user_domain;
}

void EnrollmentLauncherImpl::EnrollUsingAuthCode(const std::string& auth_code) {
  DCHECK(oauth_status_ == OAUTH_NOT_STARTED);
  oauth_status_ = OAUTH_STARTED_WITH_AUTH_CODE;
  oauth_fetcher_ =
      policy::PolicyOAuth2TokenFetcher::CreateInstance(kOAuthConsumerName);
  oauth_fetcher_->StartWithAuthCode(
      auth_code,
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      base::BindOnce(&EnrollmentLauncherImpl::OnTokenFetched,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentLauncherImpl::EnrollUsingToken(const std::string& token) {
  DCHECK(oauth_status_ != OAUTH_STARTED_WITH_TOKEN);
  if (oauth_status_ == OAUTH_NOT_STARTED) {
    oauth_status_ = OAUTH_STARTED_WITH_TOKEN;
  }
  DoEnroll(policy::DMAuth::FromOAuthToken(token));
}

void EnrollmentLauncherImpl::EnrollUsingAttestation() {
  CHECK(enrollment_config_.is_mode_attestation());
  // The tokens are not used in attestation mode.
  DoEnroll(policy::DMAuth::NoAuth());
}

void EnrollmentLauncherImpl::EnrollUsingEnrollmentToken() {
  CHECK(enrollment_config_.mode ==
        policy::EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED);
  CHECK(!enrollment_config_.enrollment_token.empty());
  DoEnroll(
      policy::DMAuth::FromEnrollmentToken(enrollment_config_.enrollment_token));
}

void EnrollmentLauncherImpl::ClearAuth(base::OnceClosure callback,
                                       bool revoke_oauth2_tokens) {
  if (revoke_oauth2_tokens) {
    RevokeOAuth2Tokens();
  }

  auth_data_ = policy::DMAuth::NoAuth();
  SigninProfileHandler::Get()->ClearSigninProfile(
      base::BindOnce(&EnrollmentLauncherImpl::OnSigninProfileCleared,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void EnrollmentLauncherImpl::RevokeOAuth2Tokens() {
  if (oauth_status_ == OAUTH_NOT_STARTED) {
    return;
  }
  OAuth2TokenRevoker token_revoker;
  if (oauth_fetcher_) {
    if (!oauth_fetcher_->OAuth2AccessToken().empty()) {
      token_revoker.Start(oauth_fetcher_->OAuth2AccessToken());
    }

    if (!oauth_fetcher_->OAuth2RefreshToken().empty()) {
      token_revoker.Start(oauth_fetcher_->OAuth2RefreshToken());
    }

    oauth_fetcher_.reset();
  } else if (auth_data_.has_oauth_token()) {
    // EnrollUsingToken was called.
    token_revoker.Start(auth_data_.oauth_token());
  }
}

void EnrollmentLauncherImpl::DoEnroll(policy::DMAuth auth_data) {
  DCHECK(auth_data_.empty() || auth_data_ == auth_data);
  DCHECK(enrollment_config_.is_mode_attestation() ||
         oauth_status_ == OAUTH_STARTED_WITH_AUTH_CODE ||
         oauth_status_ == OAUTH_STARTED_WITH_TOKEN);

  // Logging as "WARNING" to make sure it's preserved in the logs.
  LOG(WARNING) << "Enroll with token type: "
               << static_cast<int>(auth_data.token_type());
  auth_data_ = std::move(auth_data);
  // If an enrollment domain is already fixed in install attributes and
  // re-enrollment happens via login, domains need to be equal.
  // If there is a mismatch between domain set in install attributes and
  // auto re-enrollment domain provided by the server, policy validation will
  // fail later in the process.
  if (InstallAttributes::Get()->IsCloudManaged() &&
      !enrolling_user_domain_.empty() &&
      !enrollment_config_.is_mode_attestation() &&
      InstallAttributes::Get()->GetDomain() != enrolling_user_domain_) {
    LOG(ERROR) << "Trying to re-enroll to a different domain than "
               << InstallAttributes::Get()->GetDomain();
    UMA(policy::kMetricEnrollmentPrecheckDomainMismatch);
    if (oauth_status_ != OAUTH_NOT_STARTED) {
      oauth_status_ = OAUTH_FINISHED;
    }
    status_consumer_->OnOtherError(OTHER_ERROR_DOMAIN_MISMATCH);
    return;
  }

  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();

  connector->ScheduleServiceInitialization(0);

  DCHECK(!enrollment_handler_);
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  // DeviceDMToken callback is empty here because for device policies this
  // DMToken is already provided in the policy fetch requests.
  auto client = policy::CreateDeviceCloudPolicyClientAsh(
      system::StatisticsProvider::GetInstance(),
      connector->device_management_service(),
      g_browser_process->shared_url_loader_factory(),
      policy::CloudPolicyClient::DeviceDMTokenCallback());

  attestation_flow_ = CreateAttestationFlow();

  enrollment_handler_ = std::make_unique<policy::EnrollmentHandler>(
      policy_manager->device_store(), InstallAttributes::Get(),
      connector->GetStateKeysBroker(), attestation_flow_.get(),
      std::move(client),
      policy::BrowserPolicyConnectorAsh::CreateBackgroundTaskRunner(),
      enrollment_config_, auth_data_.Clone(),
      InstallAttributes::Get()->GetDeviceId(),
      policy::EnrollmentRequisitionManager::GetDeviceRequisition(),
      policy::EnrollmentRequisitionManager::GetSubOrganization(),
      base::BindOnce(&EnrollmentLauncherImpl::OnEnrollmentFinished,
                     weak_ptr_factory_.GetWeakPtr()));

  enrollment_handler_->StartEnrollment();
}

bool EnrollmentLauncherImpl::InProgress() const {
  // `enrollment_handler_` lives from `DoEnroll` till `OnEnrollmentFinished`
  // which covers the whole enrollment process whether it ends with success or
  // failure.
  return enrollment_handler_ != nullptr;
}

std::string EnrollmentLauncherImpl::GetOAuth2RefreshToken() const {
  CHECK(oauth_fetcher_);

  return oauth_fetcher_->OAuth2RefreshToken();
}

void EnrollmentLauncherImpl::GetDeviceAttributeUpdatePermission() {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  policy::CloudPolicyClient* client = policy_manager->core()->client();

  std::optional<policy::DMAuth> auth =
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
      base::BindOnce(&EnrollmentLauncherImpl::OnDeviceAttributeUpdatePermission,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::optional<policy::DMAuth>
EnrollmentLauncherImpl::GetDMAuthForDeviceAttributeUpdate(
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

void EnrollmentLauncherImpl::UpdateDeviceAttributes(
    const std::string& asset_id,
    const std::string& location) {
  policy::BrowserPolicyConnectorAsh* connector =
      g_browser_process->platform_part()->browser_policy_connector_ash();
  policy::DeviceCloudPolicyManagerAsh* policy_manager =
      connector->GetDeviceCloudPolicyManager();
  policy::CloudPolicyClient* client = policy_manager->core()->client();

  std::optional<policy::DMAuth> auth =
      GetDMAuthForDeviceAttributeUpdate(client);

  // If we got here, we must have successfully run
  // GetDeviceAttributeUpdatePermission, which required a non-empty
  // GetDMAuthForDeviceAttributeUpdate result.
  DCHECK(auth.has_value());

  client->UpdateDeviceAttributes(
      std::move(auth.value()), asset_id, location,
      base::BindOnce(&EnrollmentLauncherImpl::OnDeviceAttributeUploadCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentLauncherImpl::OnTokenFetched(
    const std::string& token,
    const GoogleServiceAuthError& error) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    ReportAuthStatus(error);
    oauth_status_ = OAUTH_FINISHED;
    status_consumer_->OnAuthError(error);
    return;
  }

  EnrollUsingToken(token);
}

void EnrollmentLauncherImpl::OnEnrollmentFinished(
    policy::EnrollmentStatus status) {
  // Regardless how enrollment has finished, |enrollment_handler_| is expired.
  // |client| might still be needed to connect the manager.
  auto client = enrollment_handler_->ReleaseClient();
  // Though enrollment handler calls this method, it's "fine" do delete the
  // handler here as it does not do anything after that callback in
  // |EnrollmentHandler::ReportResult()|.
  enrollment_handler_.reset();
  attestation_flow_.reset();

  // Logging as "WARNING" to make sure it's preserved in the logs.
  LOG(WARNING) << "Enrollment finished, code: " << status.enrollment_code();
  ReportEnrollmentStatus(status);
  if (enrollment_config_.mode ==
      policy::EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED) {
    TokenBasedEnrollmentOOBEConfigUMA(status,
                                      enrollment_config_.oobe_config_source);
  }
  if (oauth_status_ != OAUTH_NOT_STARTED) {
    oauth_status_ = OAUTH_FINISHED;
  }

  if (status.enrollment_code() != policy::EnrollmentStatus::Code::kSuccess) {
    status_consumer_->OnEnrollmentError(status);
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
  status_consumer_->OnDeviceEnrolled();
}

void EnrollmentLauncherImpl::OnDeviceAttributeUpdatePermission(bool granted) {
  status_consumer_->OnDeviceAttributeUpdatePermission(granted);
}

void EnrollmentLauncherImpl::OnDeviceAttributeUploadCompleted(bool success) {
  status_consumer_->OnDeviceAttributeUploadCompleted(success);
}

void EnrollmentLauncherImpl::ReportAuthStatus(
    const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::SCOPE_LIMITED_UNRECOVERABLE_ERROR:
    case GoogleServiceAuthError::CHALLENGE_RESPONSE_REQUIRED:
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
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void EnrollmentLauncherImpl::ReportEnrollmentStatus(
    policy::EnrollmentStatus status) {
  switch (status.enrollment_code()) {
    case policy::EnrollmentStatus::Code::kSuccess:
      UMA(policy::kMetricEnrollmentOK);
      return;
    case policy::EnrollmentStatus::Code::kRegistrationFailed:
    case policy::EnrollmentStatus::Code::kPolicyFetchFailed:
      switch (status.client_status()) {
        case policy::DM_STATUS_SUCCESS:
          NOTREACHED_IN_MIGRATION();
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
          NOTREACHED_IN_MIGRATION();
          break;
        case policy::DM_STATUS_SERVICE_ARC_DISABLED:
          NOTREACHED_IN_MIGRATION();
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
    case policy::EnrollmentStatus::Code::kRegistrationBadMode:
      UMA(policy::kMetricEnrollmentInvalidEnrollmentMode);
      break;
    case policy::EnrollmentStatus::Code::kNoStateKeys:
      UMA(policy::kMetricEnrollmentNoStateKeys);
      break;
    case policy::EnrollmentStatus::Code::kValidationFailed:
      UMA(policy::kMetricEnrollmentPolicyValidationFailed);
      break;
    case policy::EnrollmentStatus::Code::kStoreError:
      UMA(policy::kMetricEnrollmentCloudPolicyStoreError);
      break;
    case policy::EnrollmentStatus::Code::kLockError:
      switch (status.lock_status()) {
        case InstallAttributes::LOCK_SUCCESS:
        case InstallAttributes::LOCK_NOT_READY:
          NOTREACHED_IN_MIGRATION();
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
    case policy::EnrollmentStatus::Code::kRobotAuthFetchFailed:
      UMA(policy::kMetricEnrollmentRobotAuthCodeFetchFailed);
      break;
    case policy::EnrollmentStatus::Code::kRobotRefreshFetchFailed:
      UMA(policy::kMetricEnrollmentRobotRefreshTokenFetchFailed);
      break;
    case policy::EnrollmentStatus::Code::kRobotRefreshStoreFailed:
      UMA(policy::kMetricEnrollmentRobotRefreshTokenStoreFailed);
      break;
    case policy::EnrollmentStatus::Code::kAttributeUpdateFailed:
      UMA(policy::kMetricEnrollmentAttributeUpdateFailed);
      break;
    case policy::EnrollmentStatus::Code::kRegistrationCertFetchFailed:
      // Report general attestation-based registration error and granular per
      // attestation failure.
      UMA(policy::kMetricEnrollmentRegistrationCertificateFetchFailed);
      switch (status.attestation_status()) {
        case attestation::ATTESTATION_SUCCESS:
          NOTREACHED_IN_MIGRATION();
          break;
        case attestation::ATTESTATION_UNSPECIFIED_FAILURE:
          UMA(policy::
                  kMetricEnrollmentRegistrationCertificateFetchUnspecifiedFailure);
          break;
        case attestation::ATTESTATION_SERVER_BAD_REQUEST_FAILURE:
          UMA(policy::kMetricEnrollmentRegistrationCertificateFetchBadRequest);
          break;
        case attestation::ATTESTATION_NOT_AVAILABLE:
          UMA(policy::
                  kMetricEnrollmentRegistrationCertificateFetchNotAvailable);
          break;
      }
      break;
    case policy::EnrollmentStatus::Code::kNoMachineIdentification:
      UMA(policy::kMetricEnrollmentNoDeviceIdentification);
      break;
    case policy::EnrollmentStatus::Code::kDmTokenStoreFailed:
      UMA(policy::kMetricEnrollmentStoreDMTokenFailed);
      break;
    case policy::EnrollmentStatus::Code::kMayNotBlockDevMode:
      UMA(policy::kMetricEnrollmentMayNotBlockDevMode);
      break;
  }
}

void EnrollmentLauncherImpl::UMA(policy::MetricEnrollment sample) {
  EnrollmentUMA(sample, enrollment_config_.mode);
}

void EnrollmentLauncherImpl::OnSigninProfileCleared(
    base::OnceClosure callback) {
  oauth_data_cleared_ = true;
  std::move(callback).Run();
}

}  // namespace

EnrollmentLauncher::EnrollmentLauncher() = default;

EnrollmentLauncher::~EnrollmentLauncher() = default;

// static
std::unique_ptr<EnrollmentLauncher> EnrollmentLauncher::Create(
    EnrollmentStatusConsumer* status_consumer,
    const policy::EnrollmentConfig& enrollment_config,
    const std::string& enrolling_user_domain) {
  if (!g_testing_factory->is_null()) {
    CHECK_IS_TEST();
    return g_testing_factory->Run(status_consumer, enrollment_config,
                                  enrolling_user_domain);
  }

  auto result = std::make_unique<EnrollmentLauncherImpl>(status_consumer);
  result->Setup(enrollment_config, enrolling_user_domain);
  return result;
}

ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting::
    ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting(
        EnrollmentLauncher::AttestationFlowFactory testing_factory) {
  CHECK_IS_TEST();
  CHECK(g_testing_attestation_flow_factory->is_null())
      << "Only one override allowed";
  *g_testing_attestation_flow_factory = std::move(testing_factory);
}

ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting::
    ~ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting() {
  g_testing_attestation_flow_factory->Reset();
}

ScopedEnrollmentLauncherFactoryOverrideForTesting::
    ScopedEnrollmentLauncherFactoryOverrideForTesting(
        EnrollmentLauncher::Factory testing_factory) {
  CHECK_IS_TEST();
  CHECK(g_testing_factory->is_null()) << "Only one override allowed";
  Reset(std::move(testing_factory));
}

ScopedEnrollmentLauncherFactoryOverrideForTesting::
    ~ScopedEnrollmentLauncherFactoryOverrideForTesting() {
  g_testing_factory->Reset();
}

void ScopedEnrollmentLauncherFactoryOverrideForTesting::Reset(
    EnrollmentLauncher::Factory testing_factory) {
  *g_testing_factory = std::move(testing_factory);
}

ScopedEnrollmentLauncherFactoryOverrideForTesting&
ScopedEnrollmentLauncherFactoryOverrideForTesting::operator=(
    EnrollmentLauncher::Factory testing_factory) {
  Reset(std::move(testing_factory));
  return *this;
}

}  // namespace ash
