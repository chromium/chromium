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
#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_id_provider.h"
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
#include "third_party/private_membership/src/membership_response_map.h"
#include "url/gurl.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;
namespace psm_rlwe = ::private_membership::rlwe;
using EnrollmentCheckType =
    em::DeviceAutoEnrollmentRequest::EnrollmentCheckType;

// Timeout for running PSM protocol.
constexpr base::TimeDelta kPsmTimeout = base::Seconds(15);

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

class PsmHelper {
 public:
  // Callback will be triggered after completing the protocol, in case of a
  // successful determination or stopping due to an error.
  // The `psm_result` represents the final result of PSM protocol.
  using CompletionCallback = base::OnceCallback<void(PsmResult psm_result)>;

  // The PsmHelper doesn't take ownership of |device_management_service|,
  // |local_state|, |psm_rlwe_client_factory| and |psm_rlwe_id_provider|.
  // All of them must not be nullptr. Also, |device_management_service|,
  // and |local_state| must outlive PsmHelper.
  PsmHelper(DeviceManagementService* device_management_service,
            scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
            PrefService* local_state,
            PrivateMembershipRlweClient::Factory* psm_rlwe_client_factory,
            PsmRlweIdProvider* psm_rlwe_id_provider)
      : random_device_id_(base::GenerateGUID()),
        url_loader_factory_(url_loader_factory),
        device_management_service_(device_management_service),
        local_state_(local_state) {
    CHECK(device_management_service);
    CHECK(psm_rlwe_client_factory);
    CHECK(psm_rlwe_id_provider);
    DCHECK(local_state_);

    psm_rlwe_id_ = psm_rlwe_id_provider->ConstructRlweId();

    // Create PSM client for |psm_rlwe_id_| with use case as CROS_DEVICE_STATE.
    std::vector<psm_rlwe::RlwePlaintextId> psm_ids = {psm_rlwe_id_};
    auto status_or_client = psm_rlwe_client_factory->Create(
        psm_rlwe::RlweUseCase::CROS_DEVICE_STATE, psm_ids);
    if (!status_or_client.ok()) {
      // If the PSM RLWE client hasn't been created successfully, then report
      // the error and don't run the protocol.
      LOG(ERROR)
          << "PSM error: unexpected internal logic error during creating "
             "PSM RLWE client";
      last_psm_execution_result_ = PsmResult::kCreateRlweClientLibraryError;
      base::UmaHistogramEnumeration(kUMAPsmResult + uma_suffix_,
                                    PsmResult::kCreateRlweClientLibraryError);
      return;
    }

    psm_rlwe_client_ = std::move(status_or_client).value();
  }

  // Disallow copy constructor and assignment operator.
  PsmHelper(const PsmHelper&) = delete;
  PsmHelper& operator=(const PsmHelper&) = delete;

  // Cancels the ongoing PSM operation, if any (without calling the operation's
  // callbacks).
  ~PsmHelper() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  // Determines the PSM for the |psm_rlwe_id_|. Then, will call |callback| upon
  // completing the protocol, whether it finished with a successful
  // determination or stopped in case of errors. Also, the |callback| has to be
  // non-null.
  // Note: This method should be called only when there is no PSM requests in
  // progress (i.e. `IsCheckMembershipInProgress` is false).
  void CheckMembership(CompletionCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback);

    // Ignore new calls and execute `callback` with
    // |last_psm_execution_result_|, in case any error occurred while running
    // PSM previously.
    if (HasPsmError()) {
      std::move(callback).Run(last_psm_execution_result_.value());
      return;
    }

    // There should not be any pending PSM requests.
    CHECK(!psm_request_job_);

    time_start_ = base::TimeTicks::Now();

    // Set the initial PSM execution result as unknown until it finishes
    // successfully or due to an error.
    // Also, clear the PSM determination timestamp.
    local_state_->SetInteger(prefs::kEnrollmentPsmResult,
                             em::DeviceRegisterRequest::PSM_RESULT_UNKNOWN);
    local_state_->ClearPref(prefs::kEnrollmentPsmDeterminationTime);

    on_completion_callback_ = std::move(callback);

    // Start the protocol and its timeout timer.
    psm_timeout_.Start(
        FROM_HERE, kPsmTimeout,
        base::BindOnce(&PsmHelper::StoreErrorAndStop, base::Unretained(this),
                       PsmResult::kTimeout));
    SendPsmRlweOprfRequest();
  }

  // Tries to load the result of a previous execution of the PSM protocol from
  // local state. Returns decision value if it has been made and is valid,
  // otherwise nullopt.
  absl::optional<bool> GetPsmCachedDecision() const {
    const PrefService::Preference* has_psm_server_state_pref =
        local_state_->FindPreference(prefs::kShouldRetrieveDeviceState);

    if (!has_psm_server_state_pref ||
        has_psm_server_state_pref->IsDefaultValue() ||
        !has_psm_server_state_pref->GetValue()->is_bool()) {
      return absl::nullopt;
    }

    return has_psm_server_state_pref->GetValue()->GetBool();
  }

  // Indicate whether an error occurred while executing the PSM protocol.
  bool HasPsmError() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return last_psm_execution_result_ &&
           last_psm_execution_result_.value() !=
               PsmResult::kSuccessfulDetermination;
  }

  // Returns true if the PSM protocol is still running,
  // otherwise false.
  bool IsCheckMembershipInProgress() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return psm_request_job_ != nullptr;
  }

 private:
  void StoreErrorAndStop(PsmResult psm_result) {
    // Note that kUMAPsmResult histogram is only using initial enrollment as a
    // suffix until PSM support FRE.
    base::UmaHistogramEnumeration(kUMAPsmResult + uma_suffix_, psm_result);

    // Records the PSM execution as an error in local_state, so that value will
    // be used in the DeviceRegisterRequest during the enrollment flow.
    local_state_->SetInteger(prefs::kEnrollmentPsmResult,
                             em::DeviceRegisterRequest::PSM_RESULT_ERROR);
    local_state_->CommitPendingWrite();

    // Stop the PSM timer.
    psm_timeout_.Stop();

    // Stop the current |psm_request_job_|.
    psm_request_job_.reset();

    last_psm_execution_result_ = psm_result;
    std::move(on_completion_callback_).Run(psm_result);
  }

  // Constructs and sends the PSM RLWE OPRF request.
  void SendPsmRlweOprfRequest() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Create RLWE OPRF request.
    const auto status_or_oprf_request = psm_rlwe_client_->CreateOprfRequest();
    if (!status_or_oprf_request.ok()) {
      // If the RLWE OPRF request hasn't been created successfully, then report
      // the error and stop the protocol.
      LOG(ERROR)
          << "PSM error: unexpected internal logic error during creating "
             "RLWE OPRF request";
      StoreErrorAndStop(PsmResult::kCreateOprfRequestLibraryError);
      return;
    }

    LOG(WARNING) << "PSM: prepare and send out the RLWE OPRF request";

    // Prepare the RLWE OPRF request job.
    // The passed callback will not be called if |psm_request_job_| is
    // destroyed, so it's safe to use base::Unretained.
    std::unique_ptr<DMServerJobConfiguration> config =
        CreatePsmRequestJobConfiguration(base::BindOnce(
            &PsmHelper::OnRlweOprfRequestCompletion, base::Unretained(this)));

    em::DeviceManagementRequest* request = config->request();
    em::PrivateSetMembershipRlweRequest* psm_rlwe_request =
        request->mutable_private_set_membership_request()
            ->mutable_rlwe_request();

    *psm_rlwe_request->mutable_oprf_request() = status_or_oprf_request.value();
    psm_request_job_ = device_management_service_->CreateJob(std::move(config));
  }

  // If the completion was successful, then it makes another request to
  // DMServer for performing phase two.
  void OnRlweOprfRequestCompletion(
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const em::DeviceManagementResponse& response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::UmaHistogramSparse(kUMAPsmDmServerRequestStatus + uma_suffix_,
                             status);

    switch (status) {
      case DM_STATUS_SUCCESS: {
        // Check if the RLWE OPRF response is empty.
        if (!response.private_set_membership_response().has_rlwe_response() ||
            !response.private_set_membership_response()
                 .rlwe_response()
                 .has_oprf_response()) {
          LOG(ERROR) << "PSM error: empty OPRF RLWE response";
          StoreErrorAndStop(PsmResult::kEmptyOprfResponseError);
          return;
        }

        LOG(WARNING) << "PSM RLWE OPRF request completed successfully";
        SendPsmRlweQueryRequest(response.private_set_membership_response());
        return;
      }
      case DM_STATUS_REQUEST_FAILED: {
        LOG(ERROR)
            << "PSM error: RLWE OPRF request failed due to connection error";
        base::UmaHistogramSparse(kUMAPsmNetworkErrorCode + uma_suffix_,
                                 -net_error);
        StoreErrorAndStop(PsmResult::kConnectionError);
        return;
      }
      default: {
        LOG(ERROR) << "PSM error: RLWE OPRF request failed due to server error";
        StoreErrorAndStop(PsmResult::kServerError);
        return;
      }
    }
  }

  // Constructs and sends the PSM RLWE Query request.
  void SendPsmRlweQueryRequest(
      const em::PrivateSetMembershipResponse& psm_response) {
    // Extract the oprf_response from |psm_response|.
    const psm_rlwe::PrivateMembershipRlweOprfResponse oprf_response =
        psm_response.rlwe_response().oprf_response();

    const auto status_or_query_request =
        psm_rlwe_client_->CreateQueryRequest(oprf_response);

    // Create RLWE query request.
    if (!status_or_query_request.ok()) {
      // If the RLWE query request hasn't been created successfully, then report
      // the error and stop the protocol.
      LOG(ERROR)
          << "PSM error: unexpected internal logic error during creating "
             "RLWE query request";
      StoreErrorAndStop(PsmResult::kCreateQueryRequestLibraryError);
      return;
    }

    LOG(WARNING) << "PSM: prepare and send out the RLWE query request";

    // Prepare the RLWE query request job.
    std::unique_ptr<DMServerJobConfiguration> config =
        CreatePsmRequestJobConfiguration(
            base::BindOnce(&PsmHelper::OnRlweQueryRequestCompletion,
                           base::Unretained(this), oprf_response));

    em::DeviceManagementRequest* request = config->request();
    em::PrivateSetMembershipRlweRequest* psm_rlwe_request =
        request->mutable_private_set_membership_request()
            ->mutable_rlwe_request();

    *psm_rlwe_request->mutable_query_request() =
        status_or_query_request.value();
    psm_request_job_ = device_management_service_->CreateJob(std::move(config));
  }

  // If the completion was successful, then it will parse the result and call
  // the |on_completion_callback_| for |psm_id_|.
  void OnRlweQueryRequestCompletion(
      const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const em::DeviceManagementResponse& response) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    base::UmaHistogramSparse(kUMAPsmDmServerRequestStatus + uma_suffix_,
                             status);

    switch (status) {
      case DM_STATUS_SUCCESS: {
        // Check if the RLWE query response is empty.
        if (!response.private_set_membership_response().has_rlwe_response() ||
            !response.private_set_membership_response()
                 .rlwe_response()
                 .has_query_response()) {
          LOG(ERROR) << "PSM error: empty query RLWE response";
          StoreErrorAndStop(PsmResult::kEmptyQueryResponseError);
          return;
        }

        const psm_rlwe::PrivateMembershipRlweQueryResponse query_response =
            response.private_set_membership_response()
                .rlwe_response()
                .query_response();

        auto status_or_responses =
            psm_rlwe_client_->ProcessQueryResponse(query_response);

        if (!status_or_responses.ok()) {
          // If the RLWE query response hasn't processed successfully, then
          // report the error and stop the protocol.
          LOG(ERROR) << "PSM error: unexpected internal logic error during "
                        "processing the "
                        "RLWE query response";
          StoreErrorAndStop(PsmResult::kProcessingQueryResponseLibraryError);
          return;
        }

        LOG(WARNING) << "PSM query request completed successfully";

        last_psm_execution_result_ = PsmResult::kSuccessfulDetermination;
        base::UmaHistogramEnumeration(kUMAPsmResult + uma_suffix_,
                                      PsmResult::kSuccessfulDetermination);
        RecordPsmSuccessTimeHistogram();

        // The RLWE query response has been processed successfully. Extract
        // the membership response, and report the result.

        psm_rlwe::RlweMembershipResponses rlwe_membership_responses =
            std::move(status_or_responses).value();

        // Ensure the existence of one membership response. Then, verify that it
        // is regarding the current PSM ID.
        if (rlwe_membership_responses.membership_responses_size() != 1 ||
            rlwe_membership_responses.membership_responses(0)
                    .plaintext_id()
                    .sensitive_id() != psm_rlwe_id_.sensitive_id()) {
          LOG(ERROR)
              << "PSM error: RLWE membership responses are either empty or its "
                 "first response's ID is not the same as the current PSM ID.";
          // TODO(crbug.com/1302982): Record that error separately and merge it
          // with PsmResult.
          StoreErrorAndStop(PsmResult::kEmptyQueryResponseError);
          return;
        }

        const bool membership_result =
            rlwe_membership_responses.membership_responses(0)
                .membership_response()
                .is_member();

        LOG(WARNING) << "PSM determination successful. Identifier "
                     << (membership_result ? "" : "not ")
                     << "present on the server";

        // Reset the |psm_request_job_| to allow another call to
        // CheckMembership.
        psm_request_job_.reset();

        // Stop the PSM timer.
        psm_timeout_.Stop();

        // Cache the decision in local_state, so that it is reused in case
        // the device reboots before completing OOBE.
        // Also, record the PSM determination timestamp and its execution
        // result in local state. Because both values will be used in the
        // DeviceRegisterRequest during the enrollment flow.
        local_state_->SetBoolean(prefs::kShouldRetrieveDeviceState,
                                 membership_result);
        local_state_->SetTime(prefs::kEnrollmentPsmDeterminationTime,
                              base::Time::Now());
        local_state_->SetInteger(
            prefs::kEnrollmentPsmResult,
            membership_result
                ? em::DeviceRegisterRequest::PSM_RESULT_SUCCESSFUL_WITH_STATE
                : em::DeviceRegisterRequest::
                      PSM_RESULT_SUCCESSFUL_WITHOUT_STATE);
        local_state_->CommitPendingWrite();

        std::move(on_completion_callback_)
            .Run(PsmResult::kSuccessfulDetermination);
        return;
      }
      case DM_STATUS_REQUEST_FAILED: {
        LOG(ERROR)
            << "PSM error: RLWE query request failed due to connection error";
        base::UmaHistogramSparse(kUMAPsmNetworkErrorCode + uma_suffix_,
                                 -net_error);
        StoreErrorAndStop(PsmResult::kConnectionError);
        return;
      }
      default: {
        LOG(ERROR)
            << "PSM error: RLWE query request failed due to server error";
        StoreErrorAndStop(PsmResult::kServerError);
        return;
      }
    }
  }

  // Returns a job config that has TYPE_PSM_REQUEST as job type and |callback|
  // will be executed on completion.
  std::unique_ptr<DMServerJobConfiguration> CreatePsmRequestJobConfiguration(
      DMServerJobConfiguration::Callback callback) {
    return std::make_unique<DMServerJobConfiguration>(
        device_management_service_,
        DeviceManagementService::JobConfiguration::
            TYPE_PSM_HAS_DEVICE_STATE_REQUEST,
        random_device_id_,
        /*critical=*/true, DMAuth::NoAuth(),
        /*oauth_token=*/absl::nullopt, url_loader_factory_,
        std::move(callback));
  }

  // Record UMA histogram for timing of successful PSM request.
  void RecordPsmSuccessTimeHistogram() {
    // These values determine bucketing of the histogram, they should not be
    // changed.
    static const base::TimeDelta kMin = base::Milliseconds(1);
    static const base::TimeDelta kMax = base::Seconds(25);
    static const int kBuckets = 50;

    base::TimeTicks now = base::TimeTicks::Now();
    if (!time_start_.is_null()) {
      base::TimeDelta delta = now - time_start_;
      base::UmaHistogramCustomTimes(kUMAPsmSuccessTime, delta, kMin, kMax,
                                    kBuckets);
    }
  }

  // PSM RLWE client, used for preparing PSM requests and parsing PSM responses.
  std::unique_ptr<PrivateMembershipRlweClient> psm_rlwe_client_;

  // Randomly generated device id for the PSM requests.
  std::string random_device_id_;

  // The loader factory to use to perform PSM requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned by PsmHelper. Its used to communicate with the device management
  // service.
  DeviceManagementService* device_management_service_;

  // Its being used for both PSM requests e.g. RLWE OPRF request and RLWE query
  // request.
  std::unique_ptr<DeviceManagementService::Job> psm_request_job_;

  // Callback will be triggered upon completing of the protocol.
  CompletionCallback on_completion_callback_;

  // PrefService where the PSM protocol result is cached.
  PrefService* const local_state_;

  // PSM identifier, which is going to be used while preparing the PSM requests.
  psm_rlwe::RlwePlaintextId psm_rlwe_id_;

  // A timer that puts a hard limit on the maximum time to wait for PSM
  // protocol.
  base::OneShotTimer psm_timeout_;

  // The time when the PSM request started.
  base::TimeTicks time_start_;

  // Represents the last PSM protocol execution result.
  absl::optional<PsmResult> last_psm_execution_result_;

  // The UMA histogram suffix. It's set only to ".InitialEnrollment" for an
  // |AutoEnrollmentClient| until PSM will support FRE.
  const std::string uma_suffix_ = kUMASuffixInitialEnrollment;

  // A sequence checker to prevent the race condition of having the possibility
  // of the destructor being called and any of the callbacks.
  SEQUENCE_CHECKER(sequence_checker_);
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
    PrivateMembershipRlweClient::Factory* psm_rlwe_client_factory,
    PsmRlweIdProvider* psm_rlwe_id_provider) {
  return base::WrapUnique(new AutoEnrollmentClientImpl(
      progress_callback, device_management_service, local_state,
      url_loader_factory,
      /*device_identifier_provider_fre=*/nullptr,
      std::make_unique<StateDownloadMessageProcessorInitialEnrollment>(
          device_serial_number, device_brand_code),
      power_initial, power_limit, kUMASuffixInitialEnrollment,
      std::make_unique<PsmHelper>(device_management_service, url_loader_factory,
                                  local_state, psm_rlwe_client_factory,
                                  psm_rlwe_id_provider)));
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
    std::unique_ptr<PsmHelper> private_set_membership_helper)
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
      psm_helper_(std::move(private_set_membership_helper)),
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
  DCHECK(psm_helper_);

  const absl::optional<bool> private_set_membership_server_state =
      psm_helper_->GetPsmCachedDecision();

  if (private_set_membership_server_state.has_value()) {
    has_server_state_ = std::move(private_set_membership_server_state);
    return true;
  }
  return false;
}

bool AutoEnrollmentClientImpl::IsClientForInitialEnrollment() const {
  return psm_helper_ != nullptr;
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
  DCHECK(psm_helper_);

  // Don't retry if the protocol had an error.
  if (psm_helper_->HasPsmError())
    return false;

  // If the PSM protocol is in progress, signal to the caller
  // that nothing else needs to be done.
  if (psm_helper_->IsCheckMembershipInProgress())
    return true;

  if (RetrievePsmCachedDecision()) {
    LOG(WARNING) << "PSM Cached: psm_server_state="
                 << has_server_state_.value();
    return false;
  } else {
    psm_helper_->CheckMembership(
        base::BindOnce(&AutoEnrollmentClientImpl::HandlePsmCompletion,
                       base::Unretained(this)));
    return true;
  }
}

void AutoEnrollmentClientImpl::HandlePsmCompletion(PsmResult psm_result) {
  switch (psm_result) {
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
