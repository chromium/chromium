// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace psm_rlwe = private_membership::rlwe;
namespace em = enterprise_management;

namespace policy::psm {

// static
std::unique_ptr<RlweDmserverClientImpl::RlweClient>
RlweDmserverClientImpl::Create(private_membership::rlwe::RlweUseCase use_case,
                               const psm_rlwe::RlwePlaintextId& plaintext_id) {
  auto status_or_client = RlweClient::Create(use_case, {plaintext_id});
  DCHECK(status_or_client.ok()) << status_or_client.status().message();

  return std::move(status_or_client).value();
}

RlweDmserverClientImpl::RlweDmserverClientImpl(
    DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const PlaintextId& plaintext_id,
    RlweClientFactory rlwe_client_factory)
    : plaintext_id_(plaintext_id),
      psm_rlwe_client_(
          rlwe_client_factory.Run(private_membership::rlwe::CROS_DEVICE_STATE,
                                  plaintext_id)),
      random_device_id_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      url_loader_factory_(url_loader_factory),
      device_management_service_(device_management_service) {
  CHECK(psm_rlwe_client_);
  CHECK(url_loader_factory_);
  CHECK(device_management_service_);
}

RlweDmserverClientImpl::~RlweDmserverClientImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RlweDmserverClientImpl::CheckMembership(CompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);

  // There should not be any pending PSM requests.
  CHECK(!psm_request_job_);

  time_start_ = base::TimeTicks::Now();

  on_completion_callback_ = std::move(callback);

  SendRlweOprfRequest();
}

bool RlweDmserverClientImpl::IsCheckMembershipInProgress() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return psm_request_job_ != nullptr;
}

void RlweDmserverClientImpl::RecordErrorAndStop(ResultHolder result) {
  // Note that kUMAPsmResult histogram is only using initial enrollment as a
  // suffix until PSM support FRE.
  base::UmaHistogramEnumeration(kUMAPsmResult + uma_suffix_, result.psm_result);

  // Stop the current |psm_request_job_|.
  psm_request_job_.reset();

  std::move(on_completion_callback_).Run(std::move(result));
}

void RlweDmserverClientImpl::SendRlweOprfRequest() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Create RLWE OPRF request.
  const auto status_or_oprf_request = psm_rlwe_client_->CreateOprfRequest();
  if (!status_or_oprf_request.ok()) {
    // If the RLWE OPRF request hasn't been created successfully, then report
    // the error and stop the protocol.
    LOG(ERROR) << "PSM error: unexpected internal logic error during creating "
                  "RLWE OPRF request";
    RecordErrorAndStop(RlweResult::kCreateOprfRequestLibraryError);
    return;
  }

  LOG(WARNING) << "PSM: prepare and send out the RLWE OPRF request";

  // Prepare the RLWE OPRF request job.
  // The passed callback will not be called if |psm_request_job_| is
  // destroyed, so it's safe to use base::Unretained.
  std::unique_ptr<DMServerJobConfiguration> config =
      CreatePsmRequestJobConfiguration(
          base::BindOnce(&RlweDmserverClientImpl::OnRlweOprfRequestCompletion,
                         base::Unretained(this)));

  em::DeviceManagementRequest* request = config->request();
  em::PrivateSetMembershipRlweRequest* psm_rlwe_request =
      request->mutable_private_set_membership_request()->mutable_rlwe_request();

  *psm_rlwe_request->mutable_oprf_request() = status_or_oprf_request.value();
  psm_request_job_ = device_management_service_->CreateJob(std::move(config));
}

void RlweDmserverClientImpl::OnRlweOprfRequestCompletion(
    DMServerJobResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramSparse(kUMAPsmDmServerRequestStatus + uma_suffix_,
                           result.dm_status);

  switch (result.dm_status) {
    case DM_STATUS_SUCCESS: {
      // Check if the RLWE OPRF response is empty.
      if (!result.response.private_set_membership_response()
               .has_rlwe_response() ||
          !result.response.private_set_membership_response()
               .rlwe_response()
               .has_oprf_response()) {
        LOG(ERROR) << "PSM error: empty OPRF RLWE response";
        RecordErrorAndStop(RlweResult::kEmptyOprfResponseError);
        return;
      }

      LOG(WARNING) << "PSM RLWE OPRF request completed successfully";
      SendRlweQueryRequest(result.response.private_set_membership_response());
      return;
    }
    case DM_STATUS_REQUEST_FAILED: {
      LOG(ERROR)
          << "PSM error: RLWE OPRF request failed due to connection error";
      base::UmaHistogramSparse(kUMAPsmNetworkErrorCode + uma_suffix_,
                               -result.net_error);
      RecordErrorAndStop(
          AutoEnrollmentDMServerError::FromDMServerJobResult(result));
      return;
    }
    default: {
      LOG(ERROR) << "PSM error: RLWE OPRF request failed due to server error";
      RecordErrorAndStop(
          AutoEnrollmentDMServerError::FromDMServerJobResult(result));
      return;
    }
  }
}

void RlweDmserverClientImpl::SendRlweQueryRequest(
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
    RecordErrorAndStop(RlweResult::kCreateQueryRequestLibraryError);
    return;
  }

  LOG(WARNING) << "PSM: prepare and send out the RLWE query request";

  // Prepare the RLWE query request job.
  std::unique_ptr<DMServerJobConfiguration> config =
      CreatePsmRequestJobConfiguration(
          base::BindOnce(&RlweDmserverClientImpl::OnRlweQueryRequestCompletion,
                         base::Unretained(this), oprf_response));

  em::DeviceManagementRequest* request = config->request();
  em::PrivateSetMembershipRlweRequest* psm_rlwe_request =
      request->mutable_private_set_membership_request()->mutable_rlwe_request();

  *psm_rlwe_request->mutable_query_request() = status_or_query_request.value();
  psm_request_job_ = device_management_service_->CreateJob(std::move(config));
}

void RlweDmserverClientImpl::OnRlweQueryRequestCompletion(
    const OprfResponse& oprf_response,
    DMServerJobResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramSparse(kUMAPsmDmServerRequestStatus + uma_suffix_,
                           result.dm_status);

  switch (result.dm_status) {
    case DM_STATUS_SUCCESS: {
      // Check if the RLWE query response is empty.
      if (!result.response.private_set_membership_response()
               .has_rlwe_response() ||
          !result.response.private_set_membership_response()
               .rlwe_response()
               .has_query_response()) {
        LOG(ERROR) << "PSM error: empty query RLWE response";
        RecordErrorAndStop(RlweResult::kEmptyQueryResponseError);
        return;
      }

      const auto responses = psm_rlwe_client_->ProcessQueryResponse(
          result.response.private_set_membership_response()
              .rlwe_response()
              .query_response());
      if (!responses.ok()) {
        // If the RLWE query response hasn't processed successfully, then
        // report the error and stop the protocol.
        LOG(ERROR) << "PSM error: unexpected internal logic error during "
                      "processing the "
                      "RLWE query response";
        RecordErrorAndStop(RlweResult::kProcessingQueryResponseLibraryError);
        return;
      }

      DCHECK_EQ(responses->membership_responses_size(), 1);

      const bool is_member =
          responses->membership_responses(0).membership_response().is_member();

      base::UmaHistogramEnumeration(kUMAPsmResult + uma_suffix_,
                                    RlweResult::kSuccessfulDetermination);
      RecordPsmSuccessTimeHistogram();

      LOG(WARNING) << "PSM determination successful. Identifier "
                   << plaintext_id_.sensitive_id() << (is_member ? "" : " not")
                   << " present on the server";

      // Reset the |psm_request_job_| to allow another call to
      // CheckMembership.
      psm_request_job_.reset();

      std::move(on_completion_callback_)
          .Run(ResultHolder(
              is_member,
              /*membership_determination_time=*/base::Time::Now()));
      return;
    }
    case DM_STATUS_REQUEST_FAILED: {
      LOG(ERROR)
          << "PSM error: RLWE query request failed due to connection error";
      base::UmaHistogramSparse(kUMAPsmNetworkErrorCode + uma_suffix_,
                               -result.net_error);
      RecordErrorAndStop(
          AutoEnrollmentDMServerError::FromDMServerJobResult(result));
      return;
    }
    default: {
      LOG(ERROR) << "PSM error: RLWE query request failed due to server error";
      RecordErrorAndStop(
          AutoEnrollmentDMServerError::FromDMServerJobResult(result));
      return;
    }
  }
}

std::unique_ptr<DMServerJobConfiguration>
RlweDmserverClientImpl::CreatePsmRequestJobConfiguration(
    DMServerJobConfiguration::Callback callback) {
  return std::make_unique<DMServerJobConfiguration>(
      device_management_service_,
      DeviceManagementService::JobConfiguration::
          TYPE_PSM_HAS_DEVICE_STATE_REQUEST,
      random_device_id_,
      /*critical=*/true, DMAuth::NoAuth(),
      /*oauth_token=*/std::nullopt, url_loader_factory_, std::move(callback));
}

void RlweDmserverClientImpl::RecordPsmSuccessTimeHistogram() {
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

}  // namespace policy::psm
