// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/account_id/account_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace ash::quick_start {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::Field;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Optional;
using ::testing::Property;
using ::testing::StrictMock;
using ::testing::VariantWith;
using ::testing::WithArg;

using State = GoogleServiceAuthError::State;
using RefreshTokenUnknownErrorResponse =
    SecondDeviceAuthBroker::RefreshTokenUnknownErrorResponse;
using RefreshTokenSuccessResponse =
    SecondDeviceAuthBroker::RefreshTokenSuccessResponse;
using RefreshTokenParsingErrorResponse =
    SecondDeviceAuthBroker::RefreshTokenParsingErrorResponse;
using RefreshTokenRejectionResponse =
    SecondDeviceAuthBroker::RefreshTokenRejectionResponse;
using RefreshTokenAdditionalChallengesOnSourceResponse =
    SecondDeviceAuthBroker::RefreshTokenAdditionalChallengesOnSourceResponse;
using RefreshTokenAdditionalChallengesOnTargetResponse =
    SecondDeviceAuthBroker::RefreshTokenAdditionalChallengesOnTargetResponse;

namespace {

constexpr char kGetChallengeDataUrl[] =
    "https://devicesignin-pa.googleapis.com/v1/getchallengedata";
constexpr char kStartSessionUrl[] =
    "https://devicesignin-pa.googleapis.com/v1/startsession";
constexpr char kFakeChallengeDataResponse[] = R"({
      "challengeData": {
        "challenge": "eA=="
      }
    })";
constexpr char kInvalidBase64ChallengeDataResponse[] = R"({
      "challengeData": {
        "challenge": "Not-a-base64-character)("
      }
    })";
constexpr char kOAuthRefreshTokenSuccessBody[] = R"({
      "refresh_token": "fake-refresh-token",
      "access_token": "fake-access-token",
      "expires_in": 99999
    })";
constexpr char kFidoCredentialId[] = "fido_credential_id";
constexpr char kCertificate[] = "fake_certificate";
constexpr char kFakeDeviceId[] = "fake_device_id";
constexpr char kTargetDeviceType[] = "target_device_type";
constexpr char kChromeOS[] = "CHROME_OS";

MATCHER_P(ProtoBufContentBindingEq, expected, "") {
  return arg.content_binding() == expected;
}

MATCHER_P(RefreshTokenAdditionalChallengesOnTargetResponseEq, expected, "") {
  return ExplainMatchResult(
      AllOf(
          Field("email",
                &RefreshTokenAdditionalChallengesOnTargetResponse::email,
                Eq(expected.email)),
          Field("fallback_url",
                &RefreshTokenAdditionalChallengesOnTargetResponse::fallback_url,
                Eq(expected.fallback_url))),
      arg, result_listener);
}

MATCHER_P(RefreshTokenAdditionalChallengesOnSourceResponseEq, expected, "") {
  return ExplainMatchResult(
      AllOf(
          Field("email",
                &RefreshTokenAdditionalChallengesOnSourceResponse::email,
                Eq(expected.email)),
          Field("fallback_url",
                &RefreshTokenAdditionalChallengesOnSourceResponse::fallback_url,
                Eq(expected.fallback_url)),
          Field("target_session_identifier",
                &RefreshTokenAdditionalChallengesOnSourceResponse::
                    target_session_identifier,
                Eq(expected.target_session_identifier))),
      arg, result_listener);
}

MATCHER_P(RefreshTokenSuccessResponseEq, expected, "") {
  return ExplainMatchResult(
      AllOf(Field("email", &RefreshTokenSuccessResponse::email,
                  Eq(expected.email)),
            Field("refresh_token", &RefreshTokenSuccessResponse::refresh_token,
                  Eq(expected.refresh_token))),
      arg, result_listener);
}

MATCHER_P(RefreshTokenRejectionResponseEq, expected, "") {
  return ExplainMatchResult(
      AllOf(Field("email", &RefreshTokenRejectionResponse::email,
                  Eq(expected.email)),
            Field("reason", &RefreshTokenRejectionResponse::reason,
                  Eq(expected.reason))),
      arg, result_listener);
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
      : second_device_auth_broker_(kFakeDeviceId,
                                   test_factory_.GetSafeWeakWrapper(),
                                   std::make_unique<MockAttestationFlowFacade>(
                                       &mock_attestation_flow_)) {}

  SecondDeviceAuthBrokerTest(const SecondDeviceAuthBrokerTest&) = delete;
  SecondDeviceAuthBrokerTest& operator=(const SecondDeviceAuthBrokerTest&) =
      delete;
  ~SecondDeviceAuthBrokerTest() override = default;

 protected:
  base::expected<std::string, GoogleServiceAuthError> GetChallengeBytes() {
    base::test::TestFuture<
        const base::expected<std::string, GoogleServiceAuthError>&>
        future;
    second_device_auth_broker_.GetChallengeBytes(future.GetCallback());
    return future.Get();
  }

  base::expected<std::string, SecondDeviceAuthBroker::AttestationErrorType>
  FetchAttestationCertificate(const std::string& fido_credential_id) {
    base::test::TestFuture<const base::expected<
        std::string, SecondDeviceAuthBroker::AttestationErrorType>&>
        future;
    second_device_auth_broker_.FetchAttestationCertificate(
        fido_credential_id, future.GetCallback());
    return future.Get();
  }

  SecondDeviceAuthBroker::RefreshTokenResponse FetchRefreshToken(
      const FidoAssertionInfo& fido_assertion_info,
      const std::string& certificate) {
    base::test::TestFuture<const SecondDeviceAuthBroker::RefreshTokenResponse&>
        future;
    second_device_auth_broker_.FetchRefreshToken(
        fido_assertion_info, certificate, future.GetCallback());
    return future.Get();
  }

  void AddFakeResponse(const std::string& url,
                       const std::string& response,
                       net::HttpStatusCode status = net::HTTP_OK) {
    test_factory_.AddResponse(url, response, status);
  }

  // Sets an `interceptor`. Overwrites any other interceptor that may have been
  // previously set.
  void SetInterceptor(
      const network::TestURLLoaderFactory::Interceptor& interceptor) {
    test_factory_.SetInterceptor(interceptor);
  }

  void SimulateBadRequest(const std::string& url) {
    test_factory_.AddResponse(url, /*content=*/std::string(),
                              net::HTTP_BAD_REQUEST);
  }

  void SimulateAuthError(const std::string& url) {
    test_factory_.AddResponse(url, /*content=*/std::string(),
                              net::HTTP_UNAUTHORIZED);
  }

  void SimulateOAuthTokenFetchSuccess() {
    test_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(),
        kOAuthRefreshTokenSuccessBody);
  }

  void SimulateOAuthTokenFetchFailure() {
    test_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(), /*content=*/"{}",
        net::HTTP_BAD_REQUEST);
  }

  attestation::MockAttestationFlow& mock_attestation_flow() {
    return mock_attestation_flow_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return test_factory_.GetSafeWeakWrapper();
  }

 private:
  // `task_environment_` must be the first member.
  base::test::TaskEnvironment task_environment_;

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_factory_;
  StrictMock<attestation::MockAttestationFlow> mock_attestation_flow_;
  SecondDeviceAuthBroker second_device_auth_broker_;
};

// Test fixture used to run death tests. A separate test fixture is used to
// improve performance so that expected-death-crashes do not use the same
// threading context as other tests. See
// https://github.com/google/googletest/blob/main/docs/advanced.md#death-test-naming
using SecondDeviceAuthBrokerDeathTest = SecondDeviceAuthBrokerTest;

TEST_F(SecondDeviceAuthBrokerDeathTest,
       SecondDeviceAuthBrokerValidatesDeviceId) {
  EXPECT_DCHECK_DEATH(SecondDeviceAuthBroker(
      /*device_id=*/std::string(), GetSharedURLLoaderFactory(),
      std::make_unique<attestation::MockAttestationFlow>()))
      << "Using an empty device_id should DCHECK";

  EXPECT_DCHECK_DEATH(SecondDeviceAuthBroker(
      /*device_id=*/std::string(65, '0'), GetSharedURLLoaderFactory(),
      std::make_unique<attestation::MockAttestationFlow>()))
      << "Using a device_id of length greater than 64 should DCHECK";
}

TEST_F(SecondDeviceAuthBrokerTest,
       GetChallengeBytesReturnsAnErrorForAuthErrors) {
  SimulateAuthError(kGetChallengeDataUrl);
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_FALSE(response.has_value());
  EXPECT_THAT(response.error(), Property(&GoogleServiceAuthError::state,
                                         Eq(State::SERVICE_ERROR)));
}

TEST_F(SecondDeviceAuthBrokerTest,
       GetChallengeBytesReturnsAnErrorForMalformedResponse) {
  AddFakeResponse(kGetChallengeDataUrl, "");
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_FALSE(response.has_value());
  EXPECT_THAT(response.error(),
              Property(&GoogleServiceAuthError::state,
                       Eq(State::UNEXPECTED_SERVICE_RESPONSE)));

  AddFakeResponse(kGetChallengeDataUrl, "{}");
  response = GetChallengeBytes();
  ASSERT_FALSE(response.has_value());
  EXPECT_THAT(response.error(),
              Property(&GoogleServiceAuthError::state,
                       Eq(State::UNEXPECTED_SERVICE_RESPONSE)));

  AddFakeResponse(kGetChallengeDataUrl, "{\"challengeData\": \"\"}");
  response = GetChallengeBytes();
  ASSERT_FALSE(response.has_value());
  EXPECT_THAT(response.error(),
              Property(&GoogleServiceAuthError::state,
                       Eq(State::UNEXPECTED_SERVICE_RESPONSE)));
}

TEST_F(SecondDeviceAuthBrokerTest,
       GetChallengeBytesReturnsAnErrorForBase64ParsingError) {
  AddFakeResponse(kGetChallengeDataUrl, kInvalidBase64ChallengeDataResponse);
  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_FALSE(response.has_value());
  EXPECT_THAT(response.error(),
              Property(&GoogleServiceAuthError::state,
                       Eq(State::UNEXPECTED_SERVICE_RESPONSE)));
}

TEST_F(SecondDeviceAuthBrokerTest, GetChallengeBytesReturnsChallengeBytes) {
  // Set an interceptor that checks the validity of the incoming request for
  // challenge bytes.
  SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url != GURL(kGetChallengeDataUrl)) {
          return;
        }

        if (!request.request_body || !request.request_body->elements() ||
            request.request_body->elements()->empty()) {
          SimulateBadRequest(kGetChallengeDataUrl);
          return;
        }

        absl::optional<base::Value> request_body =
            base::JSONReader::Read(request.request_body->elements()
                                       ->at(0)
                                       .As<network::DataElementBytes>()
                                       .AsStringPiece());
        if (!request_body || !request_body->is_dict()) {
          SimulateBadRequest(kGetChallengeDataUrl);
          return;
        }

        const base::Value::Dict& request_dict = request_body->GetDict();
        const std::string* target_device_type =
            request_dict.FindString(kTargetDeviceType);
        if (!target_device_type || *target_device_type != kChromeOS) {
          SimulateBadRequest(kGetChallengeDataUrl);
          return;
        }

        AddFakeResponse(kGetChallengeDataUrl, kFakeChallengeDataResponse);
      }));

  base::expected<std::string, GoogleServiceAuthError> response =
      GetChallengeBytes();
  ASSERT_TRUE(response.has_value());
  EXPECT_THAT(response.value(), Property(&std::string::size, Gt(0UL)));
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
  ASSERT_FALSE(response.has_value());
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
  ASSERT_FALSE(response.has_value());
  EXPECT_THAT(
      response.error(),
      Eq(SecondDeviceAuthBroker::AttestationErrorType::kPermanentError));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchAttestationCertificateReturnsACertificate) {
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
      .WillOnce(WithArg<7>(Invoke([](attestation::AttestationFlow::
                                         CertificateCallback callback) -> void {
        std::move(callback).Run(
            /*status=*/ash::attestation::AttestationStatus::ATTESTATION_SUCCESS,
            /*pem_certificate_chain=*/kCertificate);
      })));

  base::expected<std::string, SecondDeviceAuthBroker::AttestationErrorType>
      response = FetchAttestationCertificate(kFidoCredentialId);
  ASSERT_TRUE(response.has_value());
  EXPECT_THAT(response.value(), Eq(kCertificate));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchRefreshTokenReturnsUnknownErrorResponseForUnknownErrors) {
  AddFakeResponse(kStartSessionUrl, std::string(R"(
      {
        "session_status": "UNKNOWN_SESSION_STATUS"
      }
    )"));
  SecondDeviceAuthBroker::RefreshTokenResponse response =
      FetchRefreshToken(/*fido_assertion_info=*/FidoAssertionInfo{},
                        /*certificate=*/kCertificate);
  EXPECT_THAT(response, VariantWith<RefreshTokenUnknownErrorResponse>(_));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchRefreshTokenReturnsRejectionResponseForRequestRejectionsByServer) {
  SimulateAuthError(kStartSessionUrl);
  SecondDeviceAuthBroker::RefreshTokenResponse response =
      FetchRefreshToken(/*fido_assertion_info=*/FidoAssertionInfo{},
                        /*certificate=*/kCertificate);
  EXPECT_THAT(response, VariantWith<RefreshTokenRejectionResponse>(_));
}

TEST_F(
    SecondDeviceAuthBrokerTest,
    FetchRefreshTokenReturnsAdditionalChallengesOnSourceResponseForSourceChallenges) {
  AddFakeResponse(kStartSessionUrl, std::string(R"(
      {
        "session_status": "PENDING",
        "target_session_identifier": "fake-target-session",
        "source_device_fallback_url": "https://example.com",
        "email": "fake-user@example.com"
      }
    )"));

  RefreshTokenAdditionalChallengesOnSourceResponse expected_response;
  expected_response.email = "fake-user@example.com";
  expected_response.fallback_url = "https://example.com";
  expected_response.target_session_identifier = "fake-target-session";
  SecondDeviceAuthBroker::RefreshTokenResponse response =
      FetchRefreshToken(/*fido_assertion_info=*/FidoAssertionInfo{},
                        /*certificate=*/kCertificate);
  EXPECT_THAT(response,
              VariantWith<RefreshTokenAdditionalChallengesOnSourceResponse>(
                  RefreshTokenAdditionalChallengesOnSourceResponseEq(
                      expected_response)));
}

TEST_F(
    SecondDeviceAuthBrokerTest,
    FetchRefreshTokenReturnsAdditionalChallengesOnTargetResponseForTargetChallenges) {
  AddFakeResponse(kStartSessionUrl, std::string(R"(
      {
        "session_status": "CONTINUE_ON_TARGET",
        "target_fallback_url": "https://example.com",
        "email": "fake-user@example.com"
      }
    )"));

  RefreshTokenAdditionalChallengesOnTargetResponse expected_response;
  expected_response.email = "fake-user@example.com";
  expected_response.fallback_url = "https://example.com";
  SecondDeviceAuthBroker::RefreshTokenResponse response =
      FetchRefreshToken(/*fido_assertion_info=*/FidoAssertionInfo{},
                        /*certificate=*/kCertificate);
  EXPECT_THAT(response,
              VariantWith<RefreshTokenAdditionalChallengesOnTargetResponse>(
                  RefreshTokenAdditionalChallengesOnTargetResponseEq(
                      expected_response)));
}

TEST_F(SecondDeviceAuthBrokerTest, FetchRefreshTokenReturnsARefreshToken) {
  AddFakeResponse(kStartSessionUrl, std::string(R"(
      {
        "session_status": "AUTHENTICATED",
        "credential_data": {
            "oauth_token": "fake-auth-code"
        },
        "email": "fake-user@example.com"
      }
    )"));
  SimulateOAuthTokenFetchSuccess();

  RefreshTokenSuccessResponse expected_response;
  expected_response.email = "fake-user@example.com";
  expected_response.refresh_token = "fake-refresh-token";
  SecondDeviceAuthBroker::RefreshTokenResponse response =
      FetchRefreshToken(/*fido_assertion_info=*/FidoAssertionInfo{},
                        /*certificate=*/kCertificate);
  EXPECT_THAT(response, VariantWith<RefreshTokenSuccessResponse>(
                            RefreshTokenSuccessResponseEq(expected_response)));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchRefreshTokenReturnsAnErrorForInvalidAuthorizationCode) {
  AddFakeResponse(kStartSessionUrl, std::string(R"(
      {
        "session_status": "AUTHENTICATED",
        "credential_data": {
            "oauth_token": "fake-auth-code"
        },
        "email": "fake-user@example.com"
      }
    )"));
  SimulateOAuthTokenFetchFailure();

  RefreshTokenRejectionResponse expected_response;
  expected_response.email = "fake-user@example.com";
  expected_response.reason =
      RefreshTokenRejectionResponse::Reason::kInvalidAuthorizationCode;
  SecondDeviceAuthBroker::RefreshTokenResponse response =
      FetchRefreshToken(/*fido_assertion_info=*/FidoAssertionInfo{},
                        /*certificate=*/kCertificate);
  EXPECT_THAT(response,
              VariantWith<RefreshTokenRejectionResponse>(
                  RefreshTokenRejectionResponseEq(expected_response)));
}

}  //  namespace ash::quick_start
