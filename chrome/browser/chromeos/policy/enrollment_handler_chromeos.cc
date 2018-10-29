// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/enrollment_handler_chromeos.h"

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
#include "chromeos/chromeos_switches.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart_client.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Retry for InstallAttrs initialization every 500ms.
const int kLockRetryIntervalMs = 500;
// Maximum time to retry InstallAttrs initialization before we give up.
const int kLockRetryTimeoutMs = 10 * 60 * 1000;  // 10 minutes.

em::DeviceRegisterRequest::Flavor EnrollmentModeToRegistrationFlavor(
    EnrollmentConfig::Mode mode) {
  switch (mode) {
    case EnrollmentConfig::MODE_NONE:
    case EnrollmentConfig::MODE_OFFLINE_DEMO:
      break;
    case EnrollmentConfig::MODE_MANUAL:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL;
    case EnrollmentConfig::MODE_MANUAL_REENROLLMENT:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL_RENEW;
    case EnrollmentConfig::MODE_LOCAL_FORCED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_LOCAL_FORCED;
    case EnrollmentConfig::MODE_LOCAL_ADVERTISED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_LOCAL_ADVERTISED;
    case EnrollmentConfig::MODE_SERVER_FORCED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_SERVER_FORCED;
    case EnrollmentConfig::MODE_SERVER_ADVERTISED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_SERVER_ADVERTISED;
    case EnrollmentConfig::MODE_RECOVERY:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_RECOVERY;
    case EnrollmentConfig::MODE_ATTESTATION:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_ATTESTATION;
    case EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_LOCAL_FORCED;
    case EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_SERVER_FORCED;
    case EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_MANUAL_FALLBACK;
  }

  NOTREACHED() << "Bad enrollment mode: " << mode;
  return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_MANUAL;
}

// Returns whether block_devmode is set.
bool GetBlockdevmodeFromPolicy(
    const enterprise_management::PolicyFetchResponse* policy) {
  DCHECK(policy);
  em::PolicyData policy_data;
  if (!policy_data.ParseFromString(policy->policy_data())) {
    LOG(ERROR) << "Failed to parse policy data";
    return false;
  }

  em::ChromeDeviceSettingsProto payload;
  if (!payload.ParseFromString(policy_data.policy_value())) {
    LOG(ERROR) << "Failed to parse policy value";
    return false;
  }

  bool block_devmode = false;
  if (payload.has_system_settings()) {
    const em::SystemSettingsProto& container = payload.system_settings();
    if (container.has_block_devmode()) {
      block_devmode = container.block_devmode();
    }
  }

  return block_devmode;
}

// A utility funciton of base::ReadFileToString which returns an optional
// string.
// TODO(mukai): move this to base/files.
base::Optional<std::string> ReadFileToOptionalString(
    const base::FilePath& file_path) {
  std::string content;
  base::Optional<std::string> result;
  if (base::ReadFileToString(file_path, &content))
    result = std::move(content);
  return result;
}

// Returns binary config which is encrypted by a password that the joining user
// has to enter.
std::string GetActiveDirectoryDomainJoinConfig(
    const base::DictionaryValue* config) {
  if (!config)
    return std::string();
  const base::Value* base64_value = config->FindKeyOfType(
      "active_directory_domain_join_config", base::Value::Type::STRING);
  if (!base64_value)
    return std::string();
  std::string result;
  if (!base::Base64Decode(base64_value->GetString(), &result)) {
    LOG(ERROR) << "Active Directory config is not base64";
    return std::string();
  }
  return result;
}

}  // namespace

EnrollmentHandlerChromeOS::EnrollmentHandlerChromeOS(
    DeviceCloudPolicyStoreChromeOS* store,
    chromeos::InstallAttributes* install_attributes,
    ServerBackedStateKeysBroker* state_keys_broker,
    chromeos::attestation::AttestationFlow* attestation_flow,
    std::unique_ptr<CloudPolicyClient> client,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    chromeos::ActiveDirectoryJoinDelegate* ad_join_delegate,
    const EnrollmentConfig& enrollment_config,
    std::unique_ptr<DMAuth> dm_auth,
    const std::string& client_id,
    const std::string& requisition,
    const EnrollmentCallback& completion_callback)
    : store_(store),
      install_attributes_(install_attributes),
      state_keys_broker_(state_keys_broker),
      attestation_flow_(attestation_flow),
      client_(std::move(client)),
      background_task_runner_(background_task_runner),
      ad_join_delegate_(ad_join_delegate),
      enrollment_config_(enrollment_config),
      client_id_(client_id),
      requisition_(requisition),
      completion_callback_(completion_callback),
      enrollment_step_(STEP_PENDING),
      weak_ptr_factory_(this) {
  dm_auth_ = std::move(dm_auth);
  CHECK(!client_->is_registered());
  CHECK_EQ(DM_STATUS_SUCCESS, client_->status());
  CHECK((enrollment_config_.is_mode_attestation() ||
         enrollment_config.mode == EnrollmentConfig::MODE_OFFLINE_DEMO) ==
        dm_auth_->empty());
  CHECK_NE(enrollment_config.mode == EnrollmentConfig::MODE_OFFLINE_DEMO,
           enrollment_config.offline_policy_path.empty());
  CHECK(enrollment_config_.auth_mechanism !=
            EnrollmentConfig::AUTH_MECHANISM_ATTESTATION ||
        attestation_flow_);
  store_->AddObserver(this);
  client_->AddObserver(this);
  client_->AddPolicyTypeToFetch(dm_protocol::kChromeDevicePolicyType,
                                std::string());
}

EnrollmentHandlerChromeOS::~EnrollmentHandlerChromeOS() {
  Stop();
  store_->RemoveObserver(this);
}

void EnrollmentHandlerChromeOS::CheckAvailableLicenses(
    const AvailableLicensesCallback& license_callback) {
  CHECK_EQ(STEP_PENDING, enrollment_step_);
  available_licenses_callback_ = license_callback;
  client_->RequestAvailableLicenses(
      dm_auth_->Clone(),
      base::Bind(&EnrollmentHandlerChromeOS::HandleAvailableLicensesResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::HandleAvailableLicensesResult(
    DeviceManagementStatus status,
    const CloudPolicyClient::LicenseMap& license_map) {
  if (status == DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED ||
      status == DM_STATUS_SERVICE_CONSUMER_ACCOUNT_WITH_PACKAGED_LICENSE) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&EnrollmentHandlerChromeOS::ReportResult,
                       weak_ptr_factory_.GetWeakPtr(),
                       EnrollmentStatus::ForRegistrationError(status)));
    return;
  } else if (status != DM_STATUS_SUCCESS) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&EnrollmentHandlerChromeOS::ReportResult,
                              weak_ptr_factory_.GetWeakPtr(),
                              EnrollmentStatus::ForStatus(
                                  EnrollmentStatus::LICENSE_REQUEST_FAILED)));
    return;
  }
  if (available_licenses_callback_)
    available_licenses_callback_.Run(license_map);
}

void EnrollmentHandlerChromeOS::StartEnrollmentWithLicense(
    LicenseType license_type) {
  CHECK_EQ(STEP_PENDING, enrollment_step_);
  CHECK_NE(license_type, ::policy::LicenseType::UNKNOWN);
  switch (license_type) {
    case LicenseType::PERPETUAL:
      license_type_ = ::em::LicenseType::CDM_PERPETUAL;
      break;
    case LicenseType::ANNUAL:
      license_type_ = ::em::LicenseType::CDM_ANNUAL;
      break;
    case LicenseType::KIOSK:
      license_type_ = ::em::LicenseType::KIOSK;
      break;
    case LicenseType::UNKNOWN:
      NOTREACHED();
  }
  StartEnrollment();
}

void EnrollmentHandlerChromeOS::StartEnrollment() {
  CHECK_EQ(STEP_PENDING, enrollment_step_);

  if (enrollment_config_.skip_state_keys_request()) {
    VLOG(1) << "Skipping state keys request.";
    SetStep(STEP_LOADING_STORE);
    StartRegistration();
    return;
  }

  SetStep(STEP_STATE_KEYS);

  if (client_->machine_id().empty()) {
    LOG(ERROR) << "Machine id empty.";
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::NO_MACHINE_IDENTIFICATION));
    return;
  }
  if (client_->machine_model().empty()) {
    LOG(ERROR) << "Machine model empty.";
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::NO_MACHINE_IDENTIFICATION));
    return;
  }

  state_keys_broker_->RequestStateKeys(
      base::Bind(&EnrollmentHandlerChromeOS::HandleStateKeysResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<CloudPolicyClient> EnrollmentHandlerChromeOS::ReleaseClient() {
  Stop();
  return std::move(client_);
}

void EnrollmentHandlerChromeOS::OnPolicyFetched(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);
  CHECK_EQ(STEP_POLICY_FETCH, enrollment_step_);
  SetStep(STEP_VALIDATION);

  // Validate the policy.
  const em::PolicyFetchResponse* policy = client_->GetPolicyFor(
      dm_protocol::kChromeDevicePolicyType, std::string());
  if (!policy) {
    ReportResult(
        EnrollmentStatus::ForFetchError(DM_STATUS_RESPONSE_DECODING_ERROR));
    return;
  }

  // If this is re-enrollment, make sure that the new policy matches the
  // previously-enrolled domain.  (Currently only implemented for cloud
  // management.)
  std::string domain;
  if (install_attributes_->IsCloudManaged())
    domain = install_attributes_->GetDomain();

  auto validator = CreateValidator(
      std::make_unique<em::PolicyFetchResponse>(*policy), domain);

  if (install_attributes_->IsCloudManaged())
    validator->ValidateDomain(domain);
  validator->ValidateDMToken(client->dm_token(),
                             CloudPolicyValidatorBase::DM_TOKEN_REQUIRED);
  DeviceCloudPolicyValidator::StartValidation(
      std::move(validator),
      base::Bind(&EnrollmentHandlerChromeOS::HandlePolicyValidationResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::OnRegistrationStateChanged(
    CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);

  if (enrollment_step_ == STEP_REGISTRATION && client_->is_registered()) {
    device_mode_ = client_->device_mode();
    switch (device_mode_) {
      case DEVICE_MODE_ENTERPRISE:
      case DEVICE_MODE_DEMO:
        // Do nothing.
        break;
      case DEVICE_MODE_ENTERPRISE_AD:
        chromeos::DBusThreadManager::Get()
            ->GetUpstartClient()
            ->StartAuthPolicyService();
        break;
      default:
        LOG(ERROR) << "Supplied device mode is not supported:" << device_mode_;
        ReportResult(EnrollmentStatus::ForStatus(
            EnrollmentStatus::REGISTRATION_BAD_MODE));
        return;
    }
    // Only use DMToken from now on.
    dm_auth_ = DMAuth::FromDMToken(client_->dm_token());
    SetStep(STEP_POLICY_FETCH);
    client_->FetchPolicy();
  } else {
    LOG(FATAL) << "Registration state changed to " << client_->is_registered()
               << " in step " << enrollment_step_ << ".";
  }
}

void EnrollmentHandlerChromeOS::OnClientError(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);

  if (enrollment_step_ == STEP_ROBOT_AUTH_FETCH) {
    LOG(ERROR) << "API authentication code fetch failed: " << client_->status();
    ReportResult(EnrollmentStatus::ForRobotAuthFetchError(client_->status()));
  } else if (enrollment_step_ < STEP_POLICY_FETCH) {
    ReportResult(EnrollmentStatus::ForRegistrationError(client_->status()));
  } else {
    ReportResult(EnrollmentStatus::ForFetchError(client_->status()));
  }
}

void EnrollmentHandlerChromeOS::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);

  if (enrollment_step_ == STEP_LOADING_STORE) {
    // If the |store_| wasn't initialized when StartEnrollment() was called,
    // then StartRegistration() bails silently. This gets registration rolling
    // again after the store finishes loading.
    StartRegistration();
  } else if (enrollment_step_ == STEP_STORE_POLICY) {
    ReportResult(EnrollmentStatus::ForStatus(EnrollmentStatus::SUCCESS));
  }
}

void EnrollmentHandlerChromeOS::OnStoreError(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);
  LOG(ERROR) << "Error in device policy store.";
  ReportResult(EnrollmentStatus::ForStoreError(store_->status(),
                                               store_->validation_status()));
}

void EnrollmentHandlerChromeOS::HandleStateKeysResult(
    const std::vector<std::string>& state_keys) {
  DCHECK_EQ(STEP_STATE_KEYS, enrollment_step_);

  // Make sure state keys are available if forced re-enrollment is on.
  if (chromeos::AutoEnrollmentController::IsFREEnabled()) {
    client_->SetStateKeysToUpload(state_keys);
    current_state_key_ = state_keys_broker_->current_state_key();
    if (state_keys.empty() || current_state_key_.empty()) {
      ReportResult(
          EnrollmentStatus::ForStatus(EnrollmentStatus::NO_STATE_KEYS));
      return;
    }
  }

  SetStep(STEP_LOADING_STORE);
  StartRegistration();
}

void EnrollmentHandlerChromeOS::StartRegistration() {
  DCHECK_EQ(STEP_LOADING_STORE, enrollment_step_);
  if (!store_->is_initialized()) {
    // Do nothing. StartRegistration() will be called again from OnStoreLoaded()
    // after the CloudPolicyStore has initialized.
    return;
  }
  SetStep(STEP_REGISTRATION);
  if (enrollment_config_.is_mode_attestation()) {
    StartAttestationBasedEnrollmentFlow();
  } else if (enrollment_config_.mode == EnrollmentConfig::MODE_OFFLINE_DEMO) {
    StartOfflineDemoEnrollmentFlow();
  } else {
    client_->Register(
        em::DeviceRegisterRequest::DEVICE,
        EnrollmentModeToRegistrationFlavor(enrollment_config_.mode),
        em::DeviceRegisterRequest::LIFETIME_INDEFINITE, license_type_,
        dm_auth_->Clone(), client_id_, requisition_, current_state_key_);
  }
}

void EnrollmentHandlerChromeOS::StartAttestationBasedEnrollmentFlow() {
  const chromeos::attestation::AttestationFlow::CertificateCallback callback =
      base::Bind(
          &EnrollmentHandlerChromeOS::HandleRegistrationCertificateResult,
          weak_ptr_factory_.GetWeakPtr());
  attestation_flow_->GetCertificate(
      chromeos::attestation::PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
      EmptyAccountId(), "" /* request_origin */, false /* force_new_key */,
      callback);
}

void EnrollmentHandlerChromeOS::HandleRegistrationCertificateResult(
    chromeos::attestation::AttestationStatus status,
    const std::string& pem_certificate_chain) {
  if (status == chromeos::attestation::ATTESTATION_SUCCESS) {
    client_->RegisterWithCertificate(
        em::DeviceRegisterRequest::DEVICE,
        EnrollmentModeToRegistrationFlavor(enrollment_config_.mode),
        em::DeviceRegisterRequest::LIFETIME_INDEFINITE, license_type_,
        pem_certificate_chain, client_id_, requisition_, current_state_key_);
  } else {
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::REGISTRATION_CERT_FETCH_FAILED));
  }
}

void EnrollmentHandlerChromeOS::StartOfflineDemoEnrollmentFlow() {
  DCHECK(!enrollment_config_.offline_policy_path.empty());

  device_mode_ = policy::DeviceMode::DEVICE_MODE_DEMO;
  domain_ = enrollment_config_.management_domain;
  skip_robot_auth_ = true;
  SetStep(STEP_POLICY_FETCH);

  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadFileToOptionalString,
                     enrollment_config_.offline_policy_path),
      base::BindOnce(&EnrollmentHandlerChromeOS::OnOfflinePolicyBlobLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::OnOfflinePolicyBlobLoaded(
    base::Optional<std::string> blob) {
  DCHECK_EQ(EnrollmentConfig::MODE_OFFLINE_DEMO, enrollment_config_.mode);
  DCHECK_EQ(STEP_POLICY_FETCH, enrollment_step_);

  if (!blob.has_value()) {
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::OFFLINE_POLICY_LOAD_FAILED));
    return;
  }

  SetStep(STEP_VALIDATION);

  // Validate the policy.
  auto policy = std::make_unique<em::PolicyFetchResponse>();
  if (!policy->ParseFromString(blob.value())) {
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::OFFLINE_POLICY_DECODING_FAILED));
    return;
  }

  // Validate the device policy for the offline demo mode.
  auto validator = CreateValidator(std::move(policy), domain_);
  validator->ValidateDomain(domain_);
  DeviceCloudPolicyValidator::StartValidation(
      std::move(validator),
      base::Bind(&EnrollmentHandlerChromeOS::OnOfflinePolicyValidated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::OnOfflinePolicyValidated(
    DeviceCloudPolicyValidator* validator) {
  DCHECK_EQ(enrollment_config_.mode, EnrollmentConfig::MODE_OFFLINE_DEMO);
  DCHECK_EQ(STEP_VALIDATION, enrollment_step_);

  if (!validator->success()) {
    ReportResult(EnrollmentStatus::ForValidationError(validator->status()));
    return;
  }

  // Don't use the device ID within the validated policy -- it's common among
  // all of the offline-enrolled devices.
  device_id_ = base::GenerateGUID();
  policy_ = std::move(validator->policy());

  // The steps for OAuth2 token fetching is skipped for the OFFLINE_DEMO_MODE.
  SetStep(STEP_SET_FWMP_DATA);
  SetFirmwareManagementParametersData();
}

std::unique_ptr<DeviceCloudPolicyValidator>
EnrollmentHandlerChromeOS::CreateValidator(
    std::unique_ptr<enterprise_management::PolicyFetchResponse> policy,
    const std::string& domain) {
  auto validator = std::make_unique<DeviceCloudPolicyValidator>(
      std::move(policy), background_task_runner_);

  validator->ValidateTimestamp(base::Time(),
                               CloudPolicyValidatorBase::TIMESTAMP_VALIDATED);
  validator->ValidatePolicyType(dm_protocol::kChromeDevicePolicyType);
  validator->ValidatePayload();
  // If |domain| is empty here, the policy validation code will just use the
  // domain from the username field in the policy itself to do key validation.
  // TODO(mnissler): Plumb the enrolling user's username into this object so we
  // can validate the username on the resulting policy, and use the domain from
  // that username to validate the key below (http://crbug.com/343074).
  validator->ValidateInitialKey(domain);
  return validator;
}

void EnrollmentHandlerChromeOS::HandlePolicyValidationResult(
    DeviceCloudPolicyValidator* validator) {
  DCHECK_EQ(STEP_VALIDATION, enrollment_step_);
  if (validator->success()) {
    std::string username = validator->policy_data()->username();
    device_id_ = validator->policy_data()->device_id();
    policy_ = std::move(validator->policy());
    if (device_mode_ == DEVICE_MODE_ENTERPRISE_AD) {
      // Don't use robot account for the Active Directory managed devices.
      skip_robot_auth_ = true;
      SetStep(STEP_AD_DOMAIN_JOIN);
      StartJoinAdDomain();
    } else {
      domain_ = gaia::ExtractDomainName(gaia::CanonicalizeEmail(username));
      SetStep(STEP_ROBOT_AUTH_FETCH);
      client_->FetchRobotAuthCodes(
          dm_auth_->Clone(),
          base::BindOnce(&EnrollmentHandlerChromeOS::OnRobotAuthCodesFetched,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  } else {
    ReportResult(EnrollmentStatus::ForValidationError(validator->status()));
  }
}

void EnrollmentHandlerChromeOS::OnRobotAuthCodesFetched(
    DeviceManagementStatus status,
    const std::string& auth_code) {
  CHECK_EQ(STEP_ROBOT_AUTH_FETCH, enrollment_step_);
  if (status != DM_STATUS_SUCCESS) {
    OnClientError(client_.get());
    return;
  }
  if (auth_code.empty()) {
    // If the server doesn't provide an auth code, skip the robot auth setup.
    // This allows clients running against the test server to transparently skip
    // robot auth.
    skip_robot_auth_ = true;
    SetStep(STEP_AD_DOMAIN_JOIN);
    StartJoinAdDomain();
    return;
  }

  SetStep(STEP_ROBOT_AUTH_REFRESH);
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
void EnrollmentHandlerChromeOS::OnGetTokensResponse(
    const std::string& refresh_token,
    const std::string& access_token,
    int expires_in_seconds) {
  CHECK_EQ(STEP_ROBOT_AUTH_REFRESH, enrollment_step_);

  robot_refresh_token_ = refresh_token;

  SetStep(STEP_AD_DOMAIN_JOIN);
  StartJoinAdDomain();
}

void EnrollmentHandlerChromeOS::SetFirmwareManagementParametersData() {
  DCHECK_EQ(STEP_SET_FWMP_DATA, enrollment_step_);

  // In case of reenrollment, the device has the TPM locked and nothing has to
  // change in install attributes. No need to update firmware parameters in this
  // case.
  if (install_attributes_->IsDeviceLocked()) {
    SetStep(STEP_LOCK_DEVICE);
    StartLockDevice();
    return;
  }

  install_attributes_->SetBlockDevmodeInTpm(
      GetBlockdevmodeFromPolicy(policy_.get()),
      base::BindOnce(
          &EnrollmentHandlerChromeOS::OnFirmwareManagementParametersDataSet,
          weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::OnFirmwareManagementParametersDataSet(
    base::Optional<cryptohome::BaseReply> reply) {
  DCHECK_EQ(STEP_SET_FWMP_DATA, enrollment_step_);
  if (!reply.has_value())
    LOG(ERROR) << "Failed to update firmware management parameters in TPM.";

  SetStep(STEP_LOCK_DEVICE);
  StartLockDevice();
}

// GaiaOAuthClient::Delegate
void EnrollmentHandlerChromeOS::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  // We never use the code that should trigger this callback.
  LOG(FATAL) << "Unexpected callback invoked.";
}

// GaiaOAuthClient::Delegate OAuth2 error when fetching refresh token request.
void EnrollmentHandlerChromeOS::OnOAuthError() {
  CHECK_EQ(STEP_ROBOT_AUTH_REFRESH, enrollment_step_);
  // OnOAuthError is only called if the request is bad (malformed) or the
  // response is bad (empty access token returned).
  LOG(ERROR) << "OAuth protocol error while fetching API refresh token.";
  ReportResult(
      EnrollmentStatus::ForRobotRefreshFetchError(net::HTTP_BAD_REQUEST));
}

// GaiaOAuthClient::Delegate network error when fetching refresh token.
void EnrollmentHandlerChromeOS::OnNetworkError(int response_code) {
  CHECK_EQ(STEP_ROBOT_AUTH_REFRESH, enrollment_step_);
  LOG(ERROR) << "Network error while fetching API refresh token: "
             << response_code;
  ReportResult(EnrollmentStatus::ForRobotRefreshFetchError(response_code));
}

void EnrollmentHandlerChromeOS::StartJoinAdDomain() {
  DCHECK_EQ(STEP_AD_DOMAIN_JOIN, enrollment_step_);
  if (device_mode_ != DEVICE_MODE_ENTERPRISE_AD) {
    SetStep(STEP_SET_FWMP_DATA);
    SetFirmwareManagementParametersData();
    return;
  }
  DCHECK(ad_join_delegate_);
  ad_join_delegate_->JoinDomain(
      client_->dm_token(),
      GetActiveDirectoryDomainJoinConfig(client_->configuration_seed()),
      base::BindOnce(&EnrollmentHandlerChromeOS::OnAdDomainJoined,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::OnAdDomainJoined(const std::string& realm) {
  DCHECK_EQ(STEP_AD_DOMAIN_JOIN, enrollment_step_);
  CHECK(!realm.empty());
  realm_ = realm;
  SetStep(STEP_SET_FWMP_DATA);
  SetFirmwareManagementParametersData();
}

void EnrollmentHandlerChromeOS::StartLockDevice() {
  DCHECK_EQ(STEP_LOCK_DEVICE, enrollment_step_);
  // Since this method is also called directly.
  weak_ptr_factory_.InvalidateWeakPtrs();

  install_attributes_->LockDevice(
      device_mode_, domain_, realm_, device_id_,
      base::Bind(&EnrollmentHandlerChromeOS::HandleLockDeviceResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::HandleDMTokenStoreResult(bool success) {
  CHECK_EQ(STEP_STORE_TOKEN, enrollment_step_);
  if (!success) {
    ReportResult(
        EnrollmentStatus::ForStatus(EnrollmentStatus::DM_TOKEN_STORE_FAILED));
    return;
  }

  StartStoreRobotAuth();
}

void EnrollmentHandlerChromeOS::HandleLockDeviceResult(
    chromeos::InstallAttributes::LockResult lock_result) {
  DCHECK_EQ(STEP_LOCK_DEVICE, enrollment_step_);
  switch (lock_result) {
    case chromeos::InstallAttributes::LOCK_SUCCESS:
      if (device_mode_ == DEVICE_MODE_ENTERPRISE_AD) {
        StartStoreDMToken();
      } else {
        StartStoreRobotAuth();
      }
      break;
    case chromeos::InstallAttributes::LOCK_NOT_READY:
      // We wait up to |kLockRetryTimeoutMs| milliseconds and if it hasn't
      // succeeded by then show an error to the user and stop the enrollment.
      if (lockbox_init_duration_ < kLockRetryTimeoutMs) {
        // InstallAttributes not ready yet, retry later.
        LOG(WARNING) << "Install Attributes not ready yet will retry in "
                     << kLockRetryIntervalMs << "ms.";
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&EnrollmentHandlerChromeOS::StartLockDevice,
                           weak_ptr_factory_.GetWeakPtr()),
            base::TimeDelta::FromMilliseconds(kLockRetryIntervalMs));
        lockbox_init_duration_ += kLockRetryIntervalMs;
      } else {
        HandleLockDeviceResult(chromeos::InstallAttributes::LOCK_TIMEOUT);
      }
      break;
    case chromeos::InstallAttributes::LOCK_TIMEOUT:
    case chromeos::InstallAttributes::LOCK_BACKEND_INVALID:
    case chromeos::InstallAttributes::LOCK_ALREADY_LOCKED:
    case chromeos::InstallAttributes::LOCK_SET_ERROR:
    case chromeos::InstallAttributes::LOCK_FINALIZE_ERROR:
    case chromeos::InstallAttributes::LOCK_READBACK_ERROR:
    case chromeos::InstallAttributes::LOCK_WRONG_DOMAIN:
    case chromeos::InstallAttributes::LOCK_WRONG_MODE:
      ReportResult(EnrollmentStatus::ForLockError(lock_result));
      break;
  }
}

void EnrollmentHandlerChromeOS::StartStoreDMToken() {
  DCHECK(device_mode_ == DEVICE_MODE_ENTERPRISE_AD);
  SetStep(STEP_STORE_TOKEN);
  dm_token_storage_ = std::make_unique<policy::DMTokenStorage>(
      g_browser_process->local_state());
  dm_token_storage_->StoreDMToken(
      client_->dm_token(),
      base::BindOnce(&EnrollmentHandlerChromeOS::HandleDMTokenStoreResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::StartStoreRobotAuth() {
  SetStep(STEP_STORE_ROBOT_AUTH);

  // Don't store the token if robot auth was skipped.
  if (skip_robot_auth_) {
    HandleStoreRobotAuthTokenResult(true);
    return;
  }

  chromeos::DeviceOAuth2TokenServiceFactory::Get()->SetAndSaveRefreshToken(
      robot_refresh_token_,
      base::Bind(&EnrollmentHandlerChromeOS::HandleStoreRobotAuthTokenResult,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandlerChromeOS::HandleStoreRobotAuthTokenResult(bool result) {
  DCHECK_EQ(STEP_STORE_ROBOT_AUTH, enrollment_step_);

  if (!result) {
    LOG(ERROR) << "Failed to store API refresh token.";
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::ROBOT_REFRESH_STORE_FAILED));
    return;
  }

  SetStep(STEP_STORE_POLICY);
  if (device_mode_ == policy::DEVICE_MODE_ENTERPRISE_AD) {
    CHECK(install_attributes_->IsActiveDirectoryManaged());
    // Update device settings so that in case of Active Directory unsigned
    // policy is accepted.
    chromeos::DeviceSettingsService::Get()->SetDeviceMode(
        install_attributes_->GetMode());
    chromeos::DBusThreadManager::Get()
        ->GetAuthPolicyClient()
        ->RefreshDevicePolicy(base::BindOnce(
            &EnrollmentHandlerChromeOS::HandleActiveDirectoryPolicyRefreshed,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    store_->InstallInitialPolicy(*policy_);
  }
}

void EnrollmentHandlerChromeOS::HandleActiveDirectoryPolicyRefreshed(
    authpolicy::ErrorType error) {
  DCHECK_EQ(STEP_STORE_POLICY, enrollment_step_);

  if (error != authpolicy::ERROR_NONE) {
    LOG(ERROR) << "Failed to load Active Directory policy.";
    ReportResult(EnrollmentStatus::ForStatus(
        EnrollmentStatus::ACTIVE_DIRECTORY_POLICY_FETCH_FAILED));
    return;
  }

  // After that, the enrollment flow continues in one of the OnStore* observers.
  store_->Load();
}

void EnrollmentHandlerChromeOS::Stop() {
  if (client_.get())
    client_->RemoveObserver(this);
  SetStep(STEP_FINISHED);
  weak_ptr_factory_.InvalidateWeakPtrs();
  completion_callback_.Reset();
  available_licenses_callback_.Reset();
}

void EnrollmentHandlerChromeOS::ReportResult(EnrollmentStatus status) {
  EnrollmentCallback callback = completion_callback_;
  Stop();

  if (status.status() != EnrollmentStatus::SUCCESS) {
    LOG(WARNING) << "Enrollment failed: " << status.status()
                 << ", client: " << status.client_status()
                 << ", validation: " << status.validation_status()
                 << ", store: " << status.store_status()
                 << ", lock: " << status.lock_status();
  }

  if (!callback.is_null())
    callback.Run(status);
}

void EnrollmentHandlerChromeOS::SetStep(EnrollmentStep step) {
  DCHECK_LE(enrollment_step_, step);
  VLOG(1) << "Step: " << step;
  enrollment_step_ = step;
}

}  // namespace policy
