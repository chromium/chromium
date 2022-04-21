// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

class PrefRegistrySimple;
class PrefService;

namespace enterprise_management {
class DeviceManagementResponse;
}

namespace policy {

class PsmRlweIdProvider;

// A class that handles all communications related to PSM protocol with
// DMServer. Also, upon successful determination, it caches the membership state
// of a given identifier in the local_state PrefService. Upon a failed
// determination it won't allow another membership check.
class PsmHelper;

// Indicates all possible PSM protocol results after it has executed
// successfully or terminated because of an error or timeout. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class PsmResult {
  kSuccessfulDetermination = 0,
  kCreateRlweClientLibraryError = 1,
  kCreateOprfRequestLibraryError = 2,
  kCreateQueryRequestLibraryError = 3,
  kProcessingQueryResponseLibraryError = 4,
  kEmptyOprfResponseError = 5,
  kEmptyQueryResponseError = 6,
  kConnectionError = 7,
  kServerError = 8,
  kTimeout = 9,
  kMaxValue = kTimeout,
};

// Interacts with the device management service and determines whether this
// machine should automatically enter the Enterprise Enrollment screen during
// OOBE.
class AutoEnrollmentClientImpl
    : public AutoEnrollmentClient,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // Provides device identifier for Forced Re-Enrollment (FRE), where the
  // server-backed state key is used. It will set the identifier for the
  // DeviceAutoEnrollmentRequest.
  class DeviceIdentifierProviderFRE;

  // Subclasses of this class generate the request to download the device state
  // (after determining that there is server-side device state) and parse the
  // response.
  class StateDownloadMessageProcessor;

  class FactoryImpl : public Factory {
   public:
    FactoryImpl();

    FactoryImpl(const FactoryImpl&) = delete;
    FactoryImpl& operator=(const FactoryImpl&) = delete;

    ~FactoryImpl() override;

    std::unique_ptr<AutoEnrollmentClient> CreateForFRE(
        const ProgressCallback& progress_callback,
        DeviceManagementService* device_management_service,
        PrefService* local_state,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const std::string& server_backed_state_key,
        int power_initial,
        int power_limit) override;

    std::unique_ptr<AutoEnrollmentClient> CreateForInitialEnrollment(
        const ProgressCallback& progress_callback,
        DeviceManagementService* device_management_service,
        PrefService* local_state,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        const std::string& device_serial_number,
        const std::string& device_brand_code,
        int power_initial,
        int power_limit,
        PrivateMembershipRlweClient::Factory* psm_rlwe_client_factory,
        PsmRlweIdProvider* psm_rlwe_id_provider) override;
  };

  AutoEnrollmentClientImpl(const AutoEnrollmentClientImpl&) = delete;
  AutoEnrollmentClientImpl& operator=(const AutoEnrollmentClientImpl&) = delete;

  ~AutoEnrollmentClientImpl() override;

  // Registers preferences in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void Start() override;
  void Retry() override;
  void CancelAndDeleteSoon() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 private:
  typedef bool (AutoEnrollmentClientImpl::*RequestCompletionHandler)(
      DeviceManagementService::Job*,
      DeviceManagementStatus,
      int,
      const enterprise_management::DeviceManagementResponse&);

  AutoEnrollmentClientImpl(
      const ProgressCallback& progress_callback,
      DeviceManagementService* device_management_service,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<DeviceIdentifierProviderFRE>
          device_identifier_provider_fre,
      std::unique_ptr<StateDownloadMessageProcessor>
          state_download_message_processor,
      int power_initial,
      int power_limit,
      std::string uma_suffix,
      std::unique_ptr<PsmHelper> psm_helper);

  // Tries to load the result of a previous execution of the protocol from
  // local state. Returns true if that decision has been made and is valid.
  bool GetCachedDecision();

  // Returns true if PSM has a cached decision, then store its value locally
  // (i.e. store it in |has_server_state_|). Otherwise, false.
  bool RetrievePsmCachedDecision();

  // Returns true if the current client got created for initial enrollment use
  // case. Otherwise, false.
  bool IsClientForInitialEnrollment() const;

  // Returns true if the device has a server-backed state and its state hasn't
  // been retrieved yet. Otherwise, false.
  bool ShouldSendDeviceStateRequest() const;

  // For detailed design, see go/psm-source-of-truth-initial-enrollment.
  // Kicks protocol processing, restarting the current step if applicable.
  // Returns true if progress has been made, false if the protocol is done.
  bool RetryStep();

  // Retries running PSM protocol, if it is possible to start it.
  // Returns true if the protocol is in progress, false if the protocol is done
  // or had an error.
  // Note that the PSM protocol is only performed once per OOBE flow.
  bool PsmRetryStep();

  // Calls `NextStep` in case of successful execution of PSM protocol.
  // Otherwise, reports the failure reason of PSM protocol execution.
  void HandlePsmCompletion(PsmResult psm_result);

  // Cleans up and invokes |progress_callback_|.
  void ReportProgress(AutoEnrollmentState state);

  // Calls RetryStep() to make progress or determine that all is done. In the
  // latter case, calls ReportProgress().
  void NextStep();

  // Sends an auto-enrollment check request to the device management service.
  void SendBucketDownloadRequest();

  // Sends a device state download request to the device management service.
  void SendDeviceStateRequest();

  // Runs the response handler for device management requests and calls
  // NextStep().
  void HandleRequestCompletion(
      RequestCompletionHandler handler,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Parses the server response to a bucket download request.
  bool OnBucketDownloadRequestCompletion(
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Parses the server response to a device state request.
  bool OnDeviceStateRequestCompletion(
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Returns true if the identifier hash provided by
  // |device_identifier_provider_fre_| is contained in |hashes|.
  bool IsIdHashInProtobuf(
      const google::protobuf::RepeatedPtrField<std::string>& hashes);

  // Updates UMA histograms for bucket download timings.
  void UpdateBucketDownloadTimingHistograms();

  // Updates the UMA histogram for successful hash dance.
  void RecordHashDanceSuccessTimeHistogram();

  // Callback to invoke when the protocol generates a relevant event. This can
  // be either successful completion or an error that requires external action.
  ProgressCallback progress_callback_;

  // Current state.
  AutoEnrollmentState state_;

  // Indicates whether the device has a server-backed state or not, regardless
  // of which protocol (i.e. PSM or Hash dance) collected that information.
  // Note that if it doesn't have an associated value after starting the auto
  // enrollment client, then the used protocol failed to collect that
  // information.
  absl::optional<bool> has_server_state_;

  // Whether the download of server-kept device state completed successfully.
  bool device_state_available_;

  // Randomly generated device id for the auto-enrollment requests.
  std::string device_id_;

  // Power-of-2 modulus to try next.
  int current_power_;

  // Power of the maximum power-of-2 modulus that this client will accept from
  // a retry response from the server.
  int power_limit_;

  // Number of requests for a different modulus received from the server.
  // Used to determine if the server keeps asking for different moduli.
  int modulus_updates_received_;

  // Used to communicate with the device management service.
  DeviceManagementService* device_management_service_;
  // Indicates whether Hash dance i.e. DeviceAutoEnrollmentRequest or
  // DeviceStateRetrievalRequest is in progress. Note that is not affected by
  // PSM protocol, whether it's in progress or not.
  std::unique_ptr<DeviceManagementService::Job> request_job_;

  // PrefService where the protocol's results are cached.
  PrefService* local_state_;

  // The loader factory to use to perform the auto enrollment request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Specifies the device identifier for FRE and its corresponding hash.
  std::unique_ptr<DeviceIdentifierProviderFRE> device_identifier_provider_fre_;

  // Fills and parses state retrieval request / response.
  std::unique_ptr<StateDownloadMessageProcessor>
      state_download_message_processor_;

  // Obtains the device state using PSM protocol.
  std::unique_ptr<PsmHelper> psm_helper_;

  // Times used to determine the duration of the protocol, and the extra time
  // needed to complete after the signin was complete.
  // If |hash_dance_time_start_| is not null, the protocol is still running.
  // If |time_extra_start_| is not null, the protocol is still running but our
  // owner has relinquished ownership.
  base::TimeTicks hash_dance_time_start_;
  base::TimeTicks time_extra_start_;

  // The time when the bucket download part of the protocol started.
  base::TimeTicks time_start_bucket_download_;

  // The UMA histogram suffix. Will be ".ForcedReenrollment" for an
  // |AutoEnrollmentClient| used for FRE and ".InitialEnrollment" for an
  // |AutoEnrollmentclient| used for initial enrollment.
  const std::string uma_suffix_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_IMPL_H_
