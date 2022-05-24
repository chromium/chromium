// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client_impl.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_dmserver_client.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;
namespace psm_rlwe = ::private_membership::rlwe;
using EnrollmentCheckType =
    em::DeviceAutoEnrollmentRequest::EnrollmentCheckType;

// Returns the power of the next power-of-2 starting at |value|.
int NextPowerOf2(int64_t value) {
  for (int i = 0; i <= AutoEnrollmentClient::kMaximumPower; ++i) {
    if ((INT64_C(1) << i) >= value)
      return i;
  }
  // No other value can be represented in an int64_t.
  return AutoEnrollmentClient::kMaximumPower + 1;
}

// Sets or clears a value in a dictionary.
void UpdateDict(base::Value* dict,
                const char* pref_path,
                bool set_or_clear,
                std::unique_ptr<base::Value> value) {
  if (set_or_clear)
    dict->SetPath(pref_path, base::Value::FromUniquePtrValue(std::move(value)));
  else
    dict->RemoveKey(pref_path);
}

// Converts a restore mode enum value from the DM protocol for FRE into the
// corresponding prefs string constant.
std::string ConvertRestoreMode(
    em::DeviceStateRetrievalResponse::RestoreMode restore_mode) {
  switch (restore_mode) {
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE:
      return std::string();
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_REQUESTED:
      return kDeviceStateRestoreModeReEnrollmentRequested;
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ENFORCED:
      return kDeviceStateRestoreModeReEnrollmentEnforced;
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_DISABLED:
      return kDeviceStateModeDisabled;
    case em::DeviceStateRetrievalResponse::RESTORE_MODE_REENROLLMENT_ZERO_TOUCH:
      return kDeviceStateRestoreModeReEnrollmentZeroTouch;
  }

  // Return is required to avoid compiler warning.
  NOTREACHED() << "Bad restore_mode=" << restore_mode << ".";
  return std::string();
}

// Converts an initial enrollment mode enum value from the DM protocol for
// initial enrollment into the corresponding prefs string constant.
std::string ConvertInitialEnrollmentMode(
    em::DeviceInitialEnrollmentStateResponse::InitialEnrollmentMode
        initial_enrollment_mode) {
  switch (initial_enrollment_mode) {
    case em::DeviceInitialEnrollmentStateResponse::INITIAL_ENROLLMENT_MODE_NONE:
      return std::string();
    case em::DeviceInitialEnrollmentStateResponse::
        INITIAL_ENROLLMENT_MODE_ENROLLMENT_ENFORCED:
      return kDeviceStateInitialModeEnrollmentEnforced;
    case em::DeviceInitialEnrollmentStateResponse::
        INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED:
      return kDeviceStateInitialModeEnrollmentZeroTouch;
    case em::DeviceInitialEnrollmentStateResponse::
        INITIAL_ENROLLMENT_MODE_DISABLED:
      return kDeviceStateModeDisabled;
  }
}

// Converts a license packaging sku enum value from the DM protocol for initial
// enrollment into the corresponding prefs string constant.
std::string ConvertLicenseType(
    em::DeviceInitialEnrollmentStateResponse::LicensePackagingSKU license_sku) {
  switch (license_sku) {
    case em::DeviceInitialEnrollmentStateResponse::NOT_EXIST:
      return std::string();
    case em::DeviceInitialEnrollmentStateResponse::CHROME_ENTERPRISE:
      return kDeviceStateLicenseTypeEnterprise;
    case em::DeviceInitialEnrollmentStateResponse::CHROME_EDUCATION:
      return kDeviceStateLicenseTypeEducation;
    case em::DeviceInitialEnrollmentStateResponse::CHROME_TERMINAL:
      return kDeviceStateLicenseTypeTerminal;
  }

  // Return is required to avoid compiler warning.
  NOTREACHED() << "Bad license_sku=" << license_sku << ".";
  return std::string();
}

}  // namespace

class AutoEnrollmentClientImpl::DeviceIdentifierProviderFRE {
 public:
  explicit DeviceIdentifierProviderFRE(
      const std::string& server_backed_state_key) {
    CHECK(!server_backed_state_key.empty());
    server_backed_state_key_hash_ =
        crypto::SHA256HashString(server_backed_state_key);
  }

  // Disallow copy constructor and assignment operator.
  DeviceIdentifierProviderFRE(const DeviceIdentifierProviderFRE&) = delete;
  DeviceIdentifierProviderFRE& operator=(const DeviceIdentifierProviderFRE&) =
      delete;

  ~DeviceIdentifierProviderFRE() = default;

  // Should return the EnrollmentCheckType to be used in the
  // DeviceAutoEnrollmentRequest. This specifies the identifier set used on
  // the server.
  EnrollmentCheckType GetEnrollmentCheckType() const {
    return em::DeviceAutoEnrollmentRequest::ENROLLMENT_CHECK_TYPE_FRE;
  }

  // Should return the hash of this device's identifier. The
  // DeviceAutoEnrollmentRequest exchange will check if this hash is in the
  // server-side identifier set specified by |GetEnrollmentCheckType()|
  const std::string& GetIdHash() const { return server_backed_state_key_hash_; }

 private:
  // SHA-256 digest of the stable identifier.
  std::string server_backed_state_key_hash_;
};

// Subclasses of this class generate the request to download the device state
// (after determining that there is server-side device state) and parse the
// response.
class AutoEnrollmentClientImpl::StateDownloadMessageProcessor {
 public:
  virtual ~StateDownloadMessageProcessor() {}

  // Parsed fields of DeviceManagementResponse.
  struct ParsedResponse {
    std::string restore_mode;
    absl::optional<std::string> management_domain;
    absl::optional<std::string> disabled_message;
    absl::optional<bool> is_license_packaged_with_device;
    absl::optional<std::string> license_type;
  };

  // Returns the request job type. This must match the request filled in
  // |FillRequest|.
  virtual DeviceManagementService::JobConfiguration::JobType GetJobType()
      const = 0;

  // Fills the specific request type in |request|.
  virtual void FillRequest(em::DeviceManagementRequest* request) = 0;

  // Parses the |response|. If it is valid, returns a ParsedResponse struct
  // instance. If it is invalid, returns nullopt.
  virtual absl::optional<ParsedResponse> ParseResponse(
      const em::DeviceManagementResponse& response) = 0;
};

namespace {
// Handles DeviceInitialEnrollmentStateRequest /
// DeviceInitialEnrollmentStateResponse for Forced Initial Enrollment.
class StateDownloadMessageProcessorInitialEnrollment
    : public AutoEnrollmentClientImpl::StateDownloadMessageProcessor {
 public:
  StateDownloadMessageProcessorInitialEnrollment(
      const std::string& device_serial_number,
      const std::string& device_brand_code)
      : device_serial_number_(device_serial_number),
        device_brand_code_(device_brand_code) {}

  DeviceManagementService::JobConfiguration::JobType GetJobType()
      const override {
    return DeviceManagementService::JobConfiguration::
        TYPE_INITIAL_ENROLLMENT_STATE_RETRIEVAL;
  }

  void FillRequest(em::DeviceManagementRequest* request) override {
    auto* inner_request =
        request->mutable_device_initial_enrollment_state_request();
    inner_request->set_brand_code(device_brand_code_);
    inner_request->set_serial_number(device_serial_number_);
  }

  absl::optional<ParsedResponse> ParseResponse(
      const em::DeviceManagementResponse& response) override {
    if (!response.has_device_initial_enrollment_state_response()) {
      LOG(ERROR) << "Server failed to provide initial enrollment response.";
      return absl::nullopt;
    }

    return ParseInitialEnrollmentStateResponse(
        response.device_initial_enrollment_state_response());
  }

  static absl::optional<ParsedResponse> ParseInitialEnrollmentStateResponse(
      const em::DeviceInitialEnrollmentStateResponse& state_response) {
    StateDownloadMessageProcessor::ParsedResponse parsed_response;

    if (state_response.has_initial_enrollment_mode()) {
      parsed_response.restore_mode = ConvertInitialEnrollmentMode(
          state_response.initial_enrollment_mode());
    } else {
      // Unknown initial enrollment mode - treat as no enrollment.
      parsed_response.restore_mode.clear();
    }

    if (state_response.has_management_domain())
      parsed_response.management_domain = state_response.management_domain();

    if (state_response.has_is_license_packaged_with_device()) {
      parsed_response.is_license_packaged_with_device =
          state_response.is_license_packaged_with_device();
    }

    if (state_response.has_license_packaging_sku()) {
      parsed_response.license_type =
          ConvertLicenseType(state_response.license_packaging_sku());
    }

    if (state_response.has_disabled_state()) {
      parsed_response.disabled_message =
          state_response.disabled_state().message();
    }

    // Logging as "WARNING" to make sure it's preserved in the logs.
    LOG(WARNING) << "Received initial_enrollment_mode="
                 << state_response.initial_enrollment_mode() << " ("
                 << parsed_response.restore_mode << "). "
                 << (state_response.is_license_packaged_with_device()
                         ? "Device has a packaged license for management."
                         : "No packaged license.");

    return parsed_response;
  }

 private:
  // Serial number of the device.
  std::string device_serial_number_;
  // 4-character brand code of the device.
  std::string device_brand_code_;
};

// Handles DeviceStateRetrievalRequest / DeviceStateRetrievalResponse for
// Forced Re-Enrollment (FRE).
class StateDownloadMessageProcessorFRE
    : public AutoEnrollmentClientImpl::StateDownloadMessageProcessor {
 public:
  explicit StateDownloadMessageProcessorFRE(
      const std::string& server_backed_state_key)
      : server_backed_state_key_(server_backed_state_key) {}

  DeviceManagementService::JobConfiguration::JobType GetJobType()
      const override {
    return DeviceManagementService::JobConfiguration::
        TYPE_DEVICE_STATE_RETRIEVAL;
  }

  void FillRequest(em::DeviceManagementRequest* request) override {
    request->mutable_device_state_retrieval_request()
        ->set_server_backed_state_key(server_backed_state_key_);
  }

  absl::optional<ParsedResponse> ParseResponse(
      const em::DeviceManagementResponse& response) override {
    if (!response.has_device_state_retrieval_response()) {
      LOG(ERROR) << "Server failed to provide auto-enrollment response.";
      return absl::nullopt;
    }

    const em::DeviceStateRetrievalResponse& state_response =
        response.device_state_retrieval_response();
    const auto restore_mode = state_response.restore_mode();

    if (restore_mode == em::DeviceStateRetrievalResponse::RESTORE_MODE_NONE &&
        state_response.has_initial_state_response()) {
      // Logging as "WARNING" to make sure it's preserved in the logs.
      LOG(WARNING) << "Received restore_mode=" << restore_mode << " ("
                   << ConvertRestoreMode(restore_mode) << ")"
                   << " . Parsing included initial state response.";

      return StateDownloadMessageProcessorInitialEnrollment::
          ParseInitialEnrollmentStateResponse(
              state_response.initial_state_response());
    } else {
      StateDownloadMessageProcessor::ParsedResponse parsed_response;

      parsed_response.restore_mode = ConvertRestoreMode(restore_mode);

      if (state_response.has_management_domain())
        parsed_response.management_domain = state_response.management_domain();

      if (state_response.has_disabled_state()) {
        parsed_response.disabled_message =
            state_response.disabled_state().message();
      }

      // Package license is not available during the re-enrollment
      parsed_response.is_license_packaged_with_device.reset();
      parsed_response.license_type.reset();

      // Logging as "WARNING" to make sure it's preserved in the logs.
      LOG(WARNING) << "Received restore_mode=" << restore_mode << " ("
                   << parsed_response.restore_mode << ").";

      return parsed_response;
    }
  }

 private:
  // Stable state key.
  std::string server_backed_state_key_;
};

}  // namespace

AutoEnrollmentClientImpl::FactoryImpl::FactoryImpl() {}
AutoEnrollmentClientImpl::FactoryImpl::~FactoryImpl() {}

std::unique_ptr<AutoEnrollmentClient>
AutoEnrollmentClientImpl::FactoryImpl::CreateForFRE(
    const ProgressCallback& progress_callback,
    DeviceManagementService* device_management_service,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& server_backed_state_key,
    int power_initial,
    int power_limit) {
  return base::WrapUnique(new AutoEnrollmentClientImpl(
      progress_callback, device_management_service, local_state,
      url_loader_factory,
      std::make_unique<DeviceIdentifierProviderFRE>(server_backed_state_key),
      std::make_unique<StateDownloadMessageProcessorFRE>(
          server_backed_state_key),
      power_initial, power_limit, kUMASuffixFRE,
      /*private_set_membership_helper=*/nullptr));
}

std::unique_ptr<AutoEnrollmentClient>
AutoEnrollmentClientImpl::FactoryImpl::CreateForInitialEnrollment(
    const ProgressCallback& progress_callback,
    DeviceManagementService* device_management_service,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const std::string& device_serial_number,
    const std::string& device_brand_code,
    int power_initial,
    int power_limit,
    std::unique_ptr<PsmRlweDmserverClient> psm_rlwe_dmserver_client) {
  return base::WrapUnique(new AutoEnrollmentClientImpl(
      progress_callback, device_management_service, local_state,
      url_loader_factory,
      /*device_identifier_provider_fre=*/nullptr,
      std::make_unique<StateDownloadMessageProcessorInitialEnrollment>(
          device_serial_number, device_brand_code),
      power_initial, power_limit, kUMASuffixInitialEnrollment,
      std::move(psm_rlwe_dmserver_client)));
}

AutoEnrollmentClientImpl::~AutoEnrollmentClientImpl() {
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

// static
void AutoEnrollmentClientImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShouldAutoEnroll, false);
  registry->RegisterIntegerPref(prefs::kAutoEnrollmentPowerLimit, -1);
  registry->RegisterBooleanPref(prefs::kShouldRetrieveDeviceState, false);
  registry->RegisterIntegerPref(prefs::kEnrollmentPsmResult, -1);
  registry->RegisterTimePref(prefs::kEnrollmentPsmDeterminationTime,
                             base::Time());
}

void AutoEnrollmentClientImpl::Start() {
  // (Re-)register the network change observer.
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

  // Drop the previous job and reset state.
  request_job_.reset();
  hash_dance_time_start_ = base::TimeTicks();
  state_ = AUTO_ENROLLMENT_STATE_PENDING;
  modulus_updates_received_ = 0;
  has_server_state_ = false;
  device_state_available_ = false;

  NextStep();
}

void AutoEnrollmentClientImpl::Retry() {
  RetryStep();
}

void AutoEnrollmentClientImpl::CancelAndDeleteSoon() {
  // Regardless of PSM execution, only check if neither Hash dance request (i.e.
  // DeviceAutoEnrollmentRequest), nor device state request
  // (i.e.DeviceInitialEnrollmentStateRequest or DeviceStateRetrievalRequest) is
  // in progress.
  if (!request_job_) {
    // Regardless of PsmHelper client execution, the AutoEnrollmentClientImpl
    // isn't running, just delete it and it will delete PsmHelper immediately.
    delete this;
  } else {
    // Client still running, but our owner isn't interested in the result
    // anymore. Wait until the protocol completes to measure the extra time
    // needed.
    time_extra_start_ = base::TimeTicks::Now();
    progress_callback_.Reset();
  }
}

void AutoEnrollmentClientImpl::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (type != network::mojom::ConnectionType::CONNECTION_NONE &&
      !progress_callback_.is_null()) {
    RetryStep();
  }
}

AutoEnrollmentClientImpl::AutoEnrollmentClientImpl(
    const ProgressCallback& callback,
    DeviceManagementService* service,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<DeviceIdentifierProviderFRE> device_identifier_provider_fre,
    std::unique_ptr<StateDownloadMessageProcessor>
        state_download_message_processor,
    int power_initial,
    int power_limit,
    std::string uma_suffix,
    std::unique_ptr<PsmRlweDmserverClient> psm_rlwe_dmserver_client)
    : progress_callback_(callback),
      state_(AUTO_ENROLLMENT_STATE_IDLE),
      has_server_state_(false),
      device_state_available_(false),
      device_id_(base::GenerateGUID()),
      current_power_(power_initial),
      power_limit_(power_limit),
      modulus_updates_received_(0),
      device_management_service_(service),
      local_state_(local_state),
      url_loader_factory_(url_loader_factory),
      device_identifier_provider_fre_(
          std::move(device_identifier_provider_fre)),
      state_download_message_processor_(
          std::move(state_download_message_processor)),
      psm_rlwe_dmserver_client_(std::move(psm_rlwe_dmserver_client)),
      uma_suffix_(uma_suffix) {
  DCHECK_LE(current_power_, power_limit_);
  DCHECK(!progress_callback_.is_null());
}

bool AutoEnrollmentClientImpl::GetCachedDecision() {
  const PrefService::Preference* has_server_state_pref =
      local_state_->FindPreference(prefs::kShouldAutoEnroll);
  const PrefService::Preference* previous_limit_pref =
      local_state_->FindPreference(prefs::kAutoEnrollmentPowerLimit);

  if (!has_server_state_pref || has_server_state_pref->IsDefaultValue() ||
      !has_server_state_pref->GetValue()->is_bool() || !previous_limit_pref ||
      previous_limit_pref->IsDefaultValue() ||
      !previous_limit_pref->GetValue()->is_int() ||
      power_limit_ > previous_limit_pref->GetValue()->GetInt()) {
    return false;
  }

  has_server_state_ = has_server_state_pref->GetValue()->GetBool();
  return true;
}

bool AutoEnrollmentClientImpl::RetrievePsmCachedDecision() {
  // PSM protocol has to be enabled whenever this function is called.
  DCHECK(psm_rlwe_dmserver_client_);

  const PrefService::Preference* has_psm_server_state_pref =
      local_state_->FindPreference(prefs::kShouldRetrieveDeviceState);

  if (!has_psm_server_state_pref ||
      has_psm_server_state_pref->IsDefaultValue()) {
    // Verify the pref is registered as a boolean.
    DCHECK(has_psm_server_state_pref->GetValue()->is_bool());

    return false;
  }

  has_server_state_ = has_psm_server_state_pref->GetValue()->GetBool();
  return true;
}

bool AutoEnrollmentClientImpl::IsClientForInitialEnrollment() const {
  return psm_rlwe_dmserver_client_ != nullptr;
}

bool AutoEnrollmentClientImpl::ShouldSendDeviceStateRequest() const {
  return has_server_state_.has_value() && has_server_state_.value() &&
         !device_state_available_;
}

bool AutoEnrollmentClientImpl::RetryStep() {
  // If there is a pending request job, let it finish.
  if (request_job_)
    return true;

  if (IsClientForInitialEnrollment()) {
    if (PsmRetryStep())
      return true;
  } else {
    // Send DeviceAutoEnrollmentRequest (i.e. Hash dance protocol) if Hash dance
    // decision has not been retrieved before.
    if (!GetCachedDecision()) {
      SendBucketDownloadRequest();
      return true;
    }
  }

  // Send DeviceStateRetrievalRequest if it has a server-backed state
  // (determined by either PSM or Hash dance protocol).
  if (ShouldSendDeviceStateRequest()) {
    SendDeviceStateRequest();
    return true;
  }

  return false;
}

bool AutoEnrollmentClientImpl::PsmRetryStep() {
  // PSM protocol has to be enabled whenever this function is called.
  DCHECK(psm_rlwe_dmserver_client_);

  // Don't retry if the protocol had an error.
  if (psm_result_holder_ && psm_result_holder_->IsError())
    return false;

  // If the PSM protocol is in progress, signal to the caller
  // that nothing else needs to be done.
  if (psm_rlwe_dmserver_client_->IsCheckMembershipInProgress())
    return true;

  if (RetrievePsmCachedDecision()) {
    LOG(WARNING) << "PSM Cached: psm_server_state="
                 << has_server_state_.value();
    return false;
  } else {
    // Set the initial PSM execution result as unknown until it finishes
    // successfully or due to an error.
    // Also, clear the PSM determination timestamp.
    local_state_->SetInteger(prefs::kEnrollmentPsmResult,
                             em::DeviceRegisterRequest::PSM_RESULT_UNKNOWN);
    local_state_->ClearPref(prefs::kEnrollmentPsmDeterminationTime);

    psm_rlwe_dmserver_client_->CheckMembership(
        base::BindOnce(&AutoEnrollmentClientImpl::HandlePsmCompletion,
                       base::Unretained(this)));
    return true;
  }
}

void AutoEnrollmentClientImpl::HandlePsmCompletion(
    PsmRlweDmserverClient::ResultHolder psm_result_holder) {
  psm_result_holder_ = std::move(psm_result_holder);

  // Update `local_state_` PSM's prefs with their corresponding result values.
  if (psm_result_holder_->psm_result != PsmResult::kSuccessfulDetermination) {
    local_state_->SetInteger(prefs::kEnrollmentPsmResult,
                             em::DeviceRegisterRequest::PSM_RESULT_ERROR);
  } else {
    local_state_->SetBoolean(prefs::kShouldRetrieveDeviceState,
                             psm_result_holder_->membership_result.value());
    local_state_->SetTime(
        prefs::kEnrollmentPsmDeterminationTime,
        psm_result_holder_->membership_determination_time.value());
    local_state_->SetInteger(
        prefs::kEnrollmentPsmResult,
        psm_result_holder_->membership_result.value()
            ? em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITH_STATE
            : em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITHOUT_STATE);
  }

  switch (psm_result_holder_->psm_result) {
    case PsmResult::kConnectionError:
      ReportProgress(AUTO_ENROLLMENT_STATE_CONNECTION_ERROR);
      break;
    case PsmResult::kServerError:
      ReportProgress(AUTO_ENROLLMENT_STATE_SERVER_ERROR);
      break;

    // At the moment, AutoEnrollmentClientImpl will not distinguish between
    // any of the PSM errors and will perform `NextStep`, except for
    // connection error, and server error. These are the ones that will be
    // reported directly as an error.
    // TODO(crbug.com/1249792): Call `NextStep` only when PSM executed
    // successfully (i.e. PsmResult has value kSuccessfulDetermination).
    case PsmResult::kSuccessfulDetermination:
    case PsmResult::kCreateRlweClientLibraryError:
    case PsmResult::kCreateOprfRequestLibraryError:
    case PsmResult::kCreateQueryRequestLibraryError:
    case PsmResult::kProcessingQueryResponseLibraryError:
    case PsmResult::kEmptyOprfResponseError:
    case PsmResult::kEmptyQueryResponseError:
    case PsmResult::kTimeout:
      NextStep();
      break;
  }
}

void AutoEnrollmentClientImpl::ReportProgress(AutoEnrollmentState state) {
  state_ = state;
  if (progress_callback_.is_null()) {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  } else {
    progress_callback_.Run(state_);
  }
}

void AutoEnrollmentClientImpl::NextStep() {
  if (RetryStep())
    return;

  // Protocol finished successfully, report result.
  const DeviceStateMode device_state_mode = GetDeviceStateMode();
  switch (device_state_mode) {
    case RESTORE_MODE_NONE:
      ReportProgress(AUTO_ENROLLMENT_STATE_NO_ENROLLMENT);
      break;
    case RESTORE_MODE_DISABLED:
      ReportProgress(AUTO_ENROLLMENT_STATE_DISABLED);
      break;
    case RESTORE_MODE_REENROLLMENT_REQUESTED:
    case RESTORE_MODE_REENROLLMENT_ENFORCED:
    case INITIAL_MODE_ENROLLMENT_ENFORCED:
      ReportProgress(AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT);
      break;
    case RESTORE_MODE_REENROLLMENT_ZERO_TOUCH:
    case INITIAL_MODE_ENROLLMENT_ZERO_TOUCH:
      ReportProgress(AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH);
      break;
  }
}

void AutoEnrollmentClientImpl::SendBucketDownloadRequest() {
  // This method should only be called when the client has been created for FRE
  // use case.
  DCHECK(!IsClientForInitialEnrollment());
  DCHECK(device_identifier_provider_fre_);

  // Start the Hash dance timer during the first attempt.
  if (hash_dance_time_start_.is_null())
    hash_dance_time_start_ = base::TimeTicks::Now();

  std::string id_hash = device_identifier_provider_fre_->GetIdHash();
  // Currently AutoEnrollmentClientImpl supports working with hashes that are at
  // least 8 bytes long. If this is reduced, the computation of the remainder
  // must also be adapted to handle the case of a shorter hash gracefully.
  DCHECK_GE(id_hash.size(), 8u);

  uint64_t remainder = 0;
  const size_t last_byte_index = id_hash.size() - 1;
  for (int i = 0; 8 * i < current_power_; ++i) {
    uint64_t byte = id_hash[last_byte_index - i] & 0xff;
    remainder = remainder | (byte << (8 * i));
  }
  remainder = remainder & ((UINT64_C(1) << current_power_) - 1);

  ReportProgress(AUTO_ENROLLMENT_STATE_PENDING);

  // Record the time when the bucket download request is started. Note that the
  // time may be set multiple times. This is fine, only the last request is the
  // one where the hash bucket is actually downloaded.
  time_start_bucket_download_ = base::TimeTicks::Now();

  // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
  // in the logs.
  LOG(WARNING) << "Request bucket #" << remainder;

  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          device_management_service_,
          DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
          device_id_,
          /*critical=*/false, DMAuth::NoAuth(),
          /*oauth_token=*/absl::nullopt, url_loader_factory_,
          base::BindOnce(
              &AutoEnrollmentClientImpl::HandleRequestCompletion,
              base::Unretained(this),
              &AutoEnrollmentClientImpl::OnBucketDownloadRequestCompletion));

  em::DeviceAutoEnrollmentRequest* request =
      config->request()->mutable_auto_enrollment_request();
  request->set_remainder(remainder);
  request->set_modulus(INT64_C(1) << current_power_);
  request->set_enrollment_check_type(
      device_identifier_provider_fre_->GetEnrollmentCheckType());

  request_job_ = device_management_service_->CreateJob(std::move(config));
}

void AutoEnrollmentClientImpl::SendDeviceStateRequest() {
  ReportProgress(AUTO_ENROLLMENT_STATE_PENDING);

  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          device_management_service_,
          state_download_message_processor_->GetJobType(), device_id_,
          /*critical=*/false, DMAuth::NoAuth(),
          /*oauth_token=*/absl::nullopt, url_loader_factory_,
          base::BindRepeating(
              &AutoEnrollmentClientImpl::HandleRequestCompletion,
              base::Unretained(this),
              &AutoEnrollmentClientImpl::OnDeviceStateRequestCompletion));

  state_download_message_processor_->FillRequest(config->request());
  request_job_ = device_management_service_->CreateJob(std::move(config));
}

void AutoEnrollmentClientImpl::HandleRequestCompletion(
    RequestCompletionHandler handler,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  base::UmaHistogramSparse(kUMAHashDanceRequestStatus + uma_suffix_, status);
  if (status != DM_STATUS_SUCCESS) {
    LOG(ERROR) << "Auto enrollment error: " << status;
    if (status == DM_STATUS_REQUEST_FAILED)
      base::UmaHistogramSparse(kUMAHashDanceNetworkErrorCode + uma_suffix_,
                               -net_error);
    request_job_.reset();

    // Abort if CancelAndDeleteSoon has been called meanwhile.
    if (progress_callback_.is_null()) {
      base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
    } else {
      ReportProgress(status == DM_STATUS_REQUEST_FAILED
                         ? AUTO_ENROLLMENT_STATE_CONNECTION_ERROR
                         : AUTO_ENROLLMENT_STATE_SERVER_ERROR);
    }
    return;
  }

  bool progress =
      (this->*handler)(request_job_.get(), status, net_error, response);
  request_job_.reset();
  if (progress)
    NextStep();
  else
    ReportProgress(AUTO_ENROLLMENT_STATE_SERVER_ERROR);
}

bool AutoEnrollmentClientImpl::OnBucketDownloadRequestCompletion(
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  // This method should only be called when the client has been created for FRE
  // use case.
  DCHECK(!IsClientForInitialEnrollment());

  bool progress = false;
  const em::DeviceAutoEnrollmentResponse& enrollment_response =
      response.auto_enrollment_response();
  if (!response.has_auto_enrollment_response()) {
    LOG(ERROR) << "Server failed to provide auto-enrollment response.";
  } else if (enrollment_response.has_expected_modulus()) {
    // Server is asking us to retry with a different modulus.
    modulus_updates_received_++;

    int64_t modulus = enrollment_response.expected_modulus();
    int power = NextPowerOf2(modulus);
    if ((INT64_C(1) << power) != modulus) {
      LOG(ERROR) << "Auto enrollment: the server didn't ask for a power-of-2 "
                 << "modulus. Using the closest power-of-2 instead "
                 << "(" << modulus << " vs 2^" << power << ")";
    }
    if (modulus_updates_received_ >= 2) {
      LOG(ERROR) << "Auto enrollment error: already retried with an updated "
                 << "modulus but the server asked for a new one again: "
                 << power;
    } else if (power > power_limit_) {
      LOG(ERROR) << "Auto enrollment error: the server asked for a larger "
                 << "modulus than the client accepts (" << power << " vs "
                 << power_limit_ << ").";
    } else {
      // Retry at most once with the modulus that the server requested.
      if (power <= current_power_) {
        LOG(WARNING) << "Auto enrollment: the server asked to use a modulus ("
                     << power << ") that isn't larger than the first used ("
                     << current_power_ << "). Retrying anyway.";
      }
      // Remember this value, so that eventual retries start with the correct
      // modulus.
      current_power_ = power;
      return true;
    }
  } else {
    // Server should have sent down a list of hashes to try.
    has_server_state_ = IsIdHashInProtobuf(enrollment_response.hashes());
    // Cache the current decision in local_state, so that it is reused in case
    // the device reboots before enrolling.
    local_state_->SetBoolean(prefs::kShouldAutoEnroll,
                             has_server_state_.value());
    local_state_->SetInteger(prefs::kAutoEnrollmentPowerLimit, power_limit_);
    local_state_->CommitPendingWrite();

    // TODO(crbug.com/1271134): Logging as "WARNING" to make sure it's preserved
    // in the logs.
    LOG(WARNING) << "Received has_state=" << has_server_state_.value();

    progress = true;
    // Report timing if hash dance finished successfully and if the caller is
    // still interested in the result.
    if (!progress_callback_.is_null())
      RecordHashDanceSuccessTimeHistogram();
  }

  // Bucket download done, update UMA.
  UpdateBucketDownloadTimingHistograms();
  return progress;
}

bool AutoEnrollmentClientImpl::OnDeviceStateRequestCompletion(
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  absl::optional<StateDownloadMessageProcessor::ParsedResponse>
      parsed_response_opt;

  parsed_response_opt =
      state_download_message_processor_->ParseResponse(response);
  if (!parsed_response_opt)
    return false;

  StateDownloadMessageProcessor::ParsedResponse parsed_response =
      std::move(parsed_response_opt.value());
  {
    DictionaryPrefUpdate dict(local_state_, prefs::kServerBackedDeviceState);
    UpdateDict(dict.Get(), kDeviceStateManagementDomain,
               parsed_response.management_domain.has_value(),
               std::make_unique<base::Value>(
                   parsed_response.management_domain.value_or(std::string())));

    UpdateDict(dict.Get(), kDeviceStateMode,
               !parsed_response.restore_mode.empty(),
               std::make_unique<base::Value>(parsed_response.restore_mode));

    UpdateDict(dict.Get(), kDeviceStateDisabledMessage,
               parsed_response.disabled_message.has_value(),
               std::make_unique<base::Value>(
                   parsed_response.disabled_message.value_or(std::string())));

    UpdateDict(
        dict.Get(), kDeviceStatePackagedLicense,
        parsed_response.is_license_packaged_with_device.has_value(),
        std::make_unique<base::Value>(
            parsed_response.is_license_packaged_with_device.value_or(false)));

    UpdateDict(dict.Get(), kDeviceStateLicenseType,
               parsed_response.license_type.has_value(),
               std::make_unique<base::Value>(
                   parsed_response.license_type.value_or(std::string())));
  }
  local_state_->CommitPendingWrite();
  device_state_available_ = true;
  return true;
}

bool AutoEnrollmentClientImpl::IsIdHashInProtobuf(
    const google::protobuf::RepeatedPtrField<std::string>& hashes) {
  // This method should only be called when the client has been created for FRE
  // use case.
  DCHECK(!IsClientForInitialEnrollment());
  DCHECK(device_identifier_provider_fre_);

  std::string id_hash = device_identifier_provider_fre_->GetIdHash();
  for (int i = 0; i < hashes.size(); ++i) {
    if (hashes.Get(i) == id_hash)
      return true;
  }
  return false;
}

void AutoEnrollmentClientImpl::UpdateBucketDownloadTimingHistograms() {
  // This method should only be called when the client has been created for FRE
  // use case.
  DCHECK(!IsClientForInitialEnrollment());

  // These values determine bucketing of the histogram, they should not be
  // changed.
  // The minimum time can't be 0, must be at least 1.
  static const base::TimeDelta kMin = base::Milliseconds(1);
  static const base::TimeDelta kMax = base::Minutes(5);
  // However, 0 can still be sampled.
  static const base::TimeDelta kZero = base::Milliseconds(0);
  static const int kBuckets = 50;

  base::TimeTicks now = base::TimeTicks::Now();
  if (!hash_dance_time_start_.is_null()) {
    base::TimeDelta delta = now - hash_dance_time_start_;
    base::UmaHistogramCustomTimes(kUMAHashDanceProtocolTime + uma_suffix_,
                                  delta, kMin, kMax, kBuckets);
  }
  if (!time_start_bucket_download_.is_null()) {
    base::TimeDelta delta = now - time_start_bucket_download_;
    base::UmaHistogramCustomTimes(kUMAHashDanceBucketDownloadTime + uma_suffix_,
                                  delta, kMin, kMax, kBuckets);
  }
  base::TimeDelta delta = kZero;
  if (!time_extra_start_.is_null())
    delta = now - time_extra_start_;
  // This samples |kZero| when there was no need for extra time, so that we can
  // measure the ratio of users that succeeded without needing a delay to the
  // total users going through OOBE.
  base::UmaHistogramCustomTimes(kUMAHashDanceExtraTime + uma_suffix_, delta,
                                kMin, kMax, kBuckets);
}

void AutoEnrollmentClientImpl::RecordHashDanceSuccessTimeHistogram() {
  // This method should only be called when the client has been created for FRE
  // use case.
  DCHECK(!IsClientForInitialEnrollment());

  // These values determine bucketing of the histogram, they should not be
  // changed.
  static const base::TimeDelta kMin = base::Milliseconds(1);
  static const base::TimeDelta kMax = base::Seconds(25);
  static const int kBuckets = 50;

  base::TimeTicks now = base::TimeTicks::Now();
  if (!hash_dance_time_start_.is_null()) {
    base::TimeDelta delta = now - hash_dance_time_start_;
    base::UmaHistogramCustomTimes(kUMAHashDanceSuccessTime + uma_suffix_, delta,
                                  kMin, kMax, kBuckets);
  }
}

}  // namespace policy
