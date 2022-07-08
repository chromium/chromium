// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "services/network/public/cpp/network_connection_tracker.h"

class PrefRegistrySimple;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class DeviceManagementService;
class PsmRlweDmserverClient;

// Interacts with the device management service and determines whether this
// machine should automatically enter the Enterprise Enrollment screen during
// OOBE.
class AutoEnrollmentClientImpl
    : public AutoEnrollmentClient,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
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
        std::unique_ptr<PsmRlweDmserverClient> psm_rlwe_dmserver_client)
        override;
  };

  AutoEnrollmentClientImpl(const AutoEnrollmentClientImpl&) = delete;
  AutoEnrollmentClientImpl& operator=(const AutoEnrollmentClientImpl&) = delete;

  ~AutoEnrollmentClientImpl() override;

  // Registers preferences in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void Start() override;
  void Retry() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

 private:
  // Base class to handle server state availability requests.
  class ServerStateAvailabilityRequester;

  // Responsible for resolving server state availability status via auto
  // enrollment requests for force re-enrollment.
  class FREServerStateAvailabilityRequester;

  // Responsible for resolving server state availability status via private
  // membership check requests for initial enrollment.
  class InitialServerStateAvailabilityRequester;
  enum class ServerStateAvailabilityResult;

  // Responsible for resolving server state status for both Forced Re-Enrollment
  // (FRE) and Initial Enrollment.
  class ServerStateRetriever;
  enum class ServerStateRetrievalResult;

  AutoEnrollmentClientImpl(
      const ProgressCallback& progress_callback,
      PrefService* local_state,
      std::unique_ptr<ServerStateAvailabilityRequester>
          server_state_avalability_requester,
      std::unique_ptr<ServerStateRetriever> server_state_retriever);

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
  void HandlePsmCompletion(ServerStateAvailabilityResult result);

  // Cleans up and invokes |progress_callback_|.
  void ReportProgress(AutoEnrollmentState state);

  // Calls RetryStep() to make progress or determine that all is done. In the
  // latter case, calls ReportProgress().
  void NextStep();

  // Sends an auto-enrollment check request to the device management service.
  void SendBucketDownloadRequest();

  // Sends a device state download request to the device management service.
  void SendDeviceStateRequest();

  // Handles result of server state availability request. Proceeds to the next
  // step on success. Reports failure otherwise.
  void OnBucketDownloadRequestCompleted(ServerStateAvailabilityResult result);

  // Handles result of server state retrieval request. Proceeds to the next
  // step on success. Reports failure otherwise.
  void OnStateRetrievalCompleted(ServerStateRetrievalResult result);

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

  // PrefService where the protocol's results are cached.
  PrefService* local_state_;

  // Sends server state availability request and parses response. Reports
  // results.
  std::unique_ptr<ServerStateAvailabilityRequester>
      server_state_avalability_requester_;

  // Sends server state retrieval request and parses response. Reports results.
  std::unique_ptr<ServerStateRetriever> server_state_retriever_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_IMPL_H_
