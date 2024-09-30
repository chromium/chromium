// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_handler.h"

#include <optional>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_dimensions.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_store_ash.h"
#include "chrome/browser/ash/policy/dev_mode/dev_mode_policy_util.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_type_checker.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/ash/policy/enrollment/tpm_enrollment_key_signing_service.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/attestation/attestation_features.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/upstart/upstart_client.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

// An enum for PSM execution result values.
using PsmExecutionResult = em::DeviceRegisterRequest::PsmExecutionResult;

class TpmEnrollmentKeySigningServiceProvider final
    : public EnrollmentHandler::SigningServiceProvider {
 public:
  std::unique_ptr<SigningService> CreateSigningService() const override {
    return std::make_unique<TpmEnrollmentKeySigningService>();
  }
};

// Retry for InstallAttrs initialization every 500ms.
const int kLockRetryIntervalMs = 500;
// Maximum time to retry InstallAttrs initialization before we give up.
const int kLockRetryTimeoutMs = 10 * 60 * 1000;  // 10 minutes.

em::DeviceRegisterRequest::Flavor EnrollmentModeToRegistrationFlavor(
    EnrollmentConfig::Mode mode) {
  switch (mode) {
    case EnrollmentConfig::MODE_NONE:
      NOTREACHED() << "Bad enrollment mode: " << mode;
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
    case EnrollmentConfig::MODE_INITIAL_SERVER_FORCED:
      return em::DeviceRegisterRequest::FLAVOR_ENROLLMENT_INITIAL_SERVER_FORCED;
    case EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_INITIAL_SERVER_FORCED;
    case EnrollmentConfig::MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_INITIAL_MANUAL_FALLBACK;
    case EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_ROLLBACK_FORCED;
    case EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_ATTESTATION_ROLLBACK_MANUAL_FALLBACK;
    case EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED;
    case EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK:
      return em::DeviceRegisterRequest::
          FLAVOR_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK;
  }
}

// Returns the PSM protocol execution result if prefs::kEnrollmentPsmResult is
// set, and its value is within the
// em::DeviceRegisterRequest::PsmExecutionResult enum range. Otherwise,
// std::nullopt.
std::optional<PsmExecutionResult> GetPsmExecutionResult(
    const PrefService& local_state) {
  const PrefService::Preference* has_psm_execution_result_pref =
      local_state.FindPreference(prefs::kEnrollmentPsmResult);

  if (!has_psm_execution_result_pref ||
      has_psm_execution_result_pref->IsDefaultValue() ||
      !has_psm_execution_result_pref->GetValue()->is_int()) {
    return std::nullopt;
  }

  int psm_execution_result =
      has_psm_execution_result_pref->GetValue()->GetInt();

  // Check if the psm_execution_result is a valid value of
  // em::DeviceRegisterRequest::PsmExecutionResult enum.
  if (!em::DeviceRegisterRequest::PsmExecutionResult_IsValid(
          psm_execution_result))
    return std::nullopt;

  // Cast the psm_execution_result integer value to its corresponding enum
  // entry.
  return static_cast<PsmExecutionResult>(psm_execution_result);
}

// Returns the PSM determination timestamp in ms if
// prefs::kEnrollmentPsmDeterminationTime is set. Otherwise, std::nullopt.
std::optional<int64_t> GetPsmDeterminationTimestamp(
    const PrefService& local_state) {
  const PrefService::Preference* has_psm_determination_timestamp_pref =
      local_state.FindPreference(prefs::kEnrollmentPsmDeterminationTime);

  if (!has_psm_determination_timestamp_pref ||
      has_psm_determination_timestamp_pref->IsDefaultValue()) {
    return std::nullopt;
  }

  const base::Time psm_determination_timestamp =
      local_state.GetTime(prefs::kEnrollmentPsmDeterminationTime);

  // The PSM determination timestamp should exist at this stage. Because
  // we already checked the existence of the pref with non-default value.
  DCHECK(!psm_determination_timestamp.is_null());

  return psm_determination_timestamp.InMillisecondsSinceUnixEpoch();
}

}  // namespace

EnrollmentHandler::EnrollmentHandler(
    DeviceCloudPolicyStoreAsh* store,
    ash::InstallAttributes* install_attributes,
    ServerBackedStateKeysBroker* state_keys_broker,
    ash::attestation::AttestationFlow* attestation_flow,
    std::unique_ptr<CloudPolicyClient> client,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const EnrollmentConfig& enrollment_config,
    DMAuth dm_auth,
    const std::string& client_id,
    const std::string& requisition,
    const std::string& sub_organization,
    EnrollmentCallback completion_callback)
    : store_(store),
      install_attributes_(install_attributes),
      state_keys_broker_(state_keys_broker),
      attestation_flow_(attestation_flow),
      signing_service_provider_(
          std::make_unique<TpmEnrollmentKeySigningServiceProvider>()),
      client_(std::move(client)),
      background_task_runner_(background_task_runner),
      enrollment_config_(enrollment_config),
      client_id_(client_id),
      sub_organization_(sub_organization),
      completion_callback_(std::move(completion_callback)),
      enrollment_step_(STEP_PENDING) {
  dm_auth_ = std::move(dm_auth);
  CHECK(!client_->is_registered());
  CHECK_EQ(DM_STATUS_SUCCESS, client_->last_dm_status());
  CHECK_EQ(dm_auth_.empty(), enrollment_config_.is_mode_attestation());
  CHECK(enrollment_config_.is_mode_attestation() || attestation_flow_);
  register_params_ =
      std::make_unique<CloudPolicyClient::RegistrationParameters>(
          em::DeviceRegisterRequest::DEVICE,
          EnrollmentModeToRegistrationFlavor(enrollment_config.mode));
  register_params_->psm_execution_result =
      GetPsmExecutionResult(*g_browser_process->local_state());
  register_params_->psm_determination_timestamp =
      GetPsmDeterminationTimestamp(*g_browser_process->local_state());
  // License type is set only if terminal license is used. Unset field is
  // treated as enterprise license.
  if (enrollment_config_.license_type == LicenseType::kTerminal) {
    register_params_->license_type =
        em::LicenseType_LicenseTypeEnum::LicenseType_LicenseTypeEnum_KIOSK;
  }

  register_params_->requisition = requisition;

  if (requisition == EnrollmentRequisitionManager::kDemoRequisition) {
    register_params_->demo_mode_dimensions =
        ash::demo_mode::GetDemoModeDimensions();
  }

  store_->AddObserver(this);
  client_->AddObserver(this);
  client_->AddPolicyTypeToFetch(dm_protocol::kChromeDevicePolicyType,
                                std::string());
}

EnrollmentHandler::~EnrollmentHandler() {
  Stop();
  store_->RemoveObserver(this);
}

void EnrollmentHandler::SetSigningServiceProviderForTesting(
    std::unique_ptr<SigningServiceProvider> signing_service_provider) {
  signing_service_provider_ = std::move(signing_service_provider);
}

void EnrollmentHandler::StartEnrollment() {
  CHECK_EQ(STEP_PENDING, enrollment_step_);

  SetStep(STEP_STATE_KEYS);

  if (client_->machine_id().empty()) {
    LOG(ERROR) << "Machine id empty.";
    ReportResult(EnrollmentStatus::ForEnrollmentCode(
        EnrollmentStatus::Code::kNoMachineIdentification));
    return;
  }
  if (client_->machine_model().empty()) {
    LOG(ERROR) << "Machine model empty.";
    ReportResult(EnrollmentStatus::ForEnrollmentCode(
        EnrollmentStatus::Code::kNoMachineIdentification));
    return;
  }

  // Request state keys if FRE is enabled.
  if (AutoEnrollmentTypeChecker::IsFREEnabled()) {
    LOG(WARNING) << "Requesting state keys.";
    state_keys_broker_->RequestStateKeys(base::BindOnce(
        // This simply allows the keys to be wrapped in an std::optional, which
        // the compiler can't do if we just bind
        // &EnrollmentHandler::HandleStateKeys.
        [](base::WeakPtr<EnrollmentHandler> self,
           const std::vector<std::string>& keys) {
          if (self) {
            self->HandleStateKeys(keys);
          }
        },
        weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Skip the request for state keys, but handle their absence and move to the
    // next step.
    LOG(WARNING) << "Skipping state keys.";
    HandleStateKeys(std::nullopt);
  }
}

std::unique_ptr<CloudPolicyClient> EnrollmentHandler::ReleaseClient() {
  Stop();
  return std::move(client_);
}

void EnrollmentHandler::OnPolicyFetched(CloudPolicyClient* client) {
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
      base::BindOnce(&EnrollmentHandler::HandlePolicyValidationResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandler::OnRegistrationStateChanged(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);

  if (enrollment_step_ != STEP_REGISTRATION || !client_->is_registered()) {
    LOG(FATAL) << "Registration state changed to " << client_->is_registered()
               << " in step " << enrollment_step_ << ".";
  }

  device_mode_ = client_->device_mode();

  switch (device_mode_) {
    case DEVICE_MODE_ENTERPRISE:
    case DEVICE_MODE_DEMO:
      // Do nothing.
      break;
    default:
      LOG(ERROR) << "Supplied device mode is not supported:" << device_mode_;
      ReportResult(EnrollmentStatus::ForEnrollmentCode(
          EnrollmentStatus::Code::kRegistrationBadMode));
      return;
  }
  // Only use DMToken from now on.
  dm_auth_ = DMAuth::FromDMToken(client_->dm_token());
  SetStep(STEP_POLICY_FETCH);
  client_->FetchPolicy(PolicyFetchReason::kDeviceEnrollment);
}

void EnrollmentHandler::OnClientError(CloudPolicyClient* client) {
  DCHECK_EQ(client_.get(), client);

  if (enrollment_step_ == STEP_ROBOT_AUTH_FETCH ||
      enrollment_step_ == STEP_STORE_ROBOT_AUTH) {
    // Handled in OnDeviceAccountTokenError().
    return;
  }

  if (enrollment_step_ < STEP_POLICY_FETCH) {
    ReportResult(
        EnrollmentStatus::ForRegistrationError(client_->last_dm_status()));
  } else {
    ReportResult(EnrollmentStatus::ForFetchError(client_->last_dm_status()));
  }
}

void EnrollmentHandler::OnStoreLoaded(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);

  if (enrollment_step_ == STEP_LOADING_STORE) {
    // If the |store_| wasn't initialized when StartEnrollment() was called,
    // then StartRegistration() bails silently. This gets registration rolling
    // again after the store finishes loading.
    StartRegistration();
  } else if (enrollment_step_ == STEP_STORE_POLICY) {
    ReportResult(
        EnrollmentStatus::ForEnrollmentCode(EnrollmentStatus::Code::kSuccess));
  }
}

void EnrollmentHandler::OnStoreError(CloudPolicyStore* store) {
  DCHECK_EQ(store_, store);

  if (enrollment_step_ < STEP_STORE_POLICY) {
    // At those steps it is not expected to have any error notifications from
    // |store_| since they are not initiated by enrollment handler and stored
    // policies are not in a consistent state (e.g. a late response from
    // |store_| loaded at boot). So the notification is ignored.
    // Notifications are only expected starting STEP_STORE_POLICY
    // when OnDeviceAccountTokenStored() is called.
    LOG(WARNING) << "Unexpected store error with status: " << store->status()
                 << " at step: " << enrollment_step_;
    return;
  }

  LOG(ERROR) << "Error in device policy store.";
  ReportResult(EnrollmentStatus::ForStoreError(store_->status(),
                                               store_->validation_status()));
}

void EnrollmentHandler::HandleStateKeys(
    std::optional<std::vector<std::string>> opt_state_keys) {
  DCHECK_EQ(STEP_STATE_KEYS, enrollment_step_);

  if (opt_state_keys.has_value()) {
    auto state_keys = opt_state_keys.value();

    client_->SetStateKeysToUpload(state_keys);
    register_params_->current_state_key =
        state_keys_broker_->current_state_key();
    if (state_keys.empty() || register_params_->current_state_key.empty()) {
      LOG(ERROR) << "State keys empty.";
      ReportResult(EnrollmentStatus::ForEnrollmentCode(
          EnrollmentStatus::Code::kNoStateKeys));
      return;
    }

    // Logging as "WARNING" to make sure it's preserved in the logs.
    LOG(WARNING) << "State keys generated, success=" << !state_keys.empty();
  } else {
    LOG(WARNING) << "State keys are not used.";
  }

  SetStep(STEP_LOADING_STORE);
  StartRegistration();
}

void EnrollmentHandler::StartRegistration() {
  DCHECK_EQ(STEP_LOADING_STORE, enrollment_step_);
  if (!store_->is_initialized()) {
    // Do nothing. StartRegistration() will be called again from OnStoreLoaded()
    // after the CloudPolicyStore has initialized.
    return;
  }

  // TODO(crbug.com/40805389): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Start registration, config mode = "
               << enrollment_config_.mode;

  SetStep(STEP_REGISTRATION);
  if (enrollment_config_.is_mode_attestation()) {
    StartAttestationBasedEnrollmentFlow();
  } else if (enrollment_config_.mode ==
             EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED) {
    client_->RegisterDeviceWithEnrollmentToken(*register_params_, client_id_,
                                               dm_auth_.Clone());
  } else {
    client_->Register(*register_params_, client_id_, dm_auth_.oauth_token());
  }
}

void EnrollmentHandler::StartAttestationBasedEnrollmentFlow() {
  ash::attestation::AttestationFlow::CertificateCallback callback =
      base::BindOnce(&EnrollmentHandler::HandleRegistrationCertificateResult,
                     weak_ptr_factory_.GetWeakPtr());
  ash::attestation::AttestationFeatures::GetFeatures(
      base::BindOnce(&EnrollmentHandler::OnGetFeaturesReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void EnrollmentHandler::OnGetFeaturesReady(
    ash::attestation::AttestationFlow::CertificateCallback callback,
    const ash::attestation::AttestationFeatures* features) {
  if (!features) {
    LOG(ERROR) << "Failed to get AttestationFeatures.";
    std::move(callback).Run(ash::attestation::ATTESTATION_UNSPECIFIED_FAILURE,
                            "");
    return;
  }
  if (!features->IsAttestationAvailable()) {
    LOG(ERROR) << "The Attestation is not available.";
    std::move(callback).Run(ash::attestation::ATTESTATION_UNSPECIFIED_FAILURE,
                            "");
    return;
  }

  // prefers ECC certificate if available
  ::attestation::KeyType key_crypto_type;
  if (features->IsEccSupported()) {
    key_crypto_type = ::attestation::KEY_TYPE_ECC;
  } else if (features->IsRsaSupported()) {
    key_crypto_type = ::attestation::KEY_TYPE_RSA;
  } else {
    LOG(ERROR) << "No appropriate crypto key type supported.";
    std::move(callback).Run(ash::attestation::ATTESTATION_UNSPECIFIED_FAILURE,
                            "");
    return;
  }

  // Always force a new key to obtain a fresh certificate. See crbug.com/1292897
  // for context.
  attestation_flow_->GetCertificate(
      /*certificate_profile=*/ash::attestation::
          PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/std::string(),
      /*force_new_key=*/true,
      /*key_crypto_type=*/key_crypto_type,
      /*key_name=*/ash::attestation::kEnterpriseEnrollmentKey,
      /*profile_specific_data=*/std::nullopt,
      /*callback=*/std::move(callback));
}

void EnrollmentHandler::HandleRegistrationCertificateResult(
    ash::attestation::AttestationStatus status,
    const std::string& pem_certificate_chain) {
  if (status != ash::attestation::ATTESTATION_SUCCESS) {
    ReportResult(EnrollmentStatus::ForAttestationError(status));
    return;
  }

  client_->RegisterWithCertificate(
      *register_params_, client_id_, pem_certificate_chain, sub_organization_,
      signing_service_provider_->CreateSigningService());
}

std::unique_ptr<DeviceCloudPolicyValidator> EnrollmentHandler::CreateValidator(
    std::unique_ptr<em::PolicyFetchResponse> policy,
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

void EnrollmentHandler::HandlePolicyValidationResult(
    DeviceCloudPolicyValidator* validator) {
  DCHECK_EQ(STEP_VALIDATION, enrollment_step_);
  if (!validator->success()) {
    ReportResult(EnrollmentStatus::ForValidationError(validator->status()));
    return;
  }
  std::string username = validator->policy_data()->username();
  device_id_ = validator->policy_data()->device_id();
  policy_ = std::move(validator->policy());

  if (GetDeviceBlockDevModePolicyValue(*policy_) &&
      !IsDeviceBlockDevModePolicyAllowed()) {
    ReportResult(EnrollmentStatus::ForEnrollmentCode(
        EnrollmentStatus::Code::kMayNotBlockDevMode));
    return;
  }

  domain_ = gaia::ExtractDomainName(gaia::CanonicalizeEmail(username));
  SetStep(STEP_ROBOT_AUTH_FETCH);
  device_account_initializer_ =
      std::make_unique<DeviceAccountInitializer>(client_.get(), this);
  device_account_initializer_->FetchToken();
}

void EnrollmentHandler::OnDeviceAccountTokenFetched(bool empty_token) {
  CHECK_EQ(STEP_ROBOT_AUTH_FETCH, enrollment_step_);
  skip_robot_auth_ = empty_token;
  SetStep(STEP_SET_FWMP_DATA);
  SetFirmwareManagementParametersData();
}

void EnrollmentHandler::OnDeviceAccountTokenFetchError(
    std::optional<DeviceManagementStatus> dm_status) {
  CHECK_EQ(enrollment_step_, STEP_ROBOT_AUTH_FETCH);
  if (dm_status.has_value()) {
    ReportResult(EnrollmentStatus::ForRobotAuthFetchError(dm_status.value()));
  } else {
    ReportResult(EnrollmentStatus::ForEnrollmentCode(
        EnrollmentStatus::Code::kRobotRefreshFetchFailed));
  }
}

void EnrollmentHandler::OnDeviceAccountTokenStoreError() {
  CHECK_EQ(enrollment_step_, STEP_STORE_ROBOT_AUTH);
  ReportResult(EnrollmentStatus::ForEnrollmentCode(
      EnrollmentStatus::Code::kRobotRefreshStoreFailed));
}

void EnrollmentHandler::OnDeviceAccountClientError(
    DeviceManagementStatus status) {
  // Do nothing, it would be handled in OnClientError.
}

em::DeviceServiceApiAccessRequest::DeviceType
EnrollmentHandler::GetRobotAuthCodeDeviceType() {
  return em::DeviceServiceApiAccessRequest::CHROME_OS;
}

std::set<std::string> EnrollmentHandler::GetRobotOAuthScopes() {
  return {GaiaConstants::kAnyApiOAuth2Scope};
}

scoped_refptr<network::SharedURLLoaderFactory>
EnrollmentHandler::GetURLLoaderFactory() {
  return g_browser_process->shared_url_loader_factory();
}

void EnrollmentHandler::SetFirmwareManagementParametersData() {
  DCHECK_EQ(STEP_SET_FWMP_DATA, enrollment_step_);

  // In case of reenrollment, the device has the TPM locked and nothing has to
  // change in install attributes. No need to update firmware parameters in this
  // case.
  if (install_attributes_->IsDeviceLocked()) {
    SetStep(STEP_LOCK_DEVICE);
    StartLockDevice();
    return;
  }

  const bool block_devmode = GetDeviceBlockDevModePolicyValue(*policy_);
  // TODO(crbug.com/40805389): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << (block_devmode ? "Blocking" : "Allowing")
               << " dev mode by device policy";
  install_attributes_->SetBlockDevmodeInTpm(
      block_devmode,
      base::BindOnce(&EnrollmentHandler::OnFirmwareManagementParametersDataSet,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandler::OnFirmwareManagementParametersDataSet(
    std::optional<device_management::SetFirmwareManagementParametersReply>
        reply) {
  DCHECK_EQ(STEP_SET_FWMP_DATA, enrollment_step_);
  if (!reply.has_value()) {
    LOG(ERROR) << "Failed to update firmware management parameters in TPM due "
                  "to DBus error.";
  } else if (reply->error() != device_management::DeviceManagementErrorCode::
                                   DEVICE_MANAGEMENT_ERROR_NOT_SET) {
    LOG(ERROR) << "Failed to update firmware management parameters in TPM, "
                  "error code: "
               << static_cast<int>(reply->error());
  }

  SetStep(STEP_LOCK_DEVICE);
  StartLockDevice();
}

void EnrollmentHandler::StartLockDevice() {
  DCHECK_EQ(STEP_LOCK_DEVICE, enrollment_step_);
  // Since this method is also called directly.
  weak_ptr_factory_.InvalidateWeakPtrs();

  install_attributes_->LockDevice(
      device_mode_, domain_, realm_, device_id_,
      base::BindOnce(&EnrollmentHandler::HandleLockDeviceResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentHandler::HandleLockDeviceResult(
    ash::InstallAttributes::LockResult lock_result) {
  DCHECK_EQ(STEP_LOCK_DEVICE, enrollment_step_);
  switch (lock_result) {
    case ash::InstallAttributes::LOCK_SUCCESS:
      StartStoreRobotAuth();
      break;
    case ash::InstallAttributes::LOCK_NOT_READY:
      // We wait up to |kLockRetryTimeoutMs| milliseconds and if it hasn't
      // succeeded by then show an error to the user and stop the enrollment.
      if (lockbox_init_duration_ < kLockRetryTimeoutMs) {
        // InstallAttributes not ready yet, retry later.
        LOG(WARNING) << "Install Attributes not ready yet will retry in "
                     << kLockRetryIntervalMs << "ms.";
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&EnrollmentHandler::StartLockDevice,
                           weak_ptr_factory_.GetWeakPtr()),
            base::Milliseconds(kLockRetryIntervalMs));
        lockbox_init_duration_ += kLockRetryIntervalMs;
      } else {
        HandleLockDeviceResult(ash::InstallAttributes::LOCK_TIMEOUT);
      }
      break;
    case ash::InstallAttributes::LOCK_TIMEOUT:
    case ash::InstallAttributes::LOCK_BACKEND_INVALID:
    case ash::InstallAttributes::LOCK_ALREADY_LOCKED:
    case ash::InstallAttributes::LOCK_SET_ERROR:
    case ash::InstallAttributes::LOCK_FINALIZE_ERROR:
    case ash::InstallAttributes::LOCK_READBACK_ERROR:
    case ash::InstallAttributes::LOCK_WRONG_DOMAIN:
    case ash::InstallAttributes::LOCK_WRONG_MODE:
      ReportResult(EnrollmentStatus::ForLockError(lock_result));
      break;
  }
}

void EnrollmentHandler::StartStoreRobotAuth() {
  SetStep(STEP_STORE_ROBOT_AUTH);

  // Don't store the token if robot auth was skipped.
  if (skip_robot_auth_) {
    OnDeviceAccountTokenStored();
    return;
  }
  device_account_initializer_->StoreToken();
}

void EnrollmentHandler::StoreVersion() {
  DCHECK_EQ(STEP_STORE_VERSION, enrollment_step_);
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetString(prefs::kEnrollmentVersionOS,
                   base::SysInfo::OperatingSystemVersion());
  prefs->SetString(prefs::kEnrollmentVersionBrowser,
                   version_info::GetVersionNumber());
  prefs->CommitPendingWrite();

  SetStep(STEP_STORE_POLICY);
  StartStoreDevicePolicy();
}

void EnrollmentHandler::StartStoreDevicePolicy() {
  DCHECK_EQ(STEP_STORE_POLICY, enrollment_step_);
  store_->InstallInitialPolicy(*policy_);
}

void EnrollmentHandler::OnDeviceAccountTokenStored() {
  DCHECK_EQ(STEP_STORE_ROBOT_AUTH, enrollment_step_);
  SetStep(STEP_STORE_VERSION);
  StoreVersion();
}

void EnrollmentHandler::Stop() {
  if (client_.get())
    client_->RemoveObserver(this);
  if (device_account_initializer_.get()) {
    device_account_initializer_->Stop();
    device_account_initializer_.reset();
  }
  SetStep(STEP_FINISHED);
  weak_ptr_factory_.InvalidateWeakPtrs();
  completion_callback_.Reset();
}

void EnrollmentHandler::ReportResult(EnrollmentStatus status) {
  EnrollmentCallback callback = std::move(completion_callback_);
  Stop();

  if (status.enrollment_code() != EnrollmentStatus::Code::kSuccess) {
    LOG(WARNING) << "Enrollment failed: " << status.enrollment_code()
                 << ", client: " << status.client_status()
                 << ", validation: " << status.validation_status()
                 << ", store: " << status.store_status()
                 << ", lock: " << status.lock_status();
  }

  if (!callback.is_null())
    std::move(callback).Run(status);
}

void EnrollmentHandler::SetStep(EnrollmentStep step) {
  DCHECK_LE(enrollment_step_, step);

  // TODO(crbug.com/40805389): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Step: " << step;

  enrollment_step_ = step;
}

}  // namespace policy
