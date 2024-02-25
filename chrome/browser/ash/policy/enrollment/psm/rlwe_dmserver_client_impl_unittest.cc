// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_dmserver_client.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_test_support.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/device_management_service.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace psm_rlwe = private_membership::rlwe;
namespace em = enterprise_management;

// An enum for PSM execution result values.
using PsmExecutionResult = em::DeviceRegisterRequest::PsmExecutionResult;

namespace policy::psm {

// A struct reporesents the PSM execution result params.
using PsmResultHolder = RlweDmserverClient::ResultHolder;

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::StrictMock;

class RlweDmserverClientImplTest
    : public ::testing::TestWithParam</*is_member*/ bool> {
 public:
  RlweDmserverClientImplTest() {
    // Create PSM RLWE DMServer client.
    CreateClient();
  }

  ~RlweDmserverClientImplTest() override = default;

  void CreateClient() {
    service_ =
        std::make_unique<FakeDeviceManagementService>(&job_creation_handler_);
    service_->ScheduleInitialization(0);

    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_);

    const bool is_member = GetParam();
    psm_test_case_ = testing::LoadTestCase(is_member);
    psm_client_ = std::make_unique<RlweDmserverClientImpl>(
        service_.get(), shared_url_loader_factory_,
        psm_test_case_.plaintext_id(), testing::CreateClientFactory(is_member));
  }

  // Start the `RlweDmserverClient` to retrieve the device state.
  void CheckMembershipWithRlweClient() {
    psm_client_->CheckMembership(future_result_holder_.GetCallback());
  }

  // Expects the `future_result_holder_` will be retrieved and compared
  // against `expected_result_holder`.
  void VerifyResultHolder(PsmResultHolder expected_result_holder) {
    PsmResultHolder psm_params = future_result_holder_.Take();
    EXPECT_EQ(expected_result_holder.psm_result, psm_params.psm_result);
    EXPECT_EQ(expected_result_holder.membership_result,
              psm_params.membership_result);
    EXPECT_EQ(expected_result_holder.membership_determination_time,
              psm_params.membership_determination_time);
  }

  void ServerWillReplyWithPsmOprfResponse() {
    em::DeviceManagementResponse response = GetPsmOprfResponse();

    ServerWillReplyForPsm(net::OK, DeviceManagementService::kSuccess, response);
  }

  void ServerWillReplyWithPsmQueryResponse() {
    em::DeviceManagementResponse response = GetPsmQueryResponse();

    ServerWillReplyForPsm(net::OK, DeviceManagementService::kSuccess, response);
  }

  void ServerWillReplyWithEmptyPsmResponse() {
    em::DeviceManagementResponse dummy_response;
    ServerWillReplyForPsm(net::OK, DeviceManagementService::kSuccess,
                          dummy_response);
  }

  void ServerWillFailForPsm(int net_error, int response_code) {
    em::DeviceManagementResponse dummy_response;
    ServerWillReplyForPsm(net_error, response_code, dummy_response);
  }

  // Mocks the server reply and captures the job type in |psm_last_job_type_|,
  // and the request in |psm_last_request_|.
  void ServerWillReplyForPsm(int net_error,
                             int response_code,
                             const em::DeviceManagementResponse& response) {
    EXPECT_CALL(job_creation_handler_, OnJobCreation)
        .WillOnce(DoAll(
            service_->CaptureJobType(&psm_last_job_type_),
            service_->CaptureRequest(&psm_last_request_),
            service_->SendJobResponseAsync(net_error, response_code, response)))
        .RetiresOnSaturation();
  }

  const em::PrivateSetMembershipRequest& psm_request() const {
    return psm_last_request_.private_set_membership_request();
  }

  // Returns the expected membership result for the current private set
  // membership test case.
  bool GetExpectedMembershipResult() const {
    return psm_test_case_.is_positive_membership_expected();
  }

  // Expects a sample for kUMAPsmResult to be recorded once with value
  // |protocol_result|.
  // If |success_time_recorded| is true it expects one sample
  // for kUMAPsmSuccessTime. Otherwise, expects no sample to be recorded for
  // kUMAPsmSuccessTime.
  void ExpectPsmHistograms(psm::RlweResult protocol_result,
                           bool success_time_recorded) const {
    histogram_tester_.ExpectBucketCount(
        kUMAPsmResult + kUMASuffixInitialEnrollmentStr, protocol_result,
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(kUMAPsmSuccessTime,
                                       success_time_recorded ? 1 : 0);
  }

  // Expects a sample |dm_status| for kUMAPsmDmServerRequestStatus with count
  // |dm_status_count|.
  void ExpectPsmRequestStatusHistogram(DeviceManagementStatus dm_status,
                                       int dm_status_count) const {
    histogram_tester_.ExpectBucketCount(
        kUMAPsmDmServerRequestStatus + kUMASuffixInitialEnrollmentStr,
        dm_status, dm_status_count);
  }

  // Expects one sample for |kUMAPsmNetworkErrorCode| which has value of
  // |network_error|.
  void ExpectPsmNetworkErrorHistogram(int network_error) const {
    histogram_tester_.ExpectBucketCount(
        kUMAPsmNetworkErrorCode + kUMASuffixInitialEnrollmentStr, network_error,
        /*expected_count=*/1);
  }

  void VerifyPsmLastRequestJobType() const {
    EXPECT_EQ(DeviceManagementService::JobConfiguration::
                  TYPE_PSM_HAS_DEVICE_STATE_REQUEST,
              psm_last_job_type_);
  }

  void VerifyRlweOprfRequest() const {
    EXPECT_EQ(psm_test_case_.expected_oprf_request().SerializeAsString(),
              psm_request().rlwe_request().oprf_request().SerializeAsString());
  }

  void VerifyRlweQueryRequest() const {
    EXPECT_EQ(psm_test_case_.expected_query_request().SerializeAsString(),
              psm_request().rlwe_request().query_request().SerializeAsString());
  }

  // Disallow copy constructor and assignment operator.
  RlweDmserverClientImplTest(const RlweDmserverClientImplTest&) = delete;
  RlweDmserverClientImplTest& operator=(const RlweDmserverClientImplTest&) =
      delete;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  em::DeviceManagementResponse GetPsmOprfResponse() const {
    em::DeviceManagementResponse response;
    em::PrivateSetMembershipResponse* psm_response =
        response.mutable_private_set_membership_response();

    *psm_response->mutable_rlwe_response()->mutable_oprf_response() =
        psm_test_case_.oprf_response();
    return response;
  }

  em::DeviceManagementResponse GetPsmQueryResponse() const {
    em::DeviceManagementResponse response;
    em::PrivateSetMembershipResponse* psm_response =
        response.mutable_private_set_membership_response();

    *psm_response->mutable_rlwe_response()->mutable_query_response() =
        psm_test_case_.query_response();
    return response;
  }

  std::unique_ptr<RlweDmserverClient> psm_client_;
  base::test::TestFuture<PsmResultHolder> future_result_holder_;
  psm_rlwe::PrivateMembershipRlweClientRegressionTestData::TestCase
      psm_test_case_;

  // Sets which PSM RLWE client will be created, depending on the factory. It
  // is only used for PSM during creating the client for initial enrollment.
  std::unique_ptr<RlweDmserverClientImpl::RlweClient> psm_rlwe_test_client_;

  base::HistogramTester histogram_tester_;
  std::unique_ptr<FakeDeviceManagementService> service_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  StrictMock<MockJobCreationHandler> job_creation_handler_;
  base::RunLoop run_loop_;

  DeviceManagementService::JobConfiguration::JobType psm_last_job_type_ =
      DeviceManagementService::JobConfiguration::TYPE_INVALID;
  em::DeviceManagementRequest psm_last_request_;
  const std::string kUMASuffixInitialEnrollmentStr =
      kUMASuffixInitialEnrollment;
};

TEST_P(RlweDmserverClientImplTest, MembershipRetrievedSuccessfully) {
  InSequence sequence;

  const bool kExpectedMembershipResult = GetExpectedMembershipResult();
  const base::TimeDelta kOneSecondTimeDelta = base::Seconds(1);
  const base::Time kExpectedPsmDeterminationTimestamp =
      base::Time::NowFromSystemTime() + kOneSecondTimeDelta;

  // Advance the time forward one second.
  task_environment_.FastForwardBy(kOneSecondTimeDelta);

  ServerWillReplyWithPsmOprfResponse();
  ServerWillReplyWithPsmQueryResponse();

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(kExpectedMembershipResult,
                                     kExpectedPsmDeterminationTimestamp));

  ExpectPsmHistograms(psm::RlweResult::kSuccessfulDetermination,
                      /*success_time_recorded=*/true);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/2);
  VerifyRlweQueryRequest();
  VerifyPsmLastRequestJobType();
}

TEST_P(RlweDmserverClientImplTest, EmptyRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPsmOprfResponse();
  ServerWillReplyWithEmptyPsmResponse();

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(
      PsmResultHolder(psm::RlweResult::kEmptyQueryResponseError));

  ExpectPsmHistograms(psm::RlweResult::kEmptyQueryResponseError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/2);
  VerifyRlweQueryRequest();
  VerifyPsmLastRequestJobType();
}

TEST_P(RlweDmserverClientImplTest, EmptyRlweOprfResponse) {
  InSequence sequence;
  ServerWillReplyWithEmptyPsmResponse();

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(psm::RlweResult::kEmptyOprfResponseError));

  ExpectPsmHistograms(psm::RlweResult::kEmptyOprfResponseError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/1);
  VerifyRlweOprfRequest();
  VerifyPsmLastRequestJobType();
}

TEST_P(RlweDmserverClientImplTest, ConnectionErrorForRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPsmOprfResponse();
  ServerWillFailForPsm(net::ERR_FAILED, DeviceManagementService::kSuccess);

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(AutoEnrollmentDMServerError{
      .dm_error = DM_STATUS_SUCCESS, .network_error = net::ERR_FAILED}));

  ExpectPsmHistograms(psm::RlweResult::kConnectionError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/1);
  ExpectPsmRequestStatusHistogram(DM_STATUS_REQUEST_FAILED,
                                  /*dm_status_count=*/1);
  ExpectPsmNetworkErrorHistogram(-net::ERR_FAILED);
  VerifyRlweQueryRequest();
  VerifyPsmLastRequestJobType();
}

TEST_P(RlweDmserverClientImplTest, ConnectionErrorForRlweOprfResponse) {
  InSequence sequence;
  ServerWillFailForPsm(net::ERR_FAILED, DeviceManagementService::kSuccess);

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(AutoEnrollmentDMServerError{
      .dm_error = DM_STATUS_SUCCESS, .network_error = net::ERR_FAILED}));

  ExpectPsmHistograms(psm::RlweResult::kConnectionError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_REQUEST_FAILED,
                                  /*dm_status_count=*/1);
  ExpectPsmNetworkErrorHistogram(-net::ERR_FAILED);
  VerifyRlweOprfRequest();
  VerifyPsmLastRequestJobType();
}

TEST_P(RlweDmserverClientImplTest, NetworkFailureForRlweOprfResponse) {
  InSequence sequence;
  ServerWillFailForPsm(net::OK, net::ERR_CONNECTION_CLOSED);

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(
      AutoEnrollmentDMServerError{.dm_error = DM_STATUS_HTTP_STATUS_ERROR}));

  ExpectPsmHistograms(psm::RlweResult::kServerError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_HTTP_STATUS_ERROR,
                                  /*dm_status_count=*/1);
  VerifyPsmLastRequestJobType();
}

TEST_P(RlweDmserverClientImplTest, NetworkFailureForRlweQueryResponse) {
  InSequence sequence;
  ServerWillReplyWithPsmOprfResponse();
  ServerWillFailForPsm(net::OK, net::ERR_CONNECTION_CLOSED);

  ASSERT_NO_FATAL_FAILURE(CheckMembershipWithRlweClient());

  VerifyResultHolder(PsmResultHolder(
      AutoEnrollmentDMServerError{.dm_error = DM_STATUS_HTTP_STATUS_ERROR}));

  ExpectPsmHistograms(psm::RlweResult::kServerError,
                      /*success_time_recorded=*/false);
  ExpectPsmRequestStatusHistogram(DM_STATUS_SUCCESS,
                                  /*dm_status_count=*/1);
  ExpectPsmRequestStatusHistogram(DM_STATUS_HTTP_STATUS_ERROR,
                                  /*dm_status_count=*/1);
  VerifyRlweQueryRequest();
  VerifyPsmLastRequestJobType();
}

INSTANTIATE_TEST_SUITE_P(RlweDmserverClientImplTest,
                         RlweDmserverClientImplTest,
                         /*is_member=*/::testing::Bool());

}  // namespace policy::psm
