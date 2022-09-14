// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_dmserver_client_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_dmserver_client.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_id_provider.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace psm_rlwe = private_membership::rlwe;
namespace em = enterprise_management;

namespace policy {

PsmRlweDmserverClientImpl::PsmRlweDmserverClientImpl(
    DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrivateMembershipRlweClient::Factory* psm_rlwe_client_factory,
    PsmRlweIdProvider* psm_rlwe_id_provider)
    : random_device_id_(base::GenerateGUID()),
      url_loader_factory_(url_loader_factory),
      device_management_service_(device_management_service) {
  CHECK(device_management_service);
  CHECK(psm_rlwe_client_factory);
  CHECK(psm_rlwe_id_provider);

  psm_rlwe_id_ = psm_rlwe_id_provider->ConstructRlweId();

  // Create PSM client for |psm_rlwe_id_| with use case as CROS_DEVICE_STATE.
  std::vector<psm_rlwe::RlwePlaintextId> psm_ids = {psm_rlwe_id_};
  auto status_or_client = psm_rlwe_client_factory->Create(
      psm_rlwe::RlweUseCase::CROS_DEVICE_STATE, psm_ids);
  if (!status_or_client.ok()) {
    // If the PSM RLWE client hasn't been created successfully, then report
    // the error and don't run the protocol.
    LOG(ERROR) << "PSM error: unexpected internal logic error during creating "
                  "PSM RLWE client";
    last_psm_execution_result_ =
        ResultHolder(PsmResult::kCreateRlweClientLibraryError);
    base::UmaHistogramEnumeration(kUMAPsmResult + uma_suffix_,
                                  PsmResult::kCreateRlweClientLibraryError);
    return;
  }

  psm_rlwe_client_ = std::move(status_or_client).value();
}

PsmRlweDmserverClientImpl::~PsmRlweDmserverClientImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PsmRlweDmserverClientImpl::CheckMembership(CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  // Ignore new calls and execute `callback` with
  // |last_psm_execution_result_|, in case any error occurred while running
  // PSM previously.
  if (last_psm_execution_result_ &&
      last_psm_execution_result_.value().IsError()) {
    std::move(callback).Run(last_psm_execution_result_.value());
    return;
  }

  // There should not be any pending PSM requests.
  CHECK(!psm_request_job_);

  time_start_ = base::TimeTicks::Now();

  on_completion_callback_ = std::move(callback);

  SendPsmRlweOprfRequest();
}

bool PsmRlweDmserverClientImpl::IsCheckMembershipInProgress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return psm_request_job_ != nullptr;
}

void PsmRlweDmserverClientImpl::StoreErrorAndStop(PsmResult psm_result) {
  // Note that kUMAPsmResult histogram is only using initial enrollment as a
  // suffix until PSM support FRE.
  base::UmaHistogramEnumeration(kUMAPsmResult + uma_suffix_, psm_result);

  // Stop the current |psm_request_job_|.
  psm_request_job_.reset();

  last_psm_execution_result_ = ResultHolder(psm_result);
  std::move(on_completion_callback_).Run(last_psm_execution_result_.value());
}

void PsmRlweDmserverClientImpl::SendPsmRlweOprfRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create RLWE OPRF request.
  const auto status_or_oprf_request = psm_rlwe_client_->CreateOprfRequest();
  if (!status_or_oprf_request.ok()) {
    // If the RLWE OPRF request hasn't been created successfully, then report
    // the error and stop the protocol.
    LOG(ERROR) << "PSM error: unexpected internal logic error during creating "
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
          &PsmRlweDmserverClientImpl::OnRlweOprfRequestCompletion,
          base::Unretained(this)));

  em::DeviceManagementRequest* request = config->request();
  em::PrivateSetMembershipRlweRequest* psm_rlwe_request =
      request->mutable_private_set_membership_request()->mutable_rlwe_request();

  *psm_rlwe_request->mutable_oprf_request() = status_or_oprf_request.value();
  psm_request_job_ = device_management_service_->CreateJob(std::move(config));
}

void PsmRlweDmserverClientImpl::OnRlweOprfRequestCompletion(
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramSparse(kUMAPsmDmServerRequestStatus + uma_suffix_, status);

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

void PsmRlweDmserverClientImpl::SendPsmRlweQueryRequest(
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
    LOG(ERROR) << "PSM error: unexpected internal logic error during creating "
                  "RLWE query request";
    StoreErrorAndStop(PsmResult::kCreateQueryRequestLibraryError);
    return;
  }

  LOG(WARNING) << "PSM: prepare and send out the RLWE query request";

  // Prepare the RLWE query request job.
  std::unique_ptr<DMServerJobConfiguration> config =
      CreatePsmRequestJobConfiguration(base::BindOnce(
          &PsmRlweDmserverClientImpl::OnRlweQueryRequestCompletion,
          base::Unretained(this), oprf_response));

  em::DeviceManagementRequest* request = config->request();
  em::PrivateSetMembershipRlweRequest* psm_rlwe_request =
      request->mutable_private_set_membership_request()->mutable_rlwe_request();

  *psm_rlwe_request->mutable_query_request() = status_or_query_request.value();
  psm_request_job_ = device_management_service_->CreateJob(std::move(config));
}

void PsmRlweDmserverClientImpl::OnRlweQueryRequestCompletion(
    const psm_rlwe::PrivateMembershipRlweOprfResponse& oprf_response,
    DeviceManagementService::Job* job,
    DeviceManagementStatus status,
    int net_error,
    const em::DeviceManagementResponse& response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramSparse(kUMAPsmDmServerRequestStatus + uma_suffix_, status);

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

      // Store the last PSM execution result.
      last_psm_execution_result_ =
          ResultHolder(PsmResult::kSuccessfulDetermination, membership_result,
                       /*membership_determination_time=*/base::Time::Now());

      std::move(on_completion_callback_)
          .Run(last_psm_execution_result_.value());
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
      LOG(ERROR) << "PSM error: RLWE query request failed due to server error";
      StoreErrorAndStop(PsmResult::kServerError);
      return;
    }
  }
}

std::unique_ptr<DMServerJobConfiguration>
PsmRlweDmserverClientImpl::CreatePsmRequestJobConfiguration(
    DMServerJobConfiguration::Callback callback) {
  return std::make_unique<DMServerJobConfiguration>(
      device_management_service_,
      DeviceManagementService::JobConfiguration::
          TYPE_PSM_HAS_DEVICE_STATE_REQUEST,
      random_device_id_,
      /*critical=*/true, DMAuth::NoAuth(),
      /*oauth_token=*/absl::nullopt, url_loader_factory_, std::move(callback));
}

void PsmRlweDmserverClientImpl::RecordPsmSuccessTimeHistogram() {
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

}  // namespace policy
