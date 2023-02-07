// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_client.h"

#include <string>

#include "base/strings/string_piece.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using testing::_;
using testing::Invoke;
using testing::SizeIs;

namespace ash::cert_provisioning {

namespace {

MATCHER_P(EqualsProto,
          message,
          "Match a proto Message equal to the matcher's argument.") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// A fake CloudPolicyClient that can record cert provisioning actions and
// provides the test a way to supply a response by saving the callbacks passed
// by the code-under-test.
class FakeCloudPolicyClient : public policy::MockCloudPolicyClient {
 public:
  struct CertProvCall {
    em::ClientCertificateProvisioningRequest request;
    ClientCertProvisioningRequestCallback callback;
  };

  FakeCloudPolicyClient() {
    EXPECT_CALL(*this, ClientCertProvisioningRequest)
        .WillRepeatedly(Invoke(
            this, &FakeCloudPolicyClient::OnClientCertProvisioningRequest));
  }

  std::vector<CertProvCall>& cert_prov_calls() { return cert_prov_calls_; }

 private:
  void OnClientCertProvisioningRequest(
      em::ClientCertificateProvisioningRequest request,
      ClientCertProvisioningRequestCallback callback) {
    cert_prov_calls_.push_back({std::move(request), std::move(callback)});
  }

  std::vector<CertProvCall> cert_prov_calls_;
};

// A TestFuture that supports waiting for a
// CertProvisioningClient::NextActionCallback.
class NextActionFuture
    : public base::test::TestFuture<
          policy::DeviceManagementStatus,
          absl::optional<em::ClientCertificateProvisioningResponse::Error>,
          CertProvisioningClient::CertProvNextActionResponse> {
 public:
  CertProvisioningClient::NextActionCallback GetNextActionCallback() {
    return GetCallback<
        policy::DeviceManagementStatus,
        absl::optional<em::ClientCertificateProvisioningResponse::Error>,
        const CertProvisioningClient::CertProvNextActionResponse&>();
  }

  policy::DeviceManagementStatus GetStatus() { return Get<0>(); }

  absl::optional<em::ClientCertificateProvisioningResponse::Error> GetError() {
    return Get<1>();
  }

  const CertProvisioningClient::CertProvNextActionResponse&
  GetNextActionResponse() {
    return Get<2>();
  }
};

// A TestFuture that supports waiting for a
// CertProvisioningClient::StartCsrCallback.
class StartCsrFuture
    : public base::test::TestFuture<
          policy::DeviceManagementStatus,
          absl::optional<em::ClientCertificateProvisioningResponse::Error>,
          absl::optional<int64_t>,
          std::string,
          std::string,
          em::HashingAlgorithm,
          std::vector<uint8_t>> {
 public:
  CertProvisioningClient::StartCsrCallback GetStartCsrCallback() {
    return GetCallback<
        policy::DeviceManagementStatus,
        absl::optional<em::ClientCertificateProvisioningResponse::Error>,
        absl::optional<int64_t>, const std::string&, const std::string&,
        em::HashingAlgorithm, std::vector<uint8_t>>();
  }

  policy::DeviceManagementStatus GetStatus() { return Get<0>(); }

  absl::optional<em::ClientCertificateProvisioningResponse::Error> GetError() {
    return Get<1>();
  }

  absl::optional<int64_t> GetTryLater() { return Get<2>(); }

  const std::string& GetInvalidationTopic() { return Get<3>(); }

  const std::string& GetVaChallenge() { return Get<4>(); }

  em::HashingAlgorithm GetHashingAlgorithm() { return Get<5>(); }

  const std::vector<uint8_t>& GetDataToSign() { return Get<6>(); }
};

// A TestFuture that supports waiting for a
// CertProvisioningClient::FinishCsrCallback.
class FinishCsrFuture
    : public base::test::TestFuture<
          policy::DeviceManagementStatus,
          absl::optional<em::ClientCertificateProvisioningResponse::Error>,
          absl::optional<int64_t>> {
 public:
  CertProvisioningClient::FinishCsrCallback GetFinishCsrCallback() {
    return GetCallback();
  }

  policy::DeviceManagementStatus GetStatus() { return Get<0>(); }

  absl::optional<em::ClientCertificateProvisioningResponse::Error> GetError() {
    return Get<1>();
  }

  absl::optional<int64_t> GetTryLater() { return Get<2>(); }
};

// A TestFuture that supports waiting for a
// CertProvisioningClient::DownloadCertCallback.
class DownloadCertFuture
    : public base::test::TestFuture<
          policy::DeviceManagementStatus,
          absl::optional<em::ClientCertificateProvisioningResponse::Error>,
          absl::optional<int64_t>,
          std::string> {
 public:
  CertProvisioningClient::DownloadCertCallback GetDownloadCertCallback() {
    return GetCallback<
        policy::DeviceManagementStatus,
        absl::optional<em::ClientCertificateProvisioningResponse::Error>,
        absl::optional<int64_t>, const std::string&>();
  }

  policy::DeviceManagementStatus GetStatus() { return Get<0>(); }

  absl::optional<em::ClientCertificateProvisioningResponse::Error> GetError() {
    return Get<1>();
  }

  absl::optional<int64_t> GetTryLater() { return Get<2>(); }

  const std::string& GetPemEncodedCertificate() { return Get<3>(); }
};

}  // namespace

// Tuple of CertScope enum value and corresponding device management protocol
// string.
using CertScopePair = std::tuple<CertScope, std::string>;

// Base class for testing CertProvisioningClient.
// The subclasses will implement different test parameters.
class CertProvisioningClientTestBase : public testing::Test {
 public:
  CertProvisioningClientTestBase() = default;
  ~CertProvisioningClientTestBase() override = default;

  virtual CertScope cert_scope() const = 0;
  virtual const std::string& cert_scope_dm_api_string() const = 0;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;

  FakeCloudPolicyClient cloud_policy_client_;

  const std::string kCertProfileId = "fake_cert_profile_id_1";
  const std::string kCertProfileVersion = "fake_cert_profile_version_1";
  const std::vector<uint8_t> kPublicKey = {0x66, 0x61, 0x6B, 0x65,
                                           0x5F, 0x6B, 0x65, 0x79};
  const std::string kPublicKeyAsString =
      std::string(kPublicKey.begin(), kPublicKey.end());

  const std::string kInvalidationTopic = "fake_invalidation_topic_1";
  const std::string kVaChallange = "fake_va_challenge_1";
  const std::string kDataToSignStr = {10, 11, 12, 13, 14};
  const std::vector<uint8_t> kDataToSignBin = {10, 11, 12, 13, 14};
  const em::HashingAlgorithm kHashAlgorithm = em::HashingAlgorithm::SHA256;
  const em::SigningAlgorithm kSignAlgorithm =
      em::SigningAlgorithm::RSA_PKCS1_V1_5;
  const std::string kVaChallengeResponse = "fake_va_challenge_response_1";
  const std::string kSignature = "fake_signature_1";
  const std::string kPemEncodedCert = "fake_pem_encoded_cert_1";
};

// Test fixture for CertProvisioningClient, parametrized by CertScope.
class CertProvisioningClientTest
    : public CertProvisioningClientTestBase,
      public testing::WithParamInterface<CertScopePair> {
 public:
  CertScope cert_scope() const override { return std::get<0>(GetParam()); }

  const std::string& cert_scope_dm_api_string() const override {
    return std::get<1>(GetParam());
  }
};

// Checks that StartOrContinue fills the StartOrContinueRequest correctly.
TEST_P(CertProvisioningClientTest, StartOrContinueRequest) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  NextActionFuture next_action_future;
  cert_provisioning_client.StartOrContinue(
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      next_action_future.GetNextActionCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_scope(cert_scope_dm_api_string());
    expected_request.set_cert_profile_id(kCertProfileId);
    expected_request.set_policy_version(kCertProfileVersion);
    expected_request.set_public_key(kPublicKeyAsString);
    // Sets the request type, no actual data is required.
    expected_request.mutable_start_or_continue_request();

    EXPECT_THAT(cert_prov_call.request, EqualsProto(expected_request));
  }
  // Note: Processing of the response will be tested in another test.
}

// Checks that Authorize fills the AuthorizeRequest correctly.
TEST_P(CertProvisioningClientTest, AuthorizeRequest) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  NextActionFuture next_action_future;
  cert_provisioning_client.Authorize(
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      kVaChallengeResponse, next_action_future.GetNextActionCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_scope(cert_scope_dm_api_string());
    expected_request.set_cert_profile_id(kCertProfileId);
    expected_request.set_policy_version(kCertProfileVersion);
    expected_request.set_public_key(kPublicKeyAsString);

    auto* authorize_request = expected_request.mutable_authorize_request();
    authorize_request->set_va_challenge_response(kVaChallengeResponse);

    EXPECT_THAT(cert_prov_call.request, EqualsProto(expected_request));
  }
  // Note: Processing of the response will be tested in another test.
}

// Checks that UploadProofOfPossession fills the UploadProofOfPossessionRequest
// correctly.
TEST_P(CertProvisioningClientTest, UploadProofOfPossessionRequest) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  NextActionFuture next_action_future;
  cert_provisioning_client.UploadProofOfPossession(
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      kSignature, next_action_future.GetNextActionCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_scope(cert_scope_dm_api_string());
    expected_request.set_cert_profile_id(kCertProfileId);
    expected_request.set_policy_version(kCertProfileVersion);
    expected_request.set_public_key(kPublicKeyAsString);

    auto* upload_proof_of_possession_request =
        expected_request.mutable_upload_proof_of_possession_request();
    upload_proof_of_possession_request->set_signature(kSignature);

    EXPECT_THAT(cert_prov_call.request, EqualsProto(expected_request));
  }
  // Note: Processing of the response will be tested in another test.
}

// 1. Checks that `StartCsr` generates a correct request.
// 2. Checks that CertProvisioningClient correctly extracts data from a response
// that contains data.
TEST_P(CertProvisioningClientTest, StartCsrSuccess) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  StartCsrFuture start_csr_future;
  cert_provisioning_client.StartCsr(
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      start_csr_future.GetStartCsrCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_scope(cert_scope_dm_api_string());
    expected_request.set_cert_profile_id(kCertProfileId);
    expected_request.set_policy_version(kCertProfileVersion);
    expected_request.set_public_key(kPublicKeyAsString);
    // Sets the request type, no actual data is required.
    expected_request.mutable_start_csr_request();

    EXPECT_THAT(cert_prov_call.request, EqualsProto(expected_request));
  }

  // Make CloudPolicyClient answer the request.
  em::ClientCertificateProvisioningResponse response;
  {
    em::StartCsrResponse* start_csr_response =
        response.mutable_start_csr_response();
    start_csr_response->set_invalidation_topic(kInvalidationTopic);
    start_csr_response->set_va_challenge(kVaChallange);
    start_csr_response->set_hashing_algorithm(kHashAlgorithm);
    start_csr_response->set_signing_algorithm(kSignAlgorithm);
    start_csr_response->set_data_to_sign(kDataToSignStr);
  }
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that CertProvisioningClient has translated the answer correctly.
  ASSERT_TRUE(start_csr_future.Wait());
  EXPECT_EQ(start_csr_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(start_csr_future.GetError(), absl::nullopt);
  EXPECT_EQ(start_csr_future.GetTryLater(), absl::nullopt);
  EXPECT_EQ(start_csr_future.GetInvalidationTopic(), kInvalidationTopic);
  EXPECT_EQ(start_csr_future.GetVaChallenge(), kVaChallange);
  EXPECT_EQ(start_csr_future.GetHashingAlgorithm(), kHashAlgorithm);
  EXPECT_EQ(start_csr_future.GetDataToSign(), kDataToSignBin);
}

// Checks that CertProvisioningClient correctly reacts on the `try_later` field
// in a response to StartCsr.
TEST_P(CertProvisioningClientTest, StartCsrTryLater) {
  const int64_t try_later = 60000;

  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  StartCsrFuture start_csr_future;
  cert_provisioning_client.StartCsr(
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      start_csr_future.GetStartCsrCallback());

  // Expect one request to CloudPolicyClient.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();

  // Make CloudPolicyClient answer the request.
  em::ClientCertificateProvisioningResponse response;
  response.set_try_again_later(try_later);
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that CertProvisioningClient has translated the answer correctly.
  ASSERT_TRUE(start_csr_future.Wait());
  EXPECT_EQ(start_csr_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(start_csr_future.GetError(), absl::nullopt);
  EXPECT_EQ(start_csr_future.GetTryLater(), absl::make_optional(try_later));
}

// Checks that CertProvisioningClient correctly reacts on the `error` field
// in a response to StartCsr.
TEST_P(CertProvisioningClientTest, StartCsrError) {
  const CertProvisioningResponseErrorType error =
      CertProvisioningResponseError::CA_ERROR;

  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  StartCsrFuture start_csr_future;
  cert_provisioning_client.StartCsr(
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      start_csr_future.GetStartCsrCallback());

  // Expect one request to CloudPolicyClient.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();

  // Make CloudPolicyClient answer the request.
  em::ClientCertificateProvisioningResponse response;
  response.set_error(error);
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that CertProvisioningClient has translated the answer correctly.
  ASSERT_TRUE(start_csr_future.Wait());
  EXPECT_EQ(start_csr_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(start_csr_future.GetError(), absl::make_optional(error));
  EXPECT_EQ(start_csr_future.GetTryLater(), absl::nullopt);
}

// 1. Checks that `FinishCsr` generates a correct request.
// 2. Checks that CertProvisioningClient correctly extracts data from a response
// that contains data.
TEST_P(CertProvisioningClientTest, FinishCsrSuccess) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  FinishCsrFuture finish_csr_future;
  cert_provisioning_client.FinishCsr(
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      kVaChallengeResponse, kSignature,
      finish_csr_future.GetFinishCsrCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_scope(cert_scope_dm_api_string());
    expected_request.set_cert_profile_id(kCertProfileId);
    expected_request.set_policy_version(kCertProfileVersion);
    expected_request.set_public_key(kPublicKeyAsString);

    em::FinishCsrRequest* finish_csr_request =
        expected_request.mutable_finish_csr_request();
    finish_csr_request->set_va_challenge_response(kVaChallengeResponse);
    finish_csr_request->set_signature(kSignature);

    EXPECT_THAT(cert_prov_call.request, EqualsProto(expected_request));
  }

  // Make CloudPolicyClient answer the request.
  em::ClientCertificateProvisioningResponse response;
  {
    // Sets the response id, no actual data is required.
    response.mutable_finish_csr_response();
  }
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that CertProvisioningClient has translated the answer correctly.
  ASSERT_TRUE(finish_csr_future.Wait());
  EXPECT_EQ(finish_csr_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(finish_csr_future.GetError(), absl::nullopt);
  EXPECT_EQ(finish_csr_future.GetTryLater(), absl::nullopt);
}

// Checks that CertProvisioningClient correctly reacts on the `error` field
// in a response to FinishCsr.
TEST_P(CertProvisioningClientTest, FinishCsrError) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  FinishCsrFuture finish_csr_future;
  cert_provisioning_client.FinishCsr(
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      kVaChallengeResponse, kSignature,
      finish_csr_future.GetFinishCsrCallback());

  // Expect one request to CloudPolicyClient.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();

  // Make CloudPolicyClient answer the request.
  const CertProvisioningResponseErrorType error =
      CertProvisioningResponseError::CA_ERROR;
  em::ClientCertificateProvisioningResponse response;
  response.set_error(error);
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that CertProvisioningClient has translated the answer correctly.
  ASSERT_TRUE(finish_csr_future.Wait());
  EXPECT_EQ(finish_csr_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(finish_csr_future.GetError(), absl::make_optional(error));
  EXPECT_EQ(finish_csr_future.GetTryLater(), absl::nullopt);
}

// 1. Checks that `DownloadCert` generates a correct request.
// 2. Checks that CertProvisioningClient correctly extracts data from a response
// that contains data.
TEST_P(CertProvisioningClientTest, DownloadCertSuccess) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  DownloadCertFuture download_cert_future;
  cert_provisioning_client.DownloadCert(
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      download_cert_future.GetDownloadCertCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_scope(cert_scope_dm_api_string());
    expected_request.set_cert_profile_id(kCertProfileId);
    expected_request.set_policy_version(kCertProfileVersion);
    expected_request.set_public_key(kPublicKeyAsString);

    // Sets the request type, no actual data is required.
    expected_request.mutable_download_cert_request();

    EXPECT_THAT(cert_prov_call.request, EqualsProto(expected_request));
  }

  // Make CloudPolicyClient answer the request.
  em::ClientCertificateProvisioningResponse response;
  {
    em::DownloadCertResponse* download_cert_response =
        response.mutable_download_cert_response();
    download_cert_response->set_pem_encoded_certificate(kPemEncodedCert);
  }
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that CertProvisioningClient has translated the answer correctly.
  ASSERT_TRUE(download_cert_future.Wait());
  EXPECT_EQ(download_cert_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(download_cert_future.GetError(), absl::nullopt);
  EXPECT_EQ(download_cert_future.GetTryLater(), absl::nullopt);
  EXPECT_EQ(download_cert_future.GetPemEncodedCertificate(), kPemEncodedCert);
}

// Checks that CertProvisioningClient correctly reacts on the `error` field
// in a response to DownloadCert.
TEST_P(CertProvisioningClientTest, DownloadCertError) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  DownloadCertFuture download_cert_future;
  cert_provisioning_client.DownloadCert(
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      download_cert_future.GetDownloadCertCallback());

  // Expect one request to CloudPolicyClient.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();

  // Make CloudPolicyClient answer the request.
  const CertProvisioningResponseErrorType error =
      CertProvisioningResponseError::CA_ERROR;
  em::ClientCertificateProvisioningResponse response;
  response.set_error(error);
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that CertProvisioningClient has translated the answer correctly.
  ASSERT_TRUE(download_cert_future.Wait());
  EXPECT_EQ(download_cert_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(download_cert_future.GetError(), absl::make_optional(error));
  EXPECT_EQ(download_cert_future.GetTryLater(), absl::nullopt);
  EXPECT_EQ(download_cert_future.GetPemEncodedCertificate(), std::string());
}

INSTANTIATE_TEST_SUITE_P(
    AllScopes,
    CertProvisioningClientTest,
    ::testing::Values(CertScopePair(CertScope::kUser, "google/chromeos/user"),
                      CertScopePair(CertScope::kDevice,
                                    "google/chromeos/device")));

// A Test case for CertProvisioningClientNextActionProcessingTest.
struct NextActionProcessingTestCase {
  // Invokes a CertProvisioningClient API call.
  base::RepeatingCallback<void(
      CertProvisioningClient*,
      CertProvisioningClient::ProvisioningProcess provisioining_process,
      CertProvisioningClient::NextActionCallback callback)>
      act_function;
};

// Test fixture for CertProvisioningClient, parametrized by CertScope and a
// NextActionProcessingTestCase which implements a call to one of the
// CertProvisioningClient API calls.
// This is useful for testing response processing across multiple API calls that
// provide the same response type (CertProvNextActionResponse).
class CertProvisioningClientNextActionProcessingTest
    : public CertProvisioningClientTestBase,
      public testing::WithParamInterface<
          std::tuple<CertScopePair, NextActionProcessingTestCase>> {
 public:
  CertScope cert_scope() const override {
    return std::get<0>(cert_scope_pair());
  }

  const std::string& cert_scope_dm_api_string() const override {
    return std::get<1>(cert_scope_pair());
  }

  void ExecuteCertProvisioningClientCall(
      CertProvisioningClient* client,
      CertProvisioningClient::ProvisioningProcess provisioning_process,
      CertProvisioningClient::NextActionCallback callback) const {
    std::get<1>(GetParam())
        .act_function.Run(client, std::move(provisioning_process),
                          std::move(callback));
  }

 private:
  const CertScopePair& cert_scope_pair() const {
    return std::get<0>(GetParam());
  }
};

// Checks that all "Dynamic flow" API calls forward a successful
// CertProvNextActionResponse correctly.
TEST_P(CertProvisioningClientNextActionProcessingTest, Success) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);
  // Execute the CertProvisioningClient API call. Don't verify the filled
  // request proto - this is done by other tests in this file.
  NextActionFuture next_action_future;
  ExecuteCertProvisioningClientCall(
      &cert_provisioning_client,
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      next_action_future.GetNextActionCallback());

  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  // Make CloudPolicyClient answer the request.
  em::ClientCertificateProvisioningResponse response;
  {
    auto* next_action_response = response.mutable_next_action_response();
    next_action_response->set_invalidation_topic(kInvalidationTopic);
    next_action_response->mutable_import_certificate_instruction();
  }
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Expect that the CertProvisioningClient forwards the
  // CertProvNextActionResponse.
  ASSERT_TRUE(next_action_future.Wait());
  EXPECT_EQ(next_action_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(next_action_future.GetError(), absl::nullopt);
  EXPECT_THAT(next_action_future.GetNextActionResponse(),
              EqualsProto(response.next_action_response()));
}

// Checks that all "Dynamic flow" API calls forward an error
// ClientCertificateProvisioningResponse correctly.
TEST_P(CertProvisioningClientNextActionProcessingTest, CertProvError) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  // Execute the CertProvisioningClient API call. Don't verify the filled
  // request proto - this is done by other tests in this file.
  NextActionFuture next_action_future;
  ExecuteCertProvisioningClientCall(
      &cert_provisioning_client,
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      next_action_future.GetNextActionCallback());

  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  // Make CloudPolicyClient answer the request.
  const CertProvisioningResponseErrorType error =
      CertProvisioningResponseError::CA_ERROR;
  em::ClientCertificateProvisioningResponse response;
  response.set_error(error);
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Expect that the CertProvisioningClient provides the error and an empty
  // CertProvNextActionResponse.
  ASSERT_TRUE(next_action_future.Wait());
  EXPECT_EQ(next_action_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(next_action_future.GetError(), absl::make_optional(error));
  EXPECT_THAT(
      next_action_future.GetNextActionResponse(),
      EqualsProto(CertProvisioningClient::CertProvNextActionResponse()));
}

// Checks that all "Dynamic flow" API calls forward a "DM_STATUS_.." error
// correctly.
TEST_P(CertProvisioningClientNextActionProcessingTest, DeviceManagementError) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  // Execute the CertProvisioningClient API call. Don't verify the filled
  // request proto - this is done by other tests in this file.
  NextActionFuture next_action_future;
  ExecuteCertProvisioningClientCall(
      &cert_provisioning_client,
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      next_action_future.GetNextActionCallback());

  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  // Make CloudPolicyClient answer the request with a device management error.
  em::ClientCertificateProvisioningResponse response;
  std::move(cert_prov_call.callback)
      .Run(policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND, response);

  // Expect that the CertProvisioningClient provides the error and an empty
  // CertProvNextActionResponse.
  ASSERT_TRUE(next_action_future.Wait());
  EXPECT_EQ(next_action_future.GetStatus(),
            policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND);
  EXPECT_EQ(next_action_future.GetError(), absl::nullopt);
  EXPECT_THAT(
      next_action_future.GetNextActionResponse(),
      EqualsProto(CertProvisioningClient::CertProvNextActionResponse()));
}

// Checks that if no CertProvNextActionResponse is filled , a decoding error
// will be signaled.
TEST_P(CertProvisioningClientNextActionProcessingTest,
       NoNextActionResponseFilled) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  // Execute the CertProvisioningClient API call. Don't verify the filled
  // request proto - this is done by other tests in this file.
  NextActionFuture next_action_future;
  ExecuteCertProvisioningClientCall(
      &cert_provisioning_client,
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      next_action_future.GetNextActionCallback());

  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  // Make CloudPolicyClient answer the request with no NextActionResponse
  // filled.
  em::ClientCertificateProvisioningResponse response;
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Expect that the CertProvisioningClient provides a deciding error and an
  // empty CertProvNextActionResponse.
  ASSERT_TRUE(next_action_future.Wait());
  EXPECT_EQ(next_action_future.GetStatus(),
            policy::DM_STATUS_RESPONSE_DECODING_ERROR);
  EXPECT_EQ(next_action_future.GetError(), absl::nullopt);
  EXPECT_THAT(
      next_action_future.GetNextActionResponse(),
      EqualsProto(CertProvisioningClient::CertProvNextActionResponse()));
}
// Checks that if no instruction was filled in CertProvNextActionResponse, a
// decoding error will be signaled.
TEST_P(CertProvisioningClientNextActionProcessingTest, NoInstructionFilled) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  // Execute the CertProvisioningClient API call. Don't verify the filled
  // request proto - this is done by other tests in this file.
  NextActionFuture next_action_future;
  ExecuteCertProvisioningClientCall(
      &cert_provisioning_client,
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      next_action_future.GetNextActionCallback());

  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  // Make CloudPolicyClient answer the request with no instruction filled.
  em::ClientCertificateProvisioningResponse response;
  {
    auto* next_action_response = response.mutable_next_action_response();
    next_action_response->set_invalidation_topic(kInvalidationTopic);
  }
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Expect that the CertProvisioningClient provides a deciding error and an
  // empty CertProvNextActionResponse.
  ASSERT_TRUE(next_action_future.Wait());
  EXPECT_EQ(next_action_future.GetStatus(),
            policy::DM_STATUS_RESPONSE_DECODING_ERROR);
  EXPECT_EQ(next_action_future.GetError(), absl::nullopt);
  EXPECT_THAT(
      next_action_future.GetNextActionResponse(),
      EqualsProto(CertProvisioningClient::CertProvNextActionResponse()));
}

// Checks that all "Dynamic flow" API calls forward a `try_again_later_ms`
// instruction correctly.
TEST_P(CertProvisioningClientNextActionProcessingTest, ExplicitTryLater) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  // Execute the CertProvisioningClient API call. Don't verify the filled
  // request proto - this is done by other tests in this file.
  NextActionFuture next_action_future;
  ExecuteCertProvisioningClientCall(
      &cert_provisioning_client,
      CertProvisioningClient::ProvisioningProcess(
          cert_scope(), kCertProfileId, kCertProfileVersion, kPublicKey),
      next_action_future.GetNextActionCallback());

  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  // Make CloudPolicyClient answer the request.
  em::ClientCertificateProvisioningResponse response;
  {
    auto* next_action_response = response.mutable_next_action_response();
    next_action_response->set_invalidation_topic(kInvalidationTopic);
    next_action_response->mutable_try_later_instruction()->set_delay_ms(3000);
  }
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Expect that the CertProvisioningClient provides the error and an empty
  // CertProvNextActionResponse.
  ASSERT_TRUE(next_action_future.Wait());
  EXPECT_EQ(next_action_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(next_action_future.GetError(), absl::nullopt);
  EXPECT_THAT(next_action_future.GetNextActionResponse(),
              EqualsProto(response.next_action_response()));
}

INSTANTIATE_TEST_SUITE_P(
    AllTests,
    CertProvisioningClientNextActionProcessingTest,
    ::testing::Combine(
        ::testing::Values(
            CertScopePair(CertScope::kUser, "google/chromeos/user"),
            CertScopePair(CertScope::kDevice, "google/chromeos/device")),
        ::testing::Values(
            NextActionProcessingTestCase{base::BindRepeating(
                [](CertProvisioningClient* client,
                   CertProvisioningClient::ProvisioningProcess
                       provisioning_process,
                   CertProvisioningClient::NextActionCallback callback) {
                  client->StartOrContinue(std::move(provisioning_process),
                                          std::move(callback));
                })},
            NextActionProcessingTestCase{base::BindRepeating(
                [](CertProvisioningClient* client,
                   CertProvisioningClient::ProvisioningProcess
                       provisioning_process,
                   CertProvisioningClient::NextActionCallback callback) {
                  client->Authorize(std::move(provisioning_process),
                                    /*va_challenge_response=*/std::string(),
                                    std::move(callback));
                })},
            NextActionProcessingTestCase{base::BindRepeating(
                [](CertProvisioningClient* client,
                   CertProvisioningClient::ProvisioningProcess
                       provisioning_process,
                   CertProvisioningClient::NextActionCallback callback) {
                  client->UploadProofOfPossession(
                      std::move(provisioning_process),
                      /*signature=*/std::string(), std::move(callback));
                })})));

}  // namespace ash::cert_provisioning
