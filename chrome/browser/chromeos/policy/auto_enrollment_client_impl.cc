// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/auto_enrollment_client_impl.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dm_auth.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace em = enterprise_management;

namespace policy {

namespace {

using EnrollmentCheckType =
    em::DeviceAutoEnrollmentRequest::EnrollmentCheckType;

// UMA histogram names.
constexpr char kUMAProtocolTime[] = "Enterprise.AutoEnrollmentProtocolTime";
constexpr char kUMABucketDownloadTime[] =
    "Enterprise.AutoEnrollmentBucketDownloadTime";
constexpr char kUMAExtraTime[] = "Enterprise.AutoEnrollmentExtraTime";
constexpr char kUMARequestStatus[] = "Enterprise.AutoEnrollmentRequestStatus";
constexpr char kUMANetworkErrorCode[] =
    "Enterprise.AutoEnrollmentRequestNetworkErrorCode";

// Suffix for initial enrollment.
constexpr char kUMASuffixInitialEnrollment[] = ".InitialEnrollment";
// Suffix for Forced Re-Enrollment.
constexpr char kUMASuffixFRE[] = ".ForcedReenrollment";

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
void UpdateDict(base::DictionaryValue* dict,
                const char* pref_path,
                bool set_or_clear,
                std::unique_ptr<base::Value> value) {
  if (set_or_clear)
    dict->Set(pref_path, std::move(value));
  else
    dict->Remove(pref_path, NULL);
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
      return kDeviceStateRestoreModeDisabled;
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
  }
}

}  // namespace

// Subclasses of this class provide an identifier and specify the identifier
// set for the DeviceAutoEnrollmentRequest,
class AutoEnrollmentClientImpl::DeviceIdentifierProvider {
 public:
  virtual ~DeviceIdentifierProvider() {}

  // Should return the EnrollmentCheckType to be used in the
  // DeviceAutoEnrollmentRequest. This specifies the identifier set used on
  // the server.
  virtual enterprise_management::DeviceAutoEnrollmentRequest::
      EnrollmentCheckType
      GetEnrollmentCheckType() const = 0;

  // Should return the hash of this device's identifier. The
  // DeviceAutoEnrollmentRequest exchange will check if this hash is in the
  // server-side identifier set specified by |GetEnrollmentCheckType()|
  virtual const std::string& GetIdHash() const = 0;
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
    base::Optional<std::string> management_domain;
    base::Optional<std::string> disabled_message;
    base::Optional<bool> is_license_packaged_with_device;
  };

  // Returns the request job type. This must match the request filled in
  // |FillRequest|.
  virtual DeviceManagementService::JobConfiguration::JobType GetJobType()
      const = 0;

  // Fills the specific request type in |request|.
  virtual void FillRequest(
      enterprise_management::DeviceManagementRequest* request) = 0;

  // Parses the |response|. If it is valid, returns a ParsedResponse struct
  // instance. If it is invalid, returns nullopt.
  virtual base::Optional<ParsedResponse> ParseResponse(
      const enterprise_management::DeviceManagementResponse& response) = 0;
};

namespace {

// Provides device identifier for Forced Re-Enrollment (FRE), where the
// server-backed state key is used.
class DeviceIdentifierProviderFRE
    : public AutoEnrollmentClientImpl::DeviceIdentifierProvider {
 public:
  explicit DeviceIdentifierProviderFRE(
      const std::string& server_backed_state_key) {
    CHECK(!server_backed_state_key.empty());
    server_backed_state_key_hash_ =
        crypto::SHA256HashString(server_backed_state_key);
  }

  EnrollmentCheckType GetEnrollmentCheckType() const override {
    return em::DeviceAutoEnrollmentRequest::ENROLLMENT_CHECK_TYPE_FRE;
  }

  const std::string& GetIdHash() const override {
    return server_backed_state_key_hash_;
  }

 private:
  // SHA-256 digest of the stable identifier.
  std::string server_backed_state_key_hash_;
};

// Provides device identifier for Forced Initial Enrollment, where the brand
// code and serial number is used.
class DeviceIdentifierProviderInitialEnrollment
    : public AutoEnrollmentClientImpl::DeviceIdentifierProvider {
 public:
  DeviceIdentifierProviderInitialEnrollment(
      const std::string& device_serial_number,
      const std::string& device_brand_code) {
    CHECK(!device_serial_number.empty());
    CHECK(!device_brand_code.empty());
    // The hash for initial enrollment is the first 8 bytes of
    // SHA256(<brnad_code>_<serial_number>).
    id_hash_ =
        crypto::SHA256HashString(device_brand_code + "_" + device_serial_number)
            .substr(0, 8);
  }

  EnrollmentCheckType GetEnrollmentCheckType() const override {
    return em::DeviceAutoEnrollmentRequest::
        ENROLLMENT_CHECK_TYPE_FORCED_ENROLLMENT;
  }

  const std::string& GetIdHash() const override { return id_hash_; }

 private:
  // 8-byte Hash built from serial number and brand code passed to the
  // constructor.
  std::string id_hash_;
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

  base::Optional<ParsedResponse> ParseResponse(
      const em::DeviceManagementResponse& response) override {
    StateDownloadMessageProcessorFRE::ParsedResponse parsed_response;
    if (!response.has_device_state_retrieval_response()) {
      LOG(ERROR) << "Server failed to provide auto-enrollment response.";
      return base::nullopt;
    }
    const em::DeviceStateRetrievalResponse& state_response =
        response.device_state_retrieval_response();
    parsed_response.restore_mode =
        ConvertRestoreMode(state_response.restore_mode());
    if (state_response.has_management_domain())
      parsed_response.management_domain = state_response.management_domain();

    if (state_response.has_disabled_state()) {
      parsed_response.disabled_message =
          state_response.disabled_state().message();
    }

    // Package license is not available during the re-enrollment
    parsed_response.is_license_packaged_with_device.reset();

    // Logging as "WARNING" to make sure it's preserved in the logs.
    LOG(WARNING) << "Received restore_mode=" << state_response.restore_mode();

    return parsed_response;
  }

 private:
  // Stable state key.
  std::string server_backed_state_key_;
};

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

  base::Optional<ParsedResponse> ParseResponse(
      const em::DeviceManagementResponse& response) override {
    StateDownloadMessageProcessorFRE::ParsedResponse parsed_response;
    if (!response.has_device_initial_enrollment_state_response()) {
      LOG(ERROR) << "Server failed to provide initial enrollment response.";
      return base::nullopt;
    }

    const em::DeviceInitialEnrollmentStateResponse& state_response =
        response.device_initial_enrollment_state_response();
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

    // Device disabling is not supported in initial forced enrollment.
    parsed_response.disabled_message.reset();

    // Logging as "WARNING" to make sure it's preserved in the logs.
    LOG(WARNING) << "Received initial_enrollment_mode="
                 << state_response.initial_enrollment_mode() << ". "
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
      power_initial, power_limit, base::nullopt, kUMASuffixFRE));
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
    int power_outdated_server_detect) {
  return base::WrapUnique(new AutoEnrollmentClientImpl(
      progress_callback, device_management_service, local_state,
      url_loader_factory,
      std::make_unique<DeviceIdentifierProviderInitialEnrollment>(
          device_serial_number, device_brand_code),
      std::make_unique<StateDownloadMessageProcessorInitialEnrollment>(
          device_serial_number, device_brand_code),
      power_initial, power_limit,
      base::make_optional(power_outdated_server_detect),
      kUMASuffixInitialEnrollment));
}

AutoEnrollmentClientImpl::~AutoEnrollmentClientImpl() {
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

// static
void AutoEnrollmentClientImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShouldAutoEnroll, false);
  registry->RegisterIntegerPref(prefs::kAutoEnrollmentPowerLimit, -1);
}

void AutoEnrollmentClientImpl::Start() {
  // (Re-)register the network change observer.
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

  // Drop the previous job and reset state.
  request_job_.reset();
  state_ = AUTO_ENROLLMENT_STATE_PENDING;
  time_start_ = base::Time::Now();
  modulus_updates_received_ = 0;
  has_server_state_ = false;
  device_state_available_ = false;

  NextStep();
}

void AutoEnrollmentClientImpl::Retry() {
  RetryStep();
}

void AutoEnrollmentClientImpl::CancelAndDeleteSoon() {
  if (time_start_.is_null() || !request_job_) {
    // The client isn't running, just delete it.
    delete this;
  } else {
    // Client still running, but our owner isn't interested in the result
    // anymore. Wait until the protocol completes to measure the extra time
    // needed.
    time_extra_start_ = base::Time::Now();
    progress_callback_.Reset();
  }
}

std::string AutoEnrollmentClientImpl::device_id() const {
  return device_id_;
}

AutoEnrollmentState AutoEnrollmentClientImpl::state() const {
  return state_;
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
    std::unique_ptr<DeviceIdentifierProvider> device_identifier_provider,
    std::unique_ptr<StateDownloadMessageProcessor>
        state_download_message_processor,
    int power_initial,
    int power_limit,
    base::Optional<int> power_outdated_server_detect,
    std::string uma_suffix)
    : progress_callback_(callback),
      state_(AUTO_ENROLLMENT_STATE_IDLE),
      has_server_state_(false),
      device_state_available_(false),
      device_id_(base::GenerateGUID()),
      current_power_(power_initial),
      power_limit_(power_limit),
      power_outdated_server_detect_(power_outdated_server_detect),
      modulus_updates_received_(0),
      device_management_service_(service),
      local_state_(local_state),
      url_loader_factory_(url_loader_factory),
      device_identifier_provider_(std::move(device_identifier_provider)),
      state_download_message_processor_(
          std::move(state_download_message_processor)),
      uma_suffix_(uma_suffix) {
  DCHECK_LE(current_power_, power_limit_);
  DCHECK(!progress_callback_.is_null());
}

bool AutoEnrollmentClientImpl::GetCachedDecision() {
  const PrefService::Preference* has_server_state_pref =
      local_state_->FindPreference(prefs::kShouldAutoEnroll);
  const PrefService::Preference* previous_limit_pref =
      local_state_->FindPreference(prefs::kAutoEnrollmentPowerLimit);
  bool has_server_state = false;
  int previous_limit = -1;

  if (!has_server_state_pref || has_server_state_pref->IsDefaultValue() ||
      !has_server_state_pref->GetValue()->GetAsBoolean(&has_server_state) ||
      !previous_limit_pref || previous_limit_pref->IsDefaultValue() ||
      !previous_limit_pref->GetValue()->GetAsInteger(&previous_limit) ||
      power_limit_ > previous_limit) {
    return false;
  }

  has_server_state_ = has_server_state;
  return true;
}

bool AutoEnrollmentClientImpl::RetryStep() {
  // If there is a pending request job, let it finish.
  if (request_job_)
    return true;

  if (GetCachedDecision()) {
    VLOG(1) << "Cached: has_state=" << has_server_state_;
    // The bucket download check has completed already. If it came back
    // positive, then device state should be (re-)downloaded.
    if (has_server_state_) {
      if (!device_state_available_) {
        SendDeviceStateRequest();
        return true;
      }
    }
  } else {
    // Start bucket download.
    SendBucketDownloadRequest();
    return true;
  }

  return false;
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
  std::string id_hash = device_identifier_provider_->GetIdHash();
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
  time_start_bucket_download_ = base::Time::Now();

  VLOG(1) << "Request bucket #" << remainder;
  std::unique_ptr<DMServerJobConfiguration> config = std::make_unique<
      DMServerJobConfiguration>(
      device_management_service_,
      policy::DeviceManagementService::JobConfiguration::TYPE_AUTO_ENROLLMENT,
      device_id_,
      /*critical=*/false, DMAuth::NoAuth(),
      /*oauth_token=*/base::nullopt, url_loader_factory_,
      base::BindOnce(
          &AutoEnrollmentClientImpl::HandleRequestCompletion,
          base::Unretained(this),
          &AutoEnrollmentClientImpl::OnBucketDownloadRequestCompletion));

  em::DeviceAutoEnrollmentRequest* request =
      config->request()->mutable_auto_enrollment_request();
  request->set_remainder(remainder);
  request->set_modulus(INT64_C(1) << current_power_);
  request->set_enrollment_check_type(
      device_identifier_provider_->GetEnrollmentCheckType());

  request_job_ = device_management_service_->CreateJob(std::move(config));
}

void AutoEnrollmentClientImpl::SendDeviceStateRequest() {
  ReportProgress(AUTO_ENROLLMENT_STATE_PENDING);

  std::unique_ptr<DMServerJobConfiguration> config =
      std::make_unique<DMServerJobConfiguration>(
          device_management_service_,
          state_download_message_processor_->GetJobType(), device_id_,
          /*critical=*/false, DMAuth::NoAuth(),
          /*oauth_token=*/base::nullopt, url_loader_factory_,
          base::BindRepeating(
              &AutoEnrollmentClientImpl::HandleRequestCompletion,
              base::Unretained(this),
              &AutoEnrollmentClientImpl::OnDeviceStateRequestCompletion));

  state_download_message_processor_->FillRequest(config->request());
  request_job_ = device_management_service_->CreateJob(std::move(config));
}

void AutoEnrollmentClientImpl::HandleRequestCompletion(
    RequestCompletionHandler handler,
    policy::DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  base::UmaHistogramSparse(kUMARequestStatus + uma_suffix_, status);
  if (status != DM_STATUS_SUCCESS) {
    LOG(ERROR) << "Auto enrollment error: " << status;
    if (status == DM_STATUS_REQUEST_FAILED)
      base::UmaHistogramSparse(kUMANetworkErrorCode + uma_suffix_, -net_error);
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
    policy::DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
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
    } else if (power_outdated_server_detect_.has_value() &&
               power >= power_outdated_server_detect_.value()) {
      LOG(ERROR) << "Skipping auto enrollment: The server was detected as "
                 << "outdated (power=" << power
                 << ", power_outdated_server_detect="
                 << power_outdated_server_detect_.value() << ").";
      has_server_state_ = false;
      // Cache the decision in local_state, so that it is reused in case
      // the device reboots before completing OOBE. Note that this does not
      // disable Forced Re-Enrollment for this device, because local state will
      // be empty after the device is wiped.
      local_state_->SetBoolean(prefs::kShouldAutoEnroll, false);
      local_state_->SetInteger(prefs::kAutoEnrollmentPowerLimit, power_limit_);
      local_state_->CommitPendingWrite();
      return true;
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
    local_state_->SetBoolean(prefs::kShouldAutoEnroll, has_server_state_);
    local_state_->SetInteger(prefs::kAutoEnrollmentPowerLimit, power_limit_);
    local_state_->CommitPendingWrite();
    VLOG(1) << "Received has_state=" << has_server_state_;
    progress = true;
  }

  // Bucket download done, update UMA.
  UpdateBucketDownloadTimingHistograms();
  return progress;
}

bool AutoEnrollmentClientImpl::OnDeviceStateRequestCompletion(
    policy::DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  base::Optional<StateDownloadMessageProcessorFRE::ParsedResponse>
      parsed_response_opt;

  parsed_response_opt =
      state_download_message_processor_->ParseResponse(response);
  if (!parsed_response_opt)
    return false;

  StateDownloadMessageProcessorFRE::ParsedResponse parsed_response =
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
  }
  local_state_->CommitPendingWrite();
  device_state_available_ = true;
  return true;
}

bool AutoEnrollmentClientImpl::IsIdHashInProtobuf(
    const google::protobuf::RepeatedPtrField<std::string>& hashes) {
  std::string id_hash = device_identifier_provider_->GetIdHash();
  for (int i = 0; i < hashes.size(); ++i) {
    if (hashes.Get(i) == id_hash)
      return true;
  }
  return false;
}

void AutoEnrollmentClientImpl::UpdateBucketDownloadTimingHistograms() {
  // The minimum time can't be 0, must be at least 1.
  static const base::TimeDelta kMin = base::TimeDelta::FromMilliseconds(1);
  static const base::TimeDelta kMax = base::TimeDelta::FromMinutes(5);
  // However, 0 can still be sampled.
  static const base::TimeDelta kZero = base::TimeDelta::FromMilliseconds(0);
  static const int kBuckets = 50;

  base::Time now = base::Time::Now();
  if (!time_start_.is_null()) {
    base::TimeDelta delta = now - time_start_;
    base::UmaHistogramCustomTimes(kUMAProtocolTime + uma_suffix_, delta, kMin,
                                  kMax, kBuckets);
  }
  if (!time_start_bucket_download_.is_null()) {
    base::TimeDelta delta = now - time_start_bucket_download_;
    base::UmaHistogramCustomTimes(kUMABucketDownloadTime + uma_suffix_, delta,
                                  kMin, kMax, kBuckets);
  }
  base::TimeDelta delta = kZero;
  if (!time_extra_start_.is_null())
    delta = now - time_extra_start_;
  // This samples |kZero| when there was no need for extra time, so that we can
  // measure the ratio of users that succeeded without needing a delay to the
  // total users going through OOBE.
  base::UmaHistogramCustomTimes(kUMAExtraTime + uma_suffix_, delta, kMin, kMax,
                                kBuckets);
}

}  // namespace policy
