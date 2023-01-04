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
    enterprise_management::ClientCertificateProvisioningRequest request;
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
      enterprise_management::ClientCertificateProvisioningRequest request,
      ClientCertProvisioningRequestCallback callback) {
    cert_prov_calls_.push_back({std::move(request), std::move(callback)});
  }

  std::vector<CertProvCall> cert_prov_calls_;
};

// A TestFuture that supports waiting for a
// CertProvisioningClient::StartCsrCallback.
class StartCsrFuture
    : public base::test::TestFuture<
          policy::DeviceManagementStatus,
          absl::optional<enterprise_management::
                             ClientCertificateProvisioningResponse::Error>,
          absl::optional<int64_t>,
          std::string,
          std::string,
          enterprise_management::HashingAlgorithm,
          std::string> {
 public:
  CertProvisioningClient::StartCsrCallback GetStartCsrCallback() {
    return GetCallback<
        policy::DeviceManagementStatus,
        absl::optional<enterprise_management::
                           ClientCertificateProvisioningResponse::Error>,
        absl::optional<int64_t>, const std::string&, const std::string&,
        enterprise_management::HashingAlgorithm, const std::string&>();
  }

  policy::DeviceManagementStatus GetStatus() { return Get<0>(); }

  absl::optional<
      enterprise_management::ClientCertificateProvisioningResponse::Error>
  GetError() {
    return Get<1>();
  }

  absl::optional<int64_t> GetTryLater() { return Get<2>(); }

  const std::string& GetInvalidationTopic() { return Get<3>(); }

  const std::string& GetVaChallenge() { return Get<4>(); }

  enterprise_management::HashingAlgorithm GetHashingAlgorithm() {
    return Get<5>();
  }

  const std::string& GetDataToSign() { return Get<6>(); }
};

// A TestFuture that supports waiting for a
// CertProvisioningClient::FinishCsrCallback.
class FinishCsrFuture
    : public base::test::TestFuture<
          policy::DeviceManagementStatus,
          absl::optional<enterprise_management::
                             ClientCertificateProvisioningResponse::Error>,
          absl::optional<int64_t>> {
 public:
  CertProvisioningClient::FinishCsrCallback GetFinishCsrCallback() {
    return GetCallback();
  }

  policy::DeviceManagementStatus GetStatus() { return Get<0>(); }

  absl::optional<
      enterprise_management::ClientCertificateProvisioningResponse::Error>
  GetError() {
    return Get<1>();
  }

  absl::optional<int64_t> GetTryLater() { return Get<2>(); }
};

// A TestFuture that supports waiting for a
// CertProvisioningClient::DownloadCertCallback.
class DownloadCertFuture
    : public base::test::TestFuture<
          policy::DeviceManagementStatus,
          absl::optional<enterprise_management::
                             ClientCertificateProvisioningResponse::Error>,
          absl::optional<int64_t>,
          std::string> {
 public:
  CertProvisioningClient::DownloadCertCallback GetDownloadCertCallback() {
    return GetCallback<
        policy::DeviceManagementStatus,
        absl::optional<enterprise_management::
                           ClientCertificateProvisioningResponse::Error>,
        absl::optional<int64_t>, const std::string&>();
  }

  policy::DeviceManagementStatus GetStatus() { return Get<0>(); }

  absl::optional<
      enterprise_management::ClientCertificateProvisioningResponse::Error>
  GetError() {
    return Get<1>();
  }

  absl::optional<int64_t> GetTryLater() { return Get<2>(); }

  const std::string& GetPemEncodedCertificate() { return Get<3>(); }
};

}  // namespace

// Tuple of CertScope enum value and corresponding device management protocol
// string.
using CertScopePair = std::tuple<CertScope, std::string>;

class CertProvisioningClientTest
    : public testing::TestWithParam<CertScopePair> {
 public:
  CertProvisioningClientTest() = default;
  ~CertProvisioningClientTest() override = default;

  CertScope cert_scope() const { return std::get<0>(GetParam()); }

  const std::string& cert_scope_dm_api_string() const {
    return std::get<1>(GetParam());
  }

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
  const std::string kDataToSign = "fake_data_to_sign_1";
  const em::HashingAlgorithm kHashAlgorithm = em::HashingAlgorithm::SHA256;
  const em::SigningAlgorithm kSignAlgorithm =
      em::SigningAlgorithm::RSA_PKCS1_V1_5;
  const std::string kVaChallengeResponse = "fake_va_challenge_response_1";
  const std::string kSignature = "fake_signature_1";
  const std::string kPemEncodedCert = "fake_pem_encoded_cert_1";
};

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
    start_csr_response->set_data_to_sign(kDataToSign);
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
  EXPECT_EQ(start_csr_future.GetDataToSign(), kDataToSign);
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

}  // namespace ash::cert_provisioning
