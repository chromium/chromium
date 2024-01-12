// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_DMSERVER_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_DMSERVER_CLIENT_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_state.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace private_membership::rlwe {
class PrivateMembershipRlweClient;
}  // namespace private_membership::rlwe

namespace policy::psm {

class RlweDmserverClientImpl : public RlweDmserverClient {
 public:
  using PlaintextId = private_membership::rlwe::RlwePlaintextId;
  using OprfResponse =
      private_membership::rlwe::PrivateMembershipRlweOprfResponse;
  using RlweClient = private_membership::rlwe::PrivateMembershipRlweClient;
  using RlweClientFactory = base::RepeatingCallback<std::unique_ptr<RlweClient>(
      private_membership::rlwe::RlweUseCase,
      const private_membership::rlwe::RlwePlaintextId&)>;

  // Creates PSM RLWE client that generates and holds a randomly generated
  // key.
  static std::unique_ptr<RlweClient> Create(
      private_membership::rlwe::RlweUseCase use_case,
      const PlaintextId& plaintext_id);

  // `device_management_service`, `url_loader_factory`.
  // `device_management_service` must outlive RlweDmserverClientImpl.
  // `rlwe_client_factory` must be callable and must not yield nullptr.
  RlweDmserverClientImpl(
      DeviceManagementService* device_management_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const PlaintextId& plaintext_id,
      RlweClientFactory rlwe_client_factory);

  // Disallow copy constructor and assignment operator.
  RlweDmserverClientImpl(const RlweDmserverClientImpl&) = delete;
  RlweDmserverClientImpl& operator=(const RlweDmserverClientImpl&) = delete;

  // Cancels the ongoing PSM operation, if any (without calling the operation's
  // callbacks).
  ~RlweDmserverClientImpl() override;

  // Determines membership for the |psm_rlwe_client_|.
  void CheckMembership(CompletionCallback callback) override;

  // Returns true if the PSM protocol is still running,
  // otherwise false.
  bool IsCheckMembershipInProgress() const override;

 private:
  // Records PSM execution result, and stops the protocol.
  void RecordErrorAndStop(ResultHolder result);
  void RecordErrorAndStop(RlweResult result) {
    RecordErrorAndStop(ResultHolder(result));
  }
  void RecordErrorAndStop(AutoEnrollmentDMServerError error) {
    RecordErrorAndStop(ResultHolder(error));
  }

  // Constructs and sends the PSM RLWE OPRF request.
  void SendRlweOprfRequest();

  // If the completion was successful, then it makes another request to
  // DMServer for performing phase two.
  void OnRlweOprfRequestCompletion(DMServerJobResult result);

  // Constructs and sends the PSM RLWE Query request.
  void SendRlweQueryRequest(
      const enterprise_management::PrivateSetMembershipResponse& psm_response);

  // If the completion was successful, then it will parse the result and call
  // the |on_completion_callback_| for |psm_id_|.
  void OnRlweQueryRequestCompletion(const OprfResponse& oprf_response,
                                    DMServerJobResult result);

  // Returns a job config that has TYPE_PSM_REQUEST as job type and |callback|
  // will be executed on completion.
  std::unique_ptr<DMServerJobConfiguration> CreatePsmRequestJobConfiguration(
      DMServerJobConfiguration::Callback callback);

  // Record UMA histogram for timing of successful PSM request.
  void RecordPsmSuccessTimeHistogram();

  // PSM RLWE plaintext id for logging purposes.
  PlaintextId plaintext_id_;

  // PSM RLWE client, used for preparing PSM requests and parsing PSM responses.
  std::unique_ptr<RlweClient> psm_rlwe_client_;

  // Randomly generated device id for the PSM requests.
  std::string random_device_id_;

  // The loader factory to use to perform PSM requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Unowned by RlweDmserverClientImpl. Its used to communicate with the
  // device management service.
  const raw_ptr<DeviceManagementService, DanglingUntriaged>
      device_management_service_;

  // Its being used for both PSM requests e.g. RLWE OPRF request and RLWE query
  // request.
  std::unique_ptr<DeviceManagementService::Job> psm_request_job_;

  // Callback will be triggered upon completing of the protocol.
  CompletionCallback on_completion_callback_;

  // The time when the PSM request started.
  base::TimeTicks time_start_;

  // The UMA histogram suffix. It's set only to ".InitialEnrollment" for an
  // |AutoEnrollmentClient| until PSM will support FRE.
  const std::string uma_suffix_ = kUMASuffixInitialEnrollment;

  // A sequence checker to prevent the race condition of having the possibility
  // of the destructor being called and any of the callbacks.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policy::psm

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_DMSERVER_CLIENT_IMPL_H_
