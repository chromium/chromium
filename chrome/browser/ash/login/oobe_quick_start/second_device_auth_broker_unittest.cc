// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/attestation/mock_attestation_flow.h"
#include "chromeos/ash/components/attestation/stub_attestation_features.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/quick_start/types.h"
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

using base::test::ErrorIs;
using base::test::ValueIs;
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
using AuthCodeUnknownErrorResponse =
    SecondDeviceAuthBroker::AuthCodeUnknownErrorResponse;
using AuthCodeSuccessResponse = SecondDeviceAuthBroker::AuthCodeSuccessResponse;
using AuthCodeParsingErrorResponse =
    SecondDeviceAuthBroker::AuthCodeParsingErrorResponse;
using AuthCodeRejectionResponse =
    SecondDeviceAuthBroker::AuthCodeRejectionResponse;
using AuthCodeAdditionalChallengesOnSourceResponse =
    SecondDeviceAuthBroker::AuthCodeAdditionalChallengesOnSourceResponse;
using AuthCodeAdditionalChallengesOnTargetResponse =
    SecondDeviceAuthBroker::AuthCodeAdditionalChallengesOnTargetResponse;

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
constexpr char kPemCertificateString[] = R"({
-----BEGIN CERTIFICATE-----
MIICUTCCAfugAwIBAgIBADANBgkqhkiG9w0BAQQFADBXMQswCQYDVQQGEwJDTjEL
MAkGA1UECBMCUE4xCzAJBgNVBAcTAkNOMQswCQYDVQQKEwJPTjELMAkGA1UECxMC
VU4xFDASBgNVBAMTC0hlcm9uZyBZYW5nMB4XDTA1MDcxNTIxMTk0N1oXDTA1MDgx
NDIxMTk0N1owVzELMAkGA1UEBhMCQ04xCzAJBgNVBAgTAlBOMQswCQYDVQQHEwJD
TjELMAkGA1UEChMCT04xCzAJBgNVBAsTAlVOMRQwEgYDVQQDEwtIZXJvbmcgWWFu
ZzBcMA0GCSqGSIb3DQEBAQUAA0sAMEgCQQCp5hnG7ogBhtlynpOS21cBewKE/B7j
V14qeyslnr26xZUsSVko36ZnhiaO/zbMOoRcKK9vEcgMtcLFuQTWDl3RAgMBAAGj
gbEwga4wHQYDVR0OBBYEFFXI70krXeQDxZgbaCQoR4jUDncEMH8GA1UdIwR4MHaA
FFXI70krXeQDxZgbaCQoR4jUDncEoVukWTBXMQswCQYDVQQGEwJDTjELMAkGA1UE
CBMCUE4xCzAJBgNVBAcTAkNOMQswCQYDVQQKEwJPTjELMAkGA1UECxMCVU4xFDAS
BgNVBAMTC0hlcm9uZyBZYW5nggEAMAwGA1UdEwQFMAMBAf8wDQYJKoZIhvcNAQEE
BQADQQA/ugzBrjjK9jcWnDVfGHlk3icNRq0oV7Ri32z/+HQX67aRfgZu7KWdI+Ju
Wm7DCfrPNGVwFWUQOmsPue9rZBgO
-----END CERTIFICATE-----
    })";

constexpr char kFidoCredentialIdBytes[] = "fake-fido-credential-id";
constexpr char kFakeDeviceId[] = "fake-device-id";
constexpr char kTargetDeviceType[] = "targetDeviceType";
constexpr char kTargetDeviceInfoKey[] = "targetDeviceInfo";
constexpr char kChromeOsDeviceInfoKey[] = "chromeOsDeviceInfo";
constexpr char kDeviceAttestationCertificateKey[] =
    "deviceAttestationCertificate";
constexpr char kChromeOS[] = "CHROME_OS";

// Compares the `std::string` `content_binding` proto field to the
// `Base64String` `expected` value.
MATCHER_P(ProtoBufContentBindingEq, expected, "") {
  return arg.content_binding() == *expected;
}

MATCHER_P(AuthCodeAdditionalChallengesOnTargetResponseEq, expected, "") {
  return ExplainMatchResult(
      AllOf(Field("email", &AuthCodeAdditionalChallengesOnTargetResponse::email,
                  Eq(expected.email)),
            Field("fallback_url",
                  &AuthCodeAdditionalChallengesOnTargetResponse::fallback_url,
                  Eq(expected.fallback_url))),
      arg, result_listener);
}

MATCHER_P(AuthCodeAdditionalChallengesOnSourceResponseEq, expected, "") {
  return ExplainMatchResult(
      AllOf(Field("email", &AuthCodeAdditionalChallengesOnSourceResponse::email,
                  Eq(expected.email)),
            Field("fallback_url",
                  &AuthCodeAdditionalChallengesOnSourceResponse::fallback_url,
                  Eq(expected.fallback_url)),
            Field("target_session_identifier",
                  &AuthCodeAdditionalChallengesOnSourceResponse::
                      target_session_identifier,
                  Eq(expected.target_session_identifier))),
      arg, result_listener);
}

MATCHER_P(AuthCodeSuccessResponseEq, expected, "") {
  return ExplainMatchResult(
      AllOf(Field("email", &AuthCodeSuccessResponse::email, Eq(expected.email)),
            Field("auth_code", &AuthCodeSuccessResponse::auth_code,
                  Eq(expected.auth_code))),
      arg, result_listener);
}

MATCHER_P(AuthCodeRejectionResponseEq, expected, "") {
  return ExplainMatchResult(
      AllOf(
          Field("email", &AuthCodeRejectionResponse::email, Eq(expected.email)),
          Field("reason", &AuthCodeRejectionResponse::reason,
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
                                       &mock_attestation_flow_)) {
    attestation_features_.Get()->Clear();
    attestation_features_.Get()->set_is_available(true);
    MakeECCCertificateKeysAvailable();
  }

  SecondDeviceAuthBrokerTest(const SecondDeviceAuthBrokerTest&) = delete;
  SecondDeviceAuthBrokerTest& operator=(const SecondDeviceAuthBrokerTest&) =
      delete;
  ~SecondDeviceAuthBrokerTest() override = default;

 protected:
  base::expected<Base64UrlString, GoogleServiceAuthError>
  FetchChallengeBytes() {
    base::test::TestFuture<
        const base::expected<Base64UrlString, GoogleServiceAuthError>&>
        future;
    second_device_auth_broker_.FetchChallengeBytes(future.GetCallback());
    return future.Get();
  }

  base::expected<PEMCertChain, SecondDeviceAuthBroker::AttestationErrorType>
  FetchAttestationCertificate(const Base64UrlString& fido_credential_id) {
    base::test::TestFuture<
        SecondDeviceAuthBroker::AttestationCertificateOrError>
        future;
    second_device_auth_broker_.FetchAttestationCertificate(
        fido_credential_id, future.GetCallback());
    return future.Get();
  }

  SecondDeviceAuthBroker::AuthCodeResponse FetchAuthCode(
      const FidoAssertionInfo& fido_assertion_info,
      const PEMCertChain& certificate) {
    base::test::TestFuture<const SecondDeviceAuthBroker::AuthCodeResponse&>
        future;
    second_device_auth_broker_.FetchAuthCode(fido_assertion_info, certificate,
                                             future.GetCallback());
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

  void SimulateOAuthTokenFetchFailure() {
    test_factory_.AddResponse(
        GaiaUrls::GetInstance()->oauth2_token_url().spec(), /*content=*/"{}",
        net::HTTP_BAD_REQUEST);
  }

  void MakeAttestationUnavailable() {
    attestation_features_.Get()->set_is_available(false);
  }

  void MakeECCCertificateKeysAvailable() {
    attestation_features_.Get()->set_is_ecc_supported(true);
  }

  void MakeECCCertificateKeysUnavailable() {
    attestation_features_.Get()->set_is_ecc_supported(false);
  }

  void MakeRSACertificateKeysAvailable() {
    attestation_features_.Get()->set_is_rsa_supported(true);
  }

  void MakeRSACertificateKeysUnavailable() {
    attestation_features_.Get()->set_is_rsa_supported(false);
  }

  attestation::MockAttestationFlow& mock_attestation_flow() {
    return mock_attestation_flow_;
  }

  Base64UrlString fido_credential_id() {
    return Base64UrlEncode(kFidoCredentialIdBytes);
  }

  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory() {
    return test_factory_.GetSafeWeakWrapper();
  }

  const PEMCertChain& GetCertificate() const { return certificate_; }

 private:
  // `task_environment_` must be the first member.
  base::test::TaskEnvironment task_environment_;

  PEMCertChain certificate_ = PEMCertChain(kPemCertificateString);

  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_factory_;
  attestation::ScopedStubAttestationFeatures attestation_features_;
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
       FetchChallengeBytesReturnsAnErrorForAuthErrors) {
  SimulateAuthError(kGetChallengeDataUrl);
  EXPECT_THAT(FetchChallengeBytes(),
              ErrorIs(Property(&GoogleServiceAuthError::state,
                               Eq(State::SERVICE_ERROR))));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchChallengeBytesReturnsAnErrorForMalformedResponse) {
  AddFakeResponse(kGetChallengeDataUrl, "");
  EXPECT_THAT(FetchChallengeBytes(),
              ErrorIs(Property(&GoogleServiceAuthError::state,
                               Eq(State::UNEXPECTED_SERVICE_RESPONSE))));

  AddFakeResponse(kGetChallengeDataUrl, "{}");
  EXPECT_THAT(FetchChallengeBytes(),
              ErrorIs(Property(&GoogleServiceAuthError::state,
                               Eq(State::UNEXPECTED_SERVICE_RESPONSE))));

  AddFakeResponse(kGetChallengeDataUrl, "{\"challengeData\": \"\"}");
  EXPECT_THAT(FetchChallengeBytes(),
              ErrorIs(Property(&GoogleServiceAuthError::state,
                               Eq(State::UNEXPECTED_SERVICE_RESPONSE))));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchChallengeBytesReturnsAnErrorForBase64ParsingError) {
  AddFakeResponse(kGetChallengeDataUrl, kInvalidBase64ChallengeDataResponse);
  EXPECT_THAT(FetchChallengeBytes(),
              ErrorIs(Property(&GoogleServiceAuthError::state,
                               Eq(State::UNEXPECTED_SERVICE_RESPONSE))));
}

TEST_F(SecondDeviceAuthBrokerTest, FetchChallengeBytesReturnsChallengeBytes) {
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

  EXPECT_THAT(FetchChallengeBytes(),
              ValueIs(Property(&Base64UrlString::value,
                               Property(&std::string::size, Gt(0UL)))));
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

  EXPECT_THAT(
      FetchAttestationCertificate(fido_credential_id()),
      ErrorIs(
          Eq(SecondDeviceAuthBroker::AttestationErrorType::kTransientError)));
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

  EXPECT_THAT(
      FetchAttestationCertificate(fido_credential_id()),
      ErrorIs(
          Eq(SecondDeviceAuthBroker::AttestationErrorType::kPermanentError)));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchAttestationCertificateReturnsAnErrorIfAttestationIsUnavailable) {
  MakeAttestationUnavailable();

  EXPECT_THAT(
      FetchAttestationCertificate(fido_credential_id()),
      ErrorIs(
          Eq(SecondDeviceAuthBroker::AttestationErrorType::kPermanentError)));
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
                  ProtoBufContentBindingEq(fido_credential_id()))),
          /*callback*/ _))
      .WillOnce(WithArg<7>(Invoke([this](attestation::AttestationFlow::
                                             CertificateCallback callback)
                                      -> void {
        std::move(callback).Run(
            /*status=*/ash::attestation::AttestationStatus::ATTESTATION_SUCCESS,
            /*pem_certificate_chain=*/*GetCertificate());
      })));

  EXPECT_THAT(FetchAttestationCertificate(fido_credential_id()),
              ValueIs(Eq(GetCertificate())));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchAttestationCertificateReturnsAnECCCertificateIfAvailable) {
  // Both ECC and RSA certificate keys are available, but ECC will be preferred.
  MakeECCCertificateKeysAvailable();
  MakeRSACertificateKeysAvailable();

  EXPECT_CALL(
      mock_attestation_flow(),
      GetCertificate(
          /*certificate_profile=*/attestation::AttestationCertificateProfile::
              PROFILE_DEVICE_SETUP_CERTIFICATE,
          /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
          /*force_new_key=*/_, /*key_crypto_type=*/::attestation::KEY_TYPE_ECC,
          /*key_name=*/attestation::kDeviceSetupKey,
          /*profile_specific_data=*/
          Optional(
              VariantWith<::attestation::DeviceSetupCertificateRequestMetadata>(
                  ProtoBufContentBindingEq(fido_credential_id()))),
          /*callback*/ _))
      .WillOnce(WithArg<7>(Invoke([this](attestation::AttestationFlow::
                                             CertificateCallback callback)
                                      -> void {
        std::move(callback).Run(
            /*status=*/ash::attestation::AttestationStatus::ATTESTATION_SUCCESS,
            /*pem_certificate_chain=*/*GetCertificate());
      })));

  EXPECT_THAT(FetchAttestationCertificate(fido_credential_id()),
              ValueIs(Eq(GetCertificate())));
}

TEST_F(
    SecondDeviceAuthBrokerTest,
    FetchAttestationCertificateReturnsAnRSACertificateIfECCKeysAreUnavailable) {
  MakeRSACertificateKeysAvailable();
  MakeECCCertificateKeysUnavailable();

  EXPECT_CALL(
      mock_attestation_flow(),
      GetCertificate(
          /*certificate_profile=*/attestation::AttestationCertificateProfile::
              PROFILE_DEVICE_SETUP_CERTIFICATE,
          /*account_id=*/EmptyAccountId(), /*request_origin=*/"",
          /*force_new_key=*/_, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
          /*key_name=*/attestation::kDeviceSetupKey,
          /*profile_specific_data=*/
          Optional(
              VariantWith<::attestation::DeviceSetupCertificateRequestMetadata>(
                  ProtoBufContentBindingEq(fido_credential_id()))),
          /*callback*/ _))
      .WillOnce(WithArg<7>(Invoke([this](attestation::AttestationFlow::
                                             CertificateCallback callback)
                                      -> void {
        std::move(callback).Run(
            /*status=*/ash::attestation::AttestationStatus::ATTESTATION_SUCCESS,
            /*pem_certificate_chain=*/*GetCertificate());
      })));

  EXPECT_THAT(FetchAttestationCertificate(fido_credential_id()),
              ValueIs(Eq(GetCertificate())));
}

TEST_F(
    SecondDeviceAuthBrokerTest,
    FetchAttestationCertificateReturnsAnErrorIfNoSuitableKeyTypesAreAvailable) {
  MakeRSACertificateKeysUnavailable();
  MakeECCCertificateKeysUnavailable();

  EXPECT_THAT(
      FetchAttestationCertificate(fido_credential_id()),
      ErrorIs(
          Eq(SecondDeviceAuthBroker::AttestationErrorType::kPermanentError)));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchAuthCodeReturnsUnknownErrorResponseForUnknownErrors) {
  AddFakeResponse(kStartSessionUrl, std::string(R"(
      {
        "sessionStatus": "UNKNOWN_SESSION_STATUS"
      }
    )"));
  SecondDeviceAuthBroker::AuthCodeResponse response =
      FetchAuthCode(/*fido_assertion_info=*/FidoAssertionInfo{},
                    /*certificate=*/GetCertificate());
  EXPECT_THAT(response, VariantWith<AuthCodeUnknownErrorResponse>(_));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchAuthCodeReturnsRejectionResponseForRequestRejectionsByServer) {
  SimulateAuthError(kStartSessionUrl);
  SecondDeviceAuthBroker::AuthCodeResponse response =
      FetchAuthCode(/*fido_assertion_info=*/FidoAssertionInfo{},
                    /*certificate=*/GetCertificate());
  EXPECT_THAT(response, VariantWith<AuthCodeRejectionResponse>(_));
}

TEST_F(
    SecondDeviceAuthBrokerTest,
    FetchAuthCodeReturnsAdditionalChallengesOnSourceResponseForSourceChallenges) {
  AddFakeResponse(kStartSessionUrl, std::string(R"(
      {
        "sessionStatus": "PENDING",
        "targetSessionIdentifier": "fake-target-session",
        "sourceDeviceFallbackUrl": "https://example.com",
        "email": "fake-user@example.com"
      }
    )"));

  AuthCodeAdditionalChallengesOnSourceResponse expected_response;
  expected_response.email = "fake-user@example.com";
  expected_response.fallback_url = "https://example.com";
  expected_response.target_session_identifier = "fake-target-session";
  SecondDeviceAuthBroker::AuthCodeResponse response =
      FetchAuthCode(/*fido_assertion_info=*/FidoAssertionInfo{},
                    /*certificate=*/GetCertificate());
  EXPECT_THAT(
      response,
      VariantWith<AuthCodeAdditionalChallengesOnSourceResponse>(
          AuthCodeAdditionalChallengesOnSourceResponseEq(expected_response)));
}

TEST_F(
    SecondDeviceAuthBrokerTest,
    FetchAuthCodeReturnsAdditionalChallengesOnTargetResponseForTargetChallenges) {
  AddFakeResponse(kStartSessionUrl, std::string(R"(
      {
        "sessionStatus": "CONTINUE_ON_TARGET",
        "targetFallbackUrl": "https://example.com",
        "email": "fake-user@example.com"
      }
    )"));

  AuthCodeAdditionalChallengesOnTargetResponse expected_response;
  expected_response.email = "fake-user@example.com";
  expected_response.fallback_url = "https://example.com";
  SecondDeviceAuthBroker::AuthCodeResponse response =
      FetchAuthCode(/*fido_assertion_info=*/FidoAssertionInfo{},
                    /*certificate=*/GetCertificate());
  EXPECT_THAT(
      response,
      VariantWith<AuthCodeAdditionalChallengesOnTargetResponse>(
          AuthCodeAdditionalChallengesOnTargetResponseEq(expected_response)));
}

TEST_F(SecondDeviceAuthBrokerTest, FetchAuthCodeReturnsAnAuthCode) {
  AddFakeResponse(kStartSessionUrl, std::string(R"(
      {
        "sessionStatus": "AUTHENTICATED",
        "credentialData": {
            "oauthToken": "fake-auth-code"
        },
        "email": "fake-user@example.com"
      }
    )"));

  AuthCodeSuccessResponse expected_response;
  expected_response.email = "fake-user@example.com";
  expected_response.auth_code = "fake-auth-code";
  SecondDeviceAuthBroker::AuthCodeResponse response =
      FetchAuthCode(/*fido_assertion_info=*/FidoAssertionInfo{},
                    /*certificate=*/GetCertificate());
  EXPECT_THAT(response, VariantWith<AuthCodeSuccessResponse>(
                            AuthCodeSuccessResponseEq(expected_response)));
}

TEST_F(SecondDeviceAuthBrokerTest,
       FetchAuthCodeSendsABase64EncodedCertChainToGaia) {
  // Set an interceptor that checks the validity of the incoming request for
  // auth code.
  SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        if (request.url != GURL(kStartSessionUrl)) {
          return;
        }

        if (!request.request_body || !request.request_body->elements() ||
            request.request_body->elements()->empty()) {
          SimulateBadRequest(kStartSessionUrl);
          return;
        }

        absl::optional<base::Value> request_body =
            base::JSONReader::Read(request.request_body->elements()
                                       ->at(0)
                                       .As<network::DataElementBytes>()
                                       .AsStringPiece());
        if (!request_body || !request_body->is_dict()) {
          SimulateBadRequest(kStartSessionUrl);
          return;
        }

        const base::Value::Dict& request_dict = request_body->GetDict();
        const base::Value::Dict* target_device_info =
            request_dict.FindDict(kTargetDeviceInfoKey);
        if (!target_device_info) {
          SimulateBadRequest(kStartSessionUrl);
          return;
        }

        const base::Value::Dict* chromeos_device_info =
            target_device_info->FindDict(kChromeOsDeviceInfoKey);
        if (!chromeos_device_info) {
          SimulateBadRequest(kStartSessionUrl);
          return;
        }

        const std::string* device_attestation_certificate =
            chromeos_device_info->FindString(kDeviceAttestationCertificateKey);
        if (!device_attestation_certificate) {
          SimulateBadRequest(kStartSessionUrl);
          return;
        }

        absl::optional<std::vector<uint8_t>> decoded =
            base::Base64Decode(*device_attestation_certificate);
        // The certificate must be Base64 encoded. If not, it is a bad request.
        if (!decoded) {
          SimulateBadRequest(kStartSessionUrl);
          return;
        }

        AddFakeResponse(kStartSessionUrl, std::string(R"(
          {
            "sessionStatus": "AUTHENTICATED",
            "credentialData": {
                "oauthToken": "fake-auth-code"
            },
            "email": "fake-user@example.com"
          }
        )"));
      }));

  SecondDeviceAuthBroker::AuthCodeResponse response =
      FetchAuthCode(/*fido_assertion_info=*/FidoAssertionInfo{},
                    /*certificate=*/GetCertificate());
  EXPECT_THAT(response, VariantWith<AuthCodeSuccessResponse>(_));
}

}  //  namespace ash::quick_start
