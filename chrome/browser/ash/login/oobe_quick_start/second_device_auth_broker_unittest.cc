// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::quick_start {

using ::testing::_;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Optional;
using ::testing::StrictMock;
using ::testing::VariantWith;
using ::testing::WithArg;

namespace {

constexpr char kGetChallengeDataUrl[] =
    "https://devicesignin-pa.googleapis.com/v1/getchallengedata";
constexpr char kFakeChallengeDataResponse[] =
    "{"
    "\"challengeData\": {"
    "  \"challenge\": "
    "\"AKVcFQJJ0zreBQSrWiDJlFmeTr6K1Ik+"
    "i58k4p5A64dYYcofARHmhUNQrh0vpYZ4zbOvyBSamG/"
    "hyOxa7WdmZHLfEyobJ2FyifgY114deg==\""
    "}"
    "}";
constexpr char kInvalidBase64ChallengeDataResponse[] =
    "{"
    "\"challengeData\": {"
    "  \"challenge\": "
    "\"Not-a-base64-character()\""
    "}"
    "}";
constexpr char kFidoCredentialId[] = "fido_credential_id";

MATCHER_P(ProtoBufContentBindingEq, expected, "") {
  return arg.content_binding() == expected;
}

// This class simply delegates all API calls to its mock object. It is used for
// passing information from inside `SecondDeviceAuthBroker` to the mock object,
// without passing ownership of the mock object to `SecondDeviceAuthBroker`.
class MockAttestationFlowFacade : public attestation::MockAttestationFlow {
 public:
  explicit MockAttestationFlowFacade(
      raw_ptr<attestation::MockAttestationFlow> mock_attestation_flow)
      : mock_attestation_flow_(std::move(mock_attestation_flow)) {}
  MockAttestationFlowFacade(const MockAttestationFlowFacade&) = delete;
  MockAttestationFlowFacade& operator=(const MockAttestationFlowFacade&) =
      delete;
  ~MockAttestationFlowFacade() override = default;

  void GetCertificate(
      attestation::AttestationCertificateProfile certificate_profile,
      const AccountId& account_id,
      const std::string& request_origin,
      bool force_new_key,
      ::attestation::KeyType key_crypto_type,
      const std::string& key_name,
      const absl::optional<CertProfileSpecificData>& profile_specific_data,
      CertificateCallback callback) override {
    mock_attestation_flow_->GetCertificate(
        certificate_profile, account_id, request_origin, force_new_key,
        key_crypto_type, key_name, profile_specific_data, std::move(callback));
  }

 private:
  raw_ptr<attestation::MockAttestationFlow> mock_attestation_flow_;
};

}  // namespace

class SecondDeviceAuthBrokerTest : public ::testing::Test {
 public:
  SecondDeviceAuthBrokerTest()
      : second_device_auth_broker_(test_factory_.GetSafeWeakWrapper(),
                                   std::make_unique<MockAttestationFlowFacade>(
                                       &mock_attestation_flow_)) {}

  SecondDeviceAuthBrokerTest(const SecondDeviceAuthBrokerTest&) = delete;
  SecondDeviceAuthBrokerTest& operator=(const SecondDeviceAuthBrokerTest&) =
      delete;
  ~SecondDeviceAuthBrokerTest() override = default;

 protected:
  base::expected<std::string, GoogleServiceAuthError> GetChallengeBytes() {
    base::expected<std::string, GoogleServiceAuthError> response;
    base::RunLoop run_loop;
    SecondDeviceAuthBroker::ChallengeBytesCallback callback =
        base::BindLambdaForTesting(
            [&response, &run_loop](
                const base::expected<std::string, GoogleServiceAuthError>&
                    returned_response) -> void {
              response = returned_response;
              run_loop.Quit();
            });
    second_device_auth_broker_.GetChallengeBytes(std::move(callback));
    run_loop.Run();

    return response;
  }

  base::expected<std::string, SecondDeviceAuthBroker::AttestationErrorType>
  FetchAttestationCertificate(const std::string& fido_credential_id) {
    base::expected<std::string, SecondDeviceAuthBroker::AttestationErrorType>
        response;
    base::RunLoop run_loop;
    SecondDeviceAuthBroker::AttestationCertificateCallback callback =
        base::BindLambdaForTesting(
            [&response, &run_loop](
                const base::expected<
                    std::string, SecondDeviceAuthBroker::AttestationErrorType>&
                    returned_response) -> void {
              response = returned_response;
              run_loop.Quit();
            });
    second_device_auth_broker_.FetchAttestationCertificate(fido_credential_id,
                                                           std::move(callback));
    run_loop.Run();

    return response;
  }

  void AddFakeResponse(const std::string& url, const std::string& response) {
    test_factory_.AddResponse(url, response);
  }

  void SimulateAuthError(const std::string& url) {
    test_factory_.AddResponse(url, /*content=*/std::string(),
                              net::HTTP_UNAUTHORIZED);
  }

  attestation::MockAttestationFlow& mock_attestation_flow() {
    return mock_attestation_flow_;
  }

 private:
  // `task_environment_` must be the first member.
  base::test::TaskEnvironment task_environment_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_factory_;
  StrictMock<attestation::MockAttestationFlow> mock_attestation_flow_;
  SecondDeviceAuthBroker second_device_auth_broker_;
};

TEST_F(SecondDeviceAuthBrokerTest,
       GetChallengeBytesReturnsAnErrorForAuthErrors) {
  SimulateAuthError(kGetChallengeDataUrl);
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(response.error().state(),
              Eq(GoogleServiceAuthError::State::SERVICE_ERROR));
}

TEST_F(SecondDeviceAuthBrokerTest,
       GetChallengeBytesReturnsAnErrorForMalformedResponse) {
  AddFakeResponse(kGetChallengeDataUrl, "");
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(response.error().state(),
              Eq(GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE));

  AddFakeResponse(kGetChallengeDataUrl, "{}");
  response = GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(response.error().state(),
              Eq(GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE));

  AddFakeResponse(kGetChallengeDataUrl, "{\"challengeData\": \"\"}");
  response = GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(response.error().state(),
              Eq(GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE));
}

TEST_F(SecondDeviceAuthBrokerTest,
       GetChallengeBytesReturnsAnErrorForBase64ParsingError) {
  AddFakeResponse(kGetChallengeDataUrl, kInvalidBase64ChallengeDataResponse);
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(response.error().state(),
              Eq(GoogleServiceAuthError::State::UNEXPECTED_SERVICE_RESPONSE));
}

TEST_F(SecondDeviceAuthBrokerTest, GetChallengeBytesReturnsChallengeBytes) {
  AddFakeResponse(kGetChallengeDataUrl, kFakeChallengeDataResponse);
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_THAT(response.has_value(), IsTrue());
  EXPECT_THAT(response->size(), Gt(0UL));
}

TEST_F(
    SecondDeviceAuthBrokerTest,
    FetchAttestationCertificateReturnsATransientErrorForUnspecifiedFailures) {
  EXPECT_CALL(
      mock_attestation_flow(),
      GetCertificate(
          /*certificate_profile=*/attestation::AttestationCertificateProfile::
              PROFILE_DEVICE_SETUP_CERTIFICATE,
          /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
          /*force_new_key=*/_, /*key_crypto_type=*/_,
          /*key_name=*/attestation::kDeviceSetupKey,
          /*profile_specific_data=*/_, /*callback=*/_))
      .WillOnce(WithArg<7>(
          Invoke([](attestation::AttestationFlow::CertificateCallback callback)
                     -> void {
            std::move(callback).Run(
                /*status=*/ash::attestation::AttestationStatus::
                    ATTESTATION_UNSPECIFIED_FAILURE,
                /*pem_certificate_chain=*/std::string());
          })));

  base::expected<std::string, SecondDeviceAuthBroker::AttestationErrorType>
      response = FetchAttestationCertificate(kFidoCredentialId);
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(
      response.error(),
      Eq(SecondDeviceAuthBroker::AttestationErrorType::kTransientError));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchAttestationCertificateReturnsAPermanentErrorForBadRequests) {
  EXPECT_CALL(
      mock_attestation_flow(),
      GetCertificate(
          /*certificate_profile=*/attestation::AttestationCertificateProfile::
              PROFILE_DEVICE_SETUP_CERTIFICATE,
          /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
          /*force_new_key=*/_, /*key_crypto_type=*/_,
          /*key_name=*/attestation::kDeviceSetupKey,
          /*profile_specific_data=*/_, /*callback=*/_))
      .WillOnce(WithArg<7>(
          Invoke([](attestation::AttestationFlow::CertificateCallback callback)
                     -> void {
            std::move(callback).Run(
                /*status=*/ash::attestation::AttestationStatus::
                    ATTESTATION_SERVER_BAD_REQUEST_FAILURE,
                /*pem_certificate_chain=*/std::string());
          })));

  base::expected<std::string, SecondDeviceAuthBroker::AttestationErrorType>
      response = FetchAttestationCertificate(kFidoCredentialId);
  ASSERT_THAT(response.has_value(), IsFalse());
  EXPECT_THAT(
      response.error(),
      Eq(SecondDeviceAuthBroker::AttestationErrorType::kPermanentError));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchAttestationCertificateReturnsACertificate) {
  const std::string kCertificate = "fake_certificate";
  EXPECT_CALL(
      mock_attestation_flow(),
      GetCertificate(
          /*certificate_profile=*/attestation::AttestationCertificateProfile::
              PROFILE_DEVICE_SETUP_CERTIFICATE,
          /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
          /*force_new_key=*/_, /*key_crypto_type=*/_,
          /*key_name=*/attestation::kDeviceSetupKey,
          /*profile_specific_data=*/
          Optional(
              VariantWith<::attestation::DeviceSetupCertificateRequestMetadata>(
                  ProtoBufContentBindingEq(kFidoCredentialId))),
          /*callback*/ _))
      .WillOnce(WithArg<
                7>(Invoke([&kCertificate](
                              attestation::AttestationFlow::CertificateCallback
                                  callback) -> void {
        std::move(callback).Run(
            /*status=*/ash::attestation::AttestationStatus::ATTESTATION_SUCCESS,
            /*pem_certificate_chain=*/kCertificate);
      })));

  base::expected<std::string, SecondDeviceAuthBroker::AttestationErrorType>
      response = FetchAttestationCertificate(kFidoCredentialId);
  ASSERT_THAT(response.has_value(), IsTrue());
  EXPECT_THAT(response.value(), Eq(kCertificate));
}

}  //  namespace ash::quick_start
