// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"

class PrefRegistrySimple;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy::psm {
class RlweDmserverClient;
}  // namespace policy::psm

namespace ash {
class OobeConfiguration;
}  // namespace ash

namespace policy {

class DeviceManagementService;

// Interacts with the device management service and determines whether this
// machine should automatically enter the Enterprise Enrollment screen during
// OOBE.
class AutoEnrollmentClientImpl final : public AutoEnrollmentClient {
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
        std::unique_ptr<psm::RlweDmserverClient> psm_rlwe_dmserver_client,
        ash::OobeConfiguration* oobe_config) override;
  };

  // Registers preferences in local state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  AutoEnrollmentClientImpl(const AutoEnrollmentClientImpl&) = delete;
  AutoEnrollmentClientImpl& operator=(const AutoEnrollmentClientImpl&) = delete;

  ~AutoEnrollmentClientImpl() override;

  // policy::AutoEnrollmentClient:
  void Start() override;
  void Retry() override;

 private:
  // Base class to handle server state availability requests.
  class ServerStateAvailabilityRequester;

  // Responsible for resolving server state availability status via auto
  // enrollment requests for force re-enrollment.
  class FREServerStateAvailabilityRequester;

  // Responsible for resolving server state availability status via private
  // membership check requests for initial enrollment.
  class InitialServerStateAvailabilityRequester;

  // Responsible for resolving availability status by checking enrollment
  // token presence, to determine whether the device should retrieve server
  // state for token-based initial enrollment.
  class TokenBasedEnrollmentStateAvailabilityRequester;

  enum class ServerStateAvailabilitySuccess;
  using ServerStateAvailabilityResult =
      base::expected<ServerStateAvailabilitySuccess, AutoEnrollmentError>;

  // Responsible for resolving server state status for both Forced Re-Enrollment
  // (FRE) and Initial Enrollment.
  class ServerStateRetriever;
  using ServerStateRetrievalResult = base::expected<void, AutoEnrollmentError>;

  enum class State {
    // Initial state until `Start` or `Retry` are called. Resolves into
    // `kRequestingServerStateAvailability`.
    kIdle,
    // Indicates server state availability request is in progress.
    // Reached from:
    // * `kIdle` after `Start`.
    // * `kRequestServerStateAvailabilityError` on `Retry`.
    // Resolves into:
    // * `kRequestServerStateAvailabilitySuccess` if valid response.
    // * `kRequestServerStateAvailabilityError` otherwise.
    // * `kFinished` if response is valid and server state is not available.
    kRequestingServerStateAvailability,
    // Indicate connection or server errors during server state availability
    // request.
    // Reached from:
    // * `kRequestingServerStateAvailability` if request fails.
    // Resolves into:
    // * `kRequestingServerStateAvailability` on `Retry`.
    kRequestServerStateAvailabilityError,
    // Indicates success of state availability request.
    // Reached from:
    // * `kRequestingServerStateAvailability` if request is successful and
    //   server state is available.
    // Resolves into:
    // * `kRequestingStateRetrieval` unconditionally.
    kRequestServerStateAvailabilitySuccess,
    // Indicates server state retrieval request is in progress.
    // Reached from:
    // * `kRequestServerStateAvailabilitySuccess` after server state
    // availability request succeeded the state is available.
    // * `kRequestStateRetrievalError` on `Retry`.
    // Resolves into:
    // * `kRequestStateRetrievalError` if request fails due to
    // connection error or invalid response.
    // * `kFinished` if response is valid and state is retrieved.
    kRequestingStateRetrieval,
    // Indicate connection or server errors during state retrieval request.
    // Reached from:
    // * `kRequestingStateRetrieval` if request fails.
    // Resolves into:
    // * `kRequestingStateRetrieval` on `Retry`.
    kRequestStateRetrievalError,
    // Indicates the client has finished its requests and has the answer for
    // final `AutoEnrollmentState` status.
    // Reached from:
    // * `kRequestingServerStateAvailability`
    // * `kRequestingStateRetrieval`
    // Resolves into nothing. It's the final state.
    kFinished,
  };

  AutoEnrollmentClientImpl(
      ProgressCallback progress_callback,
      std::unique_ptr<ServerStateAvailabilityRequester>
          server_state_avalability_requester,
      std::unique_ptr<ServerStateRetriever> server_state_retriever);

  // Sends an auto-enrollment check or psm membership check request to the
  // device management service.
  void RequestServerStateAvailability();

  // Handles result of server state availability request. Proceeds to the next
  // step on success. Reports failure otherwise.
  void OnServerStateAvailabilityCompleted(ServerStateAvailabilityResult result);

  // Sends a device state download request to the device management service.
  void RequestStateRetrieval();

  // Handles result of server state retrieval request. Proceeds to the next
  // step on success. Reports failure otherwise.
  void OnStateRetrievalCompleted(ServerStateRetrievalResult result);

  void ReportProgress(AutoEnrollmentState auto_enrollment_state) const;

  void ReportFinished() const;

  State state_ = State::kIdle;

  // Callback to invoke when the protocol generates a relevant event. This can
  // be either successful completion or an error that requires external action.
  ProgressCallback progress_callback_;

  // Sends server state availability request and parses response. Reports
  // results.
  std::unique_ptr<ServerStateAvailabilityRequester>
      server_state_availability_requester_;

  // Sends server state retrieval request and parses response. Reports results.
  std::unique_ptr<ServerStateRetriever> server_state_retriever_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_AUTO_ENROLLMENT_CLIENT_IMPL_H_
