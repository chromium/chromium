// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/cert_provisioning/cert_provisioning_client.h"

#include <string>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
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
// CertProvisioningClient::StartCallback.
using StartFuture = base::test::TestFuture<
    base::expected<em::CertProvStartResponse, CertProvisioningClient::Error>>;

// A TestFuture that supports waiting for a
// CertProvisioningClient::NextInstructionCallback.
using NextInstructionFuture = base::test::TestFuture<
    base::expected<em::CertProvGetNextInstructionResponse,
                   CertProvisioningClient::Error>>;

// A callback that provides no data but could provide an Error.
// Currently this matches AuthorizeCallback and UploadProofOfPossessionCallback.
using NoDataCallback = base::OnceCallback<void(
    base::expected<void, CertProvisioningClient::Error> result)>;

// A TestFuture that supports waiting for a NoDataCallback (see above).
using NoDataFuture =
    base::test::TestFuture<base::expected<void, CertProvisioningClient::Error>>;

// A TestFuture that supports waiting for a
// CertProvisioningClient::StartCsrCallback.
class StartCsrFuture
    : public base::test::TestFuture<
          policy::DeviceManagementStatus,
          std::optional<em::ClientCertificateProvisioningResponse::Error>,
          std::optional<int64_t>,
          std::string,
          std::string,
          em::HashingAlgorithm,
          std::vector<uint8_t>> {
 public:
  CertProvisioningClient::StartCsrCallback GetStartCsrCallback() {
    return GetCallback<
        policy::DeviceManagementStatus,
        std::optional<em::ClientCertificateProvisioningResponse::Error>,
        std::optional<int64_t>, const std::string&, const std::string&,
        em::HashingAlgorithm, std::vector<uint8_t>>();
  }

  policy::DeviceManagementStatus GetStatus() { return Get<0>(); }

  std::optional<em::ClientCertificateProvisioningResponse::Error> GetError() {
    return Get<1>();
  }

  std::optional<int64_t> GetTryLater() { return Get<2>(); }

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
          std::optional<em::ClientCertificateProvisioningResponse::Error>,
          std::optional<int64_t>> {
 public:
  CertProvisioningClient::FinishCsrCallback GetFinishCsrCallback() {
    return GetCallback();
  }

  policy::DeviceManagementStatus GetStatus() { return Get<0>(); }

  std::optional<em::ClientCertificateProvisioningResponse::Error> GetError() {
    return Get<1>();
  }

  std::optional<int64_t> GetTryLater() { return Get<2>(); }
};

// A TestFuture that supports waiting for a
// CertProvisioningClient::DownloadCertCallback.
class DownloadCertFuture
    : public base::test::TestFuture<
          policy::DeviceManagementStatus,
          std::optional<em::ClientCertificateProvisioningResponse::Error>,
          std::optional<int64_t>,
          std::string> {
 public:
  CertProvisioningClient::DownloadCertCallback GetDownloadCertCallback() {
    return GetCallback<
        policy::DeviceManagementStatus,
        std::optional<em::ClientCertificateProvisioningResponse::Error>,
        std::optional<int64_t>, const std::string&>();
  }

  policy::DeviceManagementStatus GetStatus() { return Get<0>(); }

  std::optional<em::ClientCertificateProvisioningResponse::Error> GetError() {
    return Get<1>();
  }

  std::optional<int64_t> GetTryLater() { return Get<2>(); }

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

  const std::string kCertProvisioningId = GenerateCertProvisioningId();
};

// Checks a successful invocation of Start.
TEST_P(CertProvisioningClientTest, StartSuccess) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  StartFuture start_future;
  cert_provisioning_client.Start(
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
      start_future.GetCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_provisioning_process_id(
        kCertProvisioningId);
    expected_request.set_certificate_scope(cert_scope_dm_api_string());
    expected_request.set_cert_profile_id(kCertProfileId);
    expected_request.set_policy_version(kCertProfileVersion);
    expected_request.set_public_key(kPublicKeyAsString);
    // Sets the request type, no actual data is required.
    expected_request.mutable_start_request();

    EXPECT_THAT(cert_prov_call.request, EqualsProto(expected_request));
  }

  // Make CloudPolicyClient answer the request.
  const std::string invalidation_topic = "test";

  em::ClientCertificateProvisioningResponse response;
  response.mutable_start_response()->set_invalidation_topic(invalidation_topic);

  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that CertProvisioningClient has forwarded the answer correctly.
  ASSERT_TRUE(start_future.Get().has_value());
  EXPECT_EQ(start_future.Get().value().invalidation_topic(),
            invalidation_topic);
}

// Checks a successful invocation of GetNextInstruction.
TEST_P(CertProvisioningClientTest, GetNextInstructionSuccess) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  NextInstructionFuture next_instruction_future;
  cert_provisioning_client.GetNextInstruction(
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
      next_instruction_future.GetCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_provisioning_process_id(
        kCertProvisioningId);
    expected_request.set_certificate_scope(cert_scope_dm_api_string());
    expected_request.set_cert_profile_id(kCertProfileId);
    expected_request.set_policy_version(kCertProfileVersion);
    expected_request.set_public_key(kPublicKeyAsString);
    // Sets the request type, no actual data is required.
    expected_request.mutable_get_next_instruction_request();

    EXPECT_THAT(cert_prov_call.request, EqualsProto(expected_request));
  }

  // Make CloudPolicyClient answer the request.
  const std::string va_challenge = "test";

  em::CertProvGetNextInstructionResponse next_instruction_response;
  next_instruction_response.mutable_authorize_instruction()->set_va_challenge(
      va_challenge);
  em::ClientCertificateProvisioningResponse response;
  *response.mutable_get_next_instruction_response() = next_instruction_response;

  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that CertProvisioningClient has forwarded the answer correctly.
  ASSERT_TRUE(next_instruction_future.Get().has_value());
  EXPECT_THAT(next_instruction_future.Get().value(),
              EqualsProto(next_instruction_response));
}

// Checks a successful invocation of Authorize.
TEST_P(CertProvisioningClientTest, AuthorizeSuccess) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  NoDataFuture no_data_future;
  cert_provisioning_client.Authorize(
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
      kVaChallengeResponse, no_data_future.GetCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_provisioning_process_id(
        kCertProvisioningId);
    expected_request.set_certificate_scope(cert_scope_dm_api_string());
    expected_request.set_cert_profile_id(kCertProfileId);
    expected_request.set_policy_version(kCertProfileVersion);
    expected_request.set_public_key(kPublicKeyAsString);

    auto* authorize_request = expected_request.mutable_authorize_request();
    authorize_request->set_va_challenge_response(kVaChallengeResponse);

    EXPECT_THAT(cert_prov_call.request, EqualsProto(expected_request));
  }

  // Make CloudPolicyClient answer the request.
  em::ClientCertificateProvisioningResponse response;
  response.mutable_authorize_response();

  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that the response has no error.
  EXPECT_TRUE(no_data_future.Get().has_value());
}

// Checks a successful invocation of UploadProofOfPossession.
TEST_P(CertProvisioningClientTest, UploadProofOfPossessionSuccess) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  NoDataFuture no_data_future;
  cert_provisioning_client.UploadProofOfPossession(
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
      kSignature, no_data_future.GetCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_provisioning_process_id(
        kCertProvisioningId);
    expected_request.set_certificate_scope(cert_scope_dm_api_string());
    expected_request.set_cert_profile_id(kCertProfileId);
    expected_request.set_policy_version(kCertProfileVersion);
    expected_request.set_public_key(kPublicKeyAsString);

    auto* upload_proof_of_possession_request =
        expected_request.mutable_upload_proof_of_possession_request();
    upload_proof_of_possession_request->set_signature(kSignature);

    EXPECT_THAT(cert_prov_call.request, EqualsProto(expected_request));
  }

  // Make CloudPolicyClient answer the request.
  em::ClientCertificateProvisioningResponse response;
  response.mutable_upload_proof_of_possession_response();

  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Check that the response has no error.
  EXPECT_TRUE(no_data_future.Get().has_value());
}

// 1. Checks that `StartCsr` generates a correct request.
// 2. Checks that CertProvisioningClient correctly extracts data from a response
// that contains data.
TEST_P(CertProvisioningClientTest, StartCsrSuccess) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  StartCsrFuture start_csr_future;
  cert_provisioning_client.StartCsr(
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
      start_csr_future.GetStartCsrCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_provisioning_process_id(
        kCertProvisioningId);
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
  EXPECT_EQ(start_csr_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(start_csr_future.GetError(), std::nullopt);
  EXPECT_EQ(start_csr_future.GetTryLater(), std::nullopt);
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
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
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
  EXPECT_EQ(start_csr_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(start_csr_future.GetError(), std::nullopt);
  EXPECT_EQ(start_csr_future.GetTryLater(), std::make_optional(try_later));
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
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
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
  EXPECT_EQ(start_csr_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(start_csr_future.GetError(), std::make_optional(error));
  EXPECT_EQ(start_csr_future.GetTryLater(), std::nullopt);
}

// 1. Checks that `FinishCsr` generates a correct request.
// 2. Checks that CertProvisioningClient correctly extracts data from a response
// that contains data.
TEST_P(CertProvisioningClientTest, FinishCsrSuccess) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  FinishCsrFuture finish_csr_future;
  cert_provisioning_client.FinishCsr(
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
      kVaChallengeResponse, kSignature,
      finish_csr_future.GetFinishCsrCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_provisioning_process_id(
        kCertProvisioningId);
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
  EXPECT_EQ(finish_csr_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(finish_csr_future.GetError(), std::nullopt);
  EXPECT_EQ(finish_csr_future.GetTryLater(), std::nullopt);
}

// Checks that CertProvisioningClient correctly reacts on the `error` field
// in a response to FinishCsr.
TEST_P(CertProvisioningClientTest, FinishCsrError) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  FinishCsrFuture finish_csr_future;
  cert_provisioning_client.FinishCsr(
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
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
  EXPECT_EQ(finish_csr_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(finish_csr_future.GetError(), std::make_optional(error));
  EXPECT_EQ(finish_csr_future.GetTryLater(), std::nullopt);
}

// 1. Checks that `DownloadCert` generates a correct request.
// 2. Checks that CertProvisioningClient correctly extracts data from a response
// that contains data.
TEST_P(CertProvisioningClientTest, DownloadCertSuccess) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  DownloadCertFuture download_cert_future;
  cert_provisioning_client.DownloadCert(
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
      download_cert_future.GetDownloadCertCallback());

  // Expect one request to CloudPolicyClient, verify its contents.
  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  {
    em::ClientCertificateProvisioningRequest expected_request;
    expected_request.set_certificate_provisioning_process_id(
        kCertProvisioningId);
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
  EXPECT_EQ(download_cert_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(download_cert_future.GetError(), std::nullopt);
  EXPECT_EQ(download_cert_future.GetTryLater(), std::nullopt);
  EXPECT_EQ(download_cert_future.GetPemEncodedCertificate(), kPemEncodedCert);
}

// Checks that CertProvisioningClient correctly reacts on the `error` field
// in a response to DownloadCert.
TEST_P(CertProvisioningClientTest, DownloadCertError) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  DownloadCertFuture download_cert_future;
  cert_provisioning_client.DownloadCert(
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
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
  EXPECT_EQ(download_cert_future.GetStatus(), policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(download_cert_future.GetError(), std::make_optional(error));
  EXPECT_EQ(download_cert_future.GetTryLater(), std::nullopt);
  EXPECT_EQ(download_cert_future.GetPemEncodedCertificate(), std::string());
}

INSTANTIATE_TEST_SUITE_P(
    AllScopes,
    CertProvisioningClientTest,
    ::testing::Values(CertScopePair(CertScope::kUser, "google/chromeos/user"),
                      CertScopePair(CertScope::kDevice,
                                    "google/chromeos/device")));

// A Test case for CertProvisioningClientErrorHandlingTest.
struct ErrorHandlingTestCase {
  // Invokes a CertProvisioningClient API call.
  // As these tests only test error cases, it is expected that any callback will
  // be adapted to a NoDataCallback.
  base::RepeatingCallback<void(
      CertProvisioningClient*,
      CertProvisioningClient::ProvisioningProcess provisioining_process,
      NoDataCallback callback)>
      act_function;
};

// Test fixture for CertProvisioningClient, parametrized by CertScope and a
// ErrorHandlingTestCase which implements a call to one of the
// "dynamic flow" API calls.
// This is useful for testing error response processing across all "dynamic
// flow" API calls.
class CertProvisioningClientErrorHandlingTest
    : public CertProvisioningClientTestBase,
      public testing::WithParamInterface<
          std::tuple<CertScopePair, ErrorHandlingTestCase>> {
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
      NoDataCallback callback) const {
    std::get<1>(GetParam())
        .act_function.Run(client, std::move(provisioning_process),
                          std::move(callback));
  }

  const std::string kCertProvisioningId = GenerateCertProvisioningId();

 private:
  const CertScopePair& cert_scope_pair() const {
    return std::get<0>(GetParam());
  }
};

// Checks that all "dynamic flow" API calls forward a CertProvBackendError
// correctly.
TEST_P(CertProvisioningClientErrorHandlingTest, CertProvBackendError) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  // Execute the CertProvisioningClient API call. Don't verify the filled
  // request proto - this is done by other tests in this file.
  NoDataFuture no_data_future;
  ExecuteCertProvisioningClientCall(
      &cert_provisioning_client,
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
      no_data_future.GetCallback());

  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  // Make CloudPolicyClient answer the request.
  const em::CertProvBackendError::Error error =
      em::CertProvBackendError::CA_FAILURE;
  const std::string debug_message = "debug info";
  em::ClientCertificateProvisioningResponse response;
  response.mutable_backend_error()->set_error(error);
  response.mutable_backend_error()->set_debug_message(debug_message);
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Expect that the CertProvisioningClient provides the error.
  ASSERT_FALSE(no_data_future.Get().has_value());
  EXPECT_EQ(no_data_future.Get().error().device_management_status,
            policy::DM_STATUS_SUCCESS);
  EXPECT_EQ(no_data_future.Get().error().backend_error.error(), error);
  EXPECT_EQ(no_data_future.Get().error().backend_error.debug_message(),
            debug_message);
}

// Checks that all "dynamic flow" API calls forward forward a "DM_STATUS_.."
// error correctly.
TEST_P(CertProvisioningClientErrorHandlingTest, DeviceManagementError) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  // Execute the CertProvisioningClient API call. Don't verify the filled
  // request proto - this is done by other tests in this file.
  NoDataFuture no_data_future;
  ExecuteCertProvisioningClientCall(
      &cert_provisioning_client,
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
      no_data_future.GetCallback());

  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  // Make CloudPolicyClient answer the request with a device management error.
  em::ClientCertificateProvisioningResponse response;
  std::move(cert_prov_call.callback)
      .Run(policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND, response);

  // Expect that the CertProvisioningClient provides the error.
  ASSERT_FALSE(no_data_future.Get().has_value());
  EXPECT_EQ(no_data_future.Get().error().device_management_status,
            policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND);
}

// Checks that if no "oneof" field of ClientCertificateProvisioningResponseis
// filled, a decoding error will be signaled.
TEST_P(CertProvisioningClientErrorHandlingTest, ResponseFieldNotFilled) {
  CertProvisioningClientImpl cert_provisioning_client(cloud_policy_client_);

  // Execute the CertProvisioningClient API call. Don't verify the filled
  // request proto - this is done by other tests in this file.
  NoDataFuture no_data_future;
  ExecuteCertProvisioningClientCall(
      &cert_provisioning_client,
      CertProvisioningClient::ProvisioningProcess(
          kCertProvisioningId, cert_scope(), kCertProfileId,
          kCertProfileVersion, kPublicKey),
      no_data_future.GetCallback());

  ASSERT_THAT(cloud_policy_client_.cert_prov_calls(), SizeIs(1));
  FakeCloudPolicyClient::CertProvCall& cert_prov_call =
      cloud_policy_client_.cert_prov_calls().back();
  // Make CloudPolicyClient answer the request with no "oneof" field filled.
  em::ClientCertificateProvisioningResponse response;
  std::move(cert_prov_call.callback).Run(policy::DM_STATUS_SUCCESS, response);

  // Expect that the CertProvisioningClient provides a decoding error.
  ASSERT_FALSE(no_data_future.Get().has_value());
  EXPECT_EQ(no_data_future.Get().error().device_management_status,
            policy::DM_STATUS_RESPONSE_DECODING_ERROR);
}

template <typename ResponseType>
void AdaptToNoDataCallback(
    NoDataCallback no_data_callback,
    base::expected<ResponseType, CertProvisioningClient::Error> result) {
  if (result.has_value()) {
    return std::move(no_data_callback).Run({});
  }
  std::move(no_data_callback).Run(base::unexpected(result.error()));
}

INSTANTIATE_TEST_SUITE_P(
    AllTests,
    CertProvisioningClientErrorHandlingTest,
    ::testing::Combine(
        ::testing::Values(
            CertScopePair(CertScope::kUser, "google/chromeos/user"),
            CertScopePair(CertScope::kDevice, "google/chromeos/device")),
        ::testing::Values(
            ErrorHandlingTestCase{base::BindRepeating(
                [](CertProvisioningClient* client,
                   CertProvisioningClient::ProvisioningProcess
                       provisioning_process,
                   NoDataCallback callback) {
                  client->Start(
                      std::move(provisioning_process),
                      base::BindOnce(
                          &AdaptToNoDataCallback<em::CertProvStartResponse>,
                          std::move(callback)));
                })},
            ErrorHandlingTestCase{base::BindRepeating(
                [](CertProvisioningClient* client,
                   CertProvisioningClient::ProvisioningProcess
                       provisioning_process,
                   NoDataCallback callback) {
                  client->GetNextInstruction(
                      std::move(provisioning_process),
                      base::BindOnce(
                          &AdaptToNoDataCallback<
                              em::CertProvGetNextInstructionResponse>,
                          std::move(callback)));
                })},
            ErrorHandlingTestCase{base::BindRepeating(
                [](CertProvisioningClient* client,
                   CertProvisioningClient::ProvisioningProcess
                       provisioning_process,
                   NoDataCallback callback) {
                  client->Authorize(std::move(provisioning_process),
                                    /*va_challenge_response=*/std::string(),
                                    std::move(callback));
                })},
            ErrorHandlingTestCase{base::BindRepeating(
                [](CertProvisioningClient* client,
                   CertProvisioningClient::ProvisioningProcess
                       provisioning_process,
                   NoDataCallback callback) {
                  client->UploadProofOfPossession(
                      std::move(provisioning_process),
                      /*signature=*/std::string(), std::move(callback));
                })})));

}  // namespace ash::cert_provisioning
