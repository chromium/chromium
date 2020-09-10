// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/auto_enrollment_client_impl.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
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
#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace psm_rlwe = private_membership::rlwe;
namespace em = enterprise_management;

namespace policy {

namespace {

using EnrollmentCheckType =
    em::DeviceAutoEnrollmentRequest::EnrollmentCheckType;

// Timeout for running private set membership protocol.
constexpr base::TimeDelta kPrivateSetMembershipTimeout =
    base::TimeDelta::FromSeconds(15);

// UMA histogram names.
constexpr char kUMAHashDanceSuccessTime[] =
    "Enterprise.AutoEnrollmentHashDanceSuccessTime";
constexpr char kUMAPrivateSetMembershipHashDanceComparison[] =
    "Enterprise.AutoEnrollmentPrivateSetMembershipHashDanceComparison";
constexpr char kUMAPrivateSetMembershipSuccessTime[] =
    "Enterprise.AutoEnrollmentPrivateSetMembershipSuccessTime";
constexpr char kUMAPrivateSetMembershipRequestStatus[] =
    "Enterprise.AutoEnrollmentPrivateSetMembershipRequestStatus";

// The following histogram names where added before private set membership
// existed. They are only recorded for hash dance.

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

}  // namespace

psm_rlwe::RlwePlaintextId ConstructDeviceRlweId(
    const std::string& device_serial_number,
    const std::string& device_rlz_brand_code) {
  psm_rlwe::RlwePlaintextId rlwe_id;

  std::string rlz_brand_code_hex = base::HexEncode(
      device_rlz_brand_code.data(), device_rlz_brand_code.size());

  rlwe_id.set_sensitive_id(rlz_brand_code_hex + "/" + device_serial_number);
  return rlwe_id;
}

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

class PrivateSetMembershipHelper {
 public:
  // Callback will be triggered after completing the protocol, in case of a
  // successful determination or stopping due to an error. Also, the bool result
  // is ignored.
  using CompletionCallback = base::OnceCallback<bool()>;

  // The PrivateSetMembershipHelper doesn't take ownership of
  // |device_management_service| and |local_state|. Also, both must not be
  // nullptr. The |device_management_service| and |local_state| must outlive
  // PrivateSetMembershipHelper.
  PrivateSetMembershipHelper(
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* local_state,
      psm_rlwe::RlwePlaintextId psm_rlwe_id)
      : random_device_id_(base::GenerateGUID()),
        url_loader_factory_(url_loader_factory),
        device_management_service_(device_management_service),
        local_state_(local_state),
        psm_rlwe_id_(std::move(psm_rlwe_id)) {
    CHECK(device_management_service);
    DCHECK(local_state_);

    // Create PSM client for |psm_rlwe_id_| with use case as CROS_DEVICE_STATE.
    std::vector<psm_rlwe::RlwePlaintextId> psm_ids = {psm_rlwe_id_};
    auto status_or_client = psm_rlwe::PrivateMembershipRlweClient::Create(
        psm_rlwe::RlweUseCase::CROS_DEVICE_STATE, psm_ids);
    if (!status_or_client.ok()) {
      // If the private set membership RLWE client hasn't been created
      // successfully, then report the error and don't run the protocol.
      LOG(ERROR)
          << "PSM error: unexpected internal logic error during creating "
             "PSM RLWE client";
      has_private_set_membership_error_ = true;
      return;
    }

    private_set_membership_rlwe_client_ = std::move(status_or_client).value();
  }

  // Disallow copy constructor and assignment operator.
  PrivateSetMembershipHelper(const PrivateSetMembershipHelper&) = delete;
  PrivateSetMembershipHelper& operator=(const PrivateSetMembershipHelper&) =
      delete;

  // Cancels the ongoing private set membership operation, if any (without
  // calling the operation's callbacks).
  ~PrivateSetMembershipHelper() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // Determines the private set membership for the |psm_rlwe_id_|. Then, will
  // call |callback| upon completing the protocol, whether it finished with a
  // successful determination or stopped in case of errors. Also, the |callback|
  // has to be non-null. In case a request is already in progress, the callback
  // is called immediately.
  void CheckMembership(CompletionCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(callback);

    // Ignore new calls and execute their completion |callback|, if any error
    // occurred while running private set membership previously, or in case the
    // requests from previous call didn't finish yet.
    if (has_private_set_membership_error_ || psm_request_job_) {
      std::move(callback).Run();
      return;
    }

    // Report the psm attempt and start the timer to measure successful private
    // set membership requests.
    base::UmaHistogramEnumeration(kUMAPrivateSetMembershipRequestStatus,
                                  PrivateSetMembershipStatus::kAttempt);
    time_start_ = base::TimeTicks::Now();

    on_completion_callback_ = std::move(callback);

    // Start the protocol and its timeout timer.
    private_set_membership_timeout_.Start(
        FROM_HERE, kPrivateSetMembershipTimeout,
        base::BindOnce(&PrivateSetMembershipHelper::OnTimeout,
                       base::Unretained(this)));
    SendPrivateSetMembershipRlweOprfRequest();
  }

  // Sets the |private_set_membership_rlwe_client_| and |psm_rlwe_id_| for
  // testing.
  void SetRlweClientAndIdForTesting(
      std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>
          private_set_membership_rlwe_client,
      psm_rlwe::RlwePlaintextId psm_rlwe_id) {
    private_set_membership_rlwe_client_ =
        std::move(private_set_membership_rlwe_client);
    psm_rlwe_id_ = std::move(psm_rlwe_id);
  }

  // Tries to load the result of a previous execution of the private set
  // memberhsip protocol from local state. Returns decision value if it has been
  // made and is valid, otherwise nullptr.
  const base::Value* GetPrivateSetMembershipCachedDecision() const {
    const PrefService::Preference* has_psm_server_state_pref =
        local_state_->FindPreference(prefs::kShouldRetrieveDeviceState);

    if (!has_psm_server_state_pref ||
        has_psm_server_state_pref->IsDefaultValue() ||
        !has_psm_server_state_pref->GetValue()->is_bool()) {
      return nullptr;
    }

    return has_psm_server_state_pref->GetValue();
  }

  // Indicate whether an error occurred while executing the private set
  // membership protocol.
  bool HasPrivateSetMembershipError() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return has_private_set_membership_error_;
  }

  // Returns true if the private set membership protocol is still running,
  // otherwise false.
  bool IsCheckMembershipInProgress() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return psm_request_job_ != nullptr;
  }

 private:
  void OnTimeout() {
    base::UmaHistogramEnumeration(kUMAPrivateSetMembershipRequestStatus,
                                  PrivateSetMembershipStatus::kTimeout);
    StoreErrorAndStop();
  }

  void StoreErrorAndStop() {
    // Record the error. Note that a timeout is also recorded as error.
    base::UmaHistogramEnumeration(kUMAPrivateSetMembershipRequestStatus,
                                  PrivateSetMembershipStatus::kError);

    // Stop the private set membership timer.
    private_set_membership_timeout_.Stop();

    // Stop the current |psm_request_job_|.
    psm_request_job_.reset();

    has_private_set_membership_error_ = true;
    std::move(on_completion_callback_).Run();
  }

  // Constructs and sends the private set membership RLWE OPRF request.
  void SendPrivateSetMembershipRlweOprfRequest() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Create RLWE OPRF request.
    const auto status_or_oprf_request =
        private_set_membership_rlwe_client_->CreateOprfRequest();
    if (!status_or_oprf_request.ok()) {
      // If the RLWE OPRF request hasn't been created successfully, then report
      // the error and stop the protocol.
      LOG(ERROR)
          << "PSM error: unexpected internal logic error during creating "
             "RLWE OPRF request";
      StoreErrorAndStop();
      return;
    }

    LOG(WARNING) << "PSM: prepare and send out the RLWE OPRF request";

    // Prepare the RLWE OPRF request job.
    // The passed callback will not be called if |psm_request_job_| is
    // destroyed, so it's safe to use base::Unretained.
    std::unique_ptr<DMServerJobConfiguration> config =
        CreatePsmRequestJobConfiguration(base::BindOnce(
            &PrivateSetMembershipHelper::OnRlweOprfRequestCompletion,
            base::Unretained(this)));

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

    switch (status) {
      case DM_STATUS_SUCCESS: {
        // Check if the RLWE OPRF response is empty.
        if (!response.private_set_membership_response().has_rlwe_response() ||
            !response.private_set_membership_response()
                 .rlwe_response()
                 .has_oprf_response()) {
          LOG(ERROR) << "PSM error: empty OPRF RLWE response";
          StoreErrorAndStop();
          return;
        }

        LOG(WARNING) << "PSM RLWE OPRF request completed successfully";
        SendPrivateSetMembershipRlweQueryRequest(
            response.private_set_membership_response());
        return;
      }
      case DM_STATUS_REQUEST_FAILED: {
        LOG(ERROR)
            << "PSM error: RLWE OPRF request failed due to connection error";
        StoreErrorAndStop();
        return;
      }
      default: {
        LOG(ERROR) << "PSM error: RLWE OPRF request failed due to server error";
        StoreErrorAndStop();
        return;
      }
    }
  }

  // Constructs and sends the private set membership RLWE Query request.
  void SendPrivateSetMembershipRlweQueryRequest(
      const em::PrivateSetMembershipResponse& private_set_membership_response) {
    // Extract the oprf_response from |private_set_membership_response|.
    const psm_rlwe::PrivateMembershipRlweOprfResponse oprf_response =
        private_set_membership_response.rlwe_response().oprf_response();

    const auto status_or_query_request =
        private_set_membership_rlwe_client_->CreateQueryRequest(oprf_response);

    // Create RLWE query request.
    if (!status_or_query_request.ok()) {
      // If the RLWE query request hasn't been created successfully, then report
      // the error and stop the protocol.
      LOG(ERROR)
          << "PSM error: unexpected internal logic error during creating "
             "RLWE query request";
      StoreErrorAndStop();
      return;
    }

    LOG(WARNING) << "PSM: prepare and send out the RLWE query request";

    // Prepare the RLWE query request job.
    std::unique_ptr<DMServerJobConfiguration> config =
        CreatePsmRequestJobConfiguration(base::BindOnce(
            &PrivateSetMembershipHelper::OnRlweQueryRequestCompletion,
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

    switch (status) {
      case DM_STATUS_SUCCESS: {
        // Check if the RLWE query response is empty.
        if (!response.private_set_membership_response().has_rlwe_response() ||
            !response.private_set_membership_response()
                 .rlwe_response()
                 .has_query_response()) {
          LOG(ERROR) << "PSM error: empty query RLWE response";
          StoreErrorAndStop();
          return;
        }

        const psm_rlwe::PrivateMembershipRlweQueryResponse query_response =
            response.private_set_membership_response()
                .rlwe_response()
                .query_response();

        auto status_or_responses =
            private_set_membership_rlwe_client_->ProcessResponse(
                query_response);

        if (!status_or_responses.ok()) {
          // If the RLWE query response hasn't processed successfully, then
          // report the error and stop the protocol.
          LOG(ERROR) << "PSM error: unexpected internal logic error during "
                        "processing the "
                        "RLWE query response";
          StoreErrorAndStop();
          return;
        }

        LOG(WARNING) << "PSM query request completed successfully";

        base::UmaHistogramEnumeration(
            kUMAPrivateSetMembershipRequestStatus,
            PrivateSetMembershipStatus::kSuccessfulDetermination);
        RecordPrivateSetMembershipSuccessTimeHistogram();

        // The RLWE query response has been processed successfully. Extract
        // the membership response, and report the result.
        psm_rlwe::MembershipResponseMap membership_responses_map =
            std::move(status_or_responses).value();
        private_membership::MembershipResponse membership_response =
            membership_responses_map.Get(psm_rlwe_id_);

        LOG(WARNING) << "PSM determination successful. Identifier "
                     << (membership_response.is_member() ? "" : "not ")
                     << "present on the server";

        // Reset the |psm_request_job_| to allow another call to
        // CheckMembership.
        psm_request_job_.reset();

        // Stop the private set membership timer.
        private_set_membership_timeout_.Stop();

        // Cache the decision in local_state, so that it is reused in case
        // the device reboots before completing OOBE.
        local_state_->SetBoolean(prefs::kShouldRetrieveDeviceState,
                                 membership_response.is_member());
        local_state_->CommitPendingWrite();

        std::move(on_completion_callback_).Run();
        return;
      }
      case DM_STATUS_REQUEST_FAILED: {
        LOG(ERROR)
            << "PSM error: RLWE query request failed due to connection error";
        StoreErrorAndStop();
        return;
      }
      default: {
        LOG(ERROR)
            << "PSM error: RLWE query request failed due to server error";
        StoreErrorAndStop();
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
        /*oauth_token=*/base::nullopt, url_loader_factory_,
        std::move(callback));
  }

  // Record UMA histogram for timing of successful private set membership
  // request.
  void RecordPrivateSetMembershipSuccessTimeHistogram() {
    // These values determine bucketing of the histogram, they should not be
    // changed.
    static const base::TimeDelta kMin = base::TimeDelta::FromMilliseconds(1);
    static const base::TimeDelta kMax = base::TimeDelta::FromSeconds(25);
    static const int kBuckets = 50;

    base::TimeTicks now = base::TimeTicks::Now();
    if (!time_start_.is_null()) {
      base::TimeDelta delta = now - time_start_;
      base::UmaHistogramCustomTimes(kUMAPrivateSetMembershipSuccessTime, delta,
                                    kMin, kMax, kBuckets);
    }
  }

  // Private Set Membership RLWE client, used for preparing PSM requests and
  // parsing PSM responses.
  std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>
      private_set_membership_rlwe_client_;

  // Randomly generated device id for the private set membership requests.
  std::string random_device_id_;

  // The loader factory to use to perform private set membership requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned by PrivateSetMembershipHelper. Its used to communicate with the
  // device management service.
  DeviceManagementService* device_management_service_;

  // Its being used for both private set membership requests e.g. RLWE OPRF
  // request and RLWE query request.
  std::unique_ptr<DeviceManagementService::Job> psm_request_job_;

  // Callback will be triggered upon completing of the protocol.
  CompletionCallback on_completion_callback_;

  // PrefService where the private set membership protocol result is cached.
  PrefService* const local_state_;

  // Private Set Membership identifier, which is going to be used while
  // preparing the private set membership requests.
  psm_rlwe::RlwePlaintextId psm_rlwe_id_;

  // Indicates whether there was previously any error occurred while running
  // private set membership protocol.
  bool has_private_set_membership_error_ = false;

  // A timer that puts a hard limit on the maximum time to wait for private set
  // membership protocol.
  base::OneShotTimer private_set_membership_timeout_;

  // The time when the private set membership request started.
  base::TimeTicks time_start_;

  // A sequence checker to prevent the race condition of having the possibility
  // of the destructor being called and any of the callbacks.
  SEQUENCE_CHECKER(sequence_checker_);
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
    if (!response.has_device_initial_enrollment_state_response()) {
      LOG(ERROR) << "Server failed to provide initial enrollment response.";
      return base::nullopt;
    }

    return ParseInitialEnrollmentStateResponse(
        response.device_initial_enrollment_state_response());
  }

  static base::Optional<ParsedResponse> ParseInitialEnrollmentStateResponse(
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

  base::Optional<ParsedResponse> ParseResponse(
      const em::DeviceManagementResponse& response) override {
    if (!response.has_device_state_retrieval_response()) {
      LOG(ERROR) << "Server failed to provide auto-enrollment response.";
      return base::nullopt;
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
      power_initial, power_limit,
      /*power_outdated_server_detect=*/base::nullopt, kUMASuffixFRE,
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
      kUMASuffixInitialEnrollment,
      chromeos::AutoEnrollmentController::IsPrivateSetMembershipEnabled()
          ? std::make_unique<PrivateSetMembershipHelper>(
                device_management_service, url_loader_factory, local_state,
                ConstructDeviceRlweId(device_serial_number, device_brand_code))
          : nullptr));
}

AutoEnrollmentClientImpl::~AutoEnrollmentClientImpl() {
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
}

// static
void AutoEnrollmentClientImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShouldAutoEnroll, false);
  registry->RegisterIntegerPref(prefs::kAutoEnrollmentPowerLimit, -1);
  registry->RegisterBooleanPref(prefs::kShouldRetrieveDeviceState, false);
}

void AutoEnrollmentClientImpl::Start() {
  // (Re-)register the network change observer.
  content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(this);
  content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);

  // Drop the previous job and reset state.
  request_job_.reset();
  state_ = AUTO_ENROLLMENT_STATE_PENDING;
  time_start_ = base::TimeTicks::Now();
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
    time_extra_start_ = base::TimeTicks::Now();
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
    std::string uma_suffix,
    std::unique_ptr<PrivateSetMembershipHelper> private_set_membership_helper)
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
      private_set_membership_helper_(std::move(private_set_membership_helper)),
      uma_suffix_(uma_suffix),
      recorded_psm_hash_dance_comparison_(false) {
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
  if (PrivateSetMembershipRetryStep())
    return true;

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

bool AutoEnrollmentClientImpl::PrivateSetMembershipRetryStep() {
  // Don't retry if the protocol is disabled, protocol is still running, or an
  // error occurred while executing the protocol.
  if (!private_set_membership_helper_ ||
      private_set_membership_helper_->HasPrivateSetMembershipError() ||
      private_set_membership_helper_->IsCheckMembershipInProgress())
    return false;

  const base::Value* private_set_membership_server_state =
      private_set_membership_helper_->GetPrivateSetMembershipCachedDecision();

  if (private_set_membership_server_state) {
    LOG(WARNING) << "PSM Cached: psm_server_state="
                 << private_set_membership_server_state->GetBool();
    return false;
  } else {
    private_set_membership_helper_->CheckMembership(base::BindOnce(
        &AutoEnrollmentClientImpl::RetryStep, base::Unretained(this)));
    return true;
  }
}

void AutoEnrollmentClientImpl::SetPrivateSetMembershipRlweClientForTesting(
    std::unique_ptr<psm_rlwe::PrivateMembershipRlweClient>
        private_set_membership_rlwe_client,
    psm_rlwe::RlwePlaintextId& psm_rlwe_id) {
  if (!private_set_membership_helper_)
    return;

  DCHECK(private_set_membership_rlwe_client);
  private_set_membership_helper_->SetRlweClientAndIdForTesting(
      std::move(private_set_membership_rlwe_client), std::move(psm_rlwe_id));
}

void AutoEnrollmentClientImpl::ReportProgress(AutoEnrollmentState state) {
  state_ = state;
  // If hash dance finished with an error or result, record comparison with
  // private set membership. Note that hash dance might be retried but for
  // recording we only care about the first attempt.
  // If |private_set_membership_helper_| is non-null, a private set membership
  // request has been made at this point because it is executed before hash
  // dance.
  const bool has_hash_dance_result = (state != AUTO_ENROLLMENT_STATE_IDLE &&
                                      state != AUTO_ENROLLMENT_STATE_PENDING);
  if (private_set_membership_helper_ && !recorded_psm_hash_dance_comparison_ &&
      has_hash_dance_result) {
    RecordPrivateSetMembershipHashDanceComparison();
  }
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
  time_start_bucket_download_ = base::TimeTicks::Now();

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
    policy::DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  base::Optional<StateDownloadMessageProcessor::ParsedResponse>
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
  // These values determine bucketing of the histogram, they should not be
  // changed.
  // The minimum time can't be 0, must be at least 1.
  static const base::TimeDelta kMin = base::TimeDelta::FromMilliseconds(1);
  static const base::TimeDelta kMax = base::TimeDelta::FromMinutes(5);
  // However, 0 can still be sampled.
  static const base::TimeDelta kZero = base::TimeDelta::FromMilliseconds(0);
  static const int kBuckets = 50;

  base::TimeTicks now = base::TimeTicks::Now();
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

void AutoEnrollmentClientImpl::RecordHashDanceSuccessTimeHistogram() {
  // These values determine bucketing of the histogram, they should not be
  // changed.
  static const base::TimeDelta kMin = base::TimeDelta::FromMilliseconds(1);
  static const base::TimeDelta kMax = base::TimeDelta::FromSeconds(25);
  static const int kBuckets = 50;

  base::TimeTicks now = base::TimeTicks::Now();
  if (!time_start_.is_null()) {
    base::TimeDelta delta = now - time_start_;
    base::UmaHistogramCustomTimes(kUMAHashDanceSuccessTime + uma_suffix_, delta,
                                  kMin, kMax, kBuckets);
  }
}

void AutoEnrollmentClientImpl::RecordPrivateSetMembershipHashDanceComparison() {
  // Private set membership timeout is enforced in the helper class. This method
  // should only be called after private set membership request finished or ran
  // into timeout.
  DCHECK(private_set_membership_helper_);
  DCHECK(!private_set_membership_helper_->IsCheckMembershipInProgress());

  // Make sure to only record once per instance.
  recorded_psm_hash_dance_comparison_ = true;

  bool private_set_membership_decision =
      private_set_membership_helper_->GetPrivateSetMembershipCachedDecision();
  bool private_set_membership_error =
      private_set_membership_helper_->HasPrivateSetMembershipError();

  bool hash_dance_decision = has_server_state_;
  bool hash_dance_error = false;
  switch (state_) {
    case AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
    case AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
    case AUTO_ENROLLMENT_STATE_TRIGGER_ZERO_TOUCH:
    case AUTO_ENROLLMENT_STATE_DISABLED:
      hash_dance_error = false;
      break;
    case AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
    case AUTO_ENROLLMENT_STATE_SERVER_ERROR:
      hash_dance_error = true;
      break;
    // This method should only be called if hash dance finished.
    case AUTO_ENROLLMENT_STATE_IDLE:
    case AUTO_ENROLLMENT_STATE_PENDING:
    default:
      NOTREACHED();
  }

  auto comparison = PrivateSetMembershipHashDanceComparison::kEqualResults;
  if (!hash_dance_error && !private_set_membership_error) {
    comparison =
        (hash_dance_decision == private_set_membership_decision)
            ? PrivateSetMembershipHashDanceComparison::kEqualResults
            : PrivateSetMembershipHashDanceComparison::kDifferentResults;
  } else if (hash_dance_error && !private_set_membership_error) {
    comparison =
        PrivateSetMembershipHashDanceComparison::kPSMSuccessHashDanceError;
  } else if (!hash_dance_error && private_set_membership_error) {
    comparison =
        PrivateSetMembershipHashDanceComparison::kPSMErrorHashDanceSuccess;
  } else {
    comparison = PrivateSetMembershipHashDanceComparison::kBothError;
  }

  base::UmaHistogramEnumeration(kUMAPrivateSetMembershipHashDanceComparison,
                                comparison);
}

}  // namespace policy
