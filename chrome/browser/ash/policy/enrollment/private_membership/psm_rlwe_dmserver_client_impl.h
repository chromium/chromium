// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_DMSERVER_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_DMSERVER_CLIENT_IMPL_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_dmserver_client.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_id_provider.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace enterprise_management {
class DeviceManagementResponse;
}  // namespace enterprise_management

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace policy {

class PsmRlweIdProvider;

class PsmRlweDmserverClientImpl : public PsmRlweDmserverClient {
 public:
  // The PsmRlweDmserverClientImpl doesn't take ownership of
  // |device_management_service|, |psm_rlwe_client_factory| and
  // |psm_rlwe_id_provider|. All of them must not be nullptr. Also,
  // |device_management_service| must outlive PsmRlweDmserverClientImpl.
  PsmRlweDmserverClientImpl(
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrivateMembershipRlweClient::Factory* psm_rlwe_client_factory,
      PsmRlweIdProvider* psm_rlwe_id_provider);

  // Timeout for running PSM protocol.
  static constexpr base::TimeDelta kPsmTimeout = base::Seconds(15);

  // Disallow copy constructor and assignment operator.
  PsmRlweDmserverClientImpl(const PsmRlweDmserverClientImpl&) = delete;
  PsmRlweDmserverClientImpl& operator=(const PsmRlweDmserverClientImpl&) =
      delete;

  // Cancels the ongoing PSM operation, if any (without calling the operation's
  // callbacks).
  ~PsmRlweDmserverClientImpl() override;

  // Determines membership for the |psm_rlwe_id_|.
  void CheckMembership(CompletionCallback callback) override;

  // Returns true if the PSM protocol is still running,
  // otherwise false.
  bool IsCheckMembershipInProgress() const override;

 private:
  // Records PSM execution result, and stops the protocol.
  void StoreErrorAndStop(PsmResult psm_result);

  // Constructs and sends the PSM RLWE OPRF request.
  void SendPsmRlweOprfRequest();

  // If the completion was successful, then it makes another request to
  // DMServer for performing phase two.
  void OnRlweOprfRequestCompletion(
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Constructs and sends the PSM RLWE Query request.
  void SendPsmRlweQueryRequest(
      const enterprise_management::PrivateSetMembershipResponse& psm_response);

  // If the completion was successful, then it will parse the result and call
  // the |on_completion_callback_| for |psm_id_|.
  void OnRlweQueryRequestCompletion(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response,
      DeviceManagementService::Job* job,
      DeviceManagementStatus status,
      int net_error,
      const enterprise_management::DeviceManagementResponse& response);

  // Returns a job config that has TYPE_PSM_REQUEST as job type and |callback|
  // will be executed on completion.
  std::unique_ptr<DMServerJobConfiguration> CreatePsmRequestJobConfiguration(
      DMServerJobConfiguration::Callback callback);

  // Record UMA histogram for timing of successful PSM request.
  void RecordPsmSuccessTimeHistogram();

  // PSM RLWE client, used for preparing PSM requests and parsing PSM responses.
  std::unique_ptr<PrivateMembershipRlweClient> psm_rlwe_client_;

  // Randomly generated device id for the PSM requests.
  std::string random_device_id_;

  // The loader factory to use to perform PSM requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned by PsmRlweDmserverClientImpl. Its used to communicate with the
  // device management service.
  DeviceManagementService* const device_management_service_;

  // Its being used for both PSM requests e.g. RLWE OPRF request and RLWE query
  // request.
  std::unique_ptr<DeviceManagementService::Job> psm_request_job_;

  // Callback will be triggered upon completing of the protocol.
  CompletionCallback on_completion_callback_;

  // PSM identifier, which is going to be used while preparing the PSM requests.
  private_membership::rlwe::RlwePlaintextId psm_rlwe_id_;

  // A timer that puts a hard limit on the maximum time to wait for PSM
  // protocol.
  base::OneShotTimer psm_timeout_;

  // The time when the PSM request started.
  base::TimeTicks time_start_;

  // Represents the last PSM protocol execution result.
  absl::optional<ResultHolder> last_psm_execution_result_;

  // The UMA histogram suffix. It's set only to ".InitialEnrollment" for an
  // |AutoEnrollmentClient| until PSM will support FRE.
  const std::string uma_suffix_ = kUMASuffixInitialEnrollment;

  // A sequence checker to prevent the race condition of having the possibility
  // of the destructor being called and any of the callbacks.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_DMSERVER_CLIENT_IMPL_H_
