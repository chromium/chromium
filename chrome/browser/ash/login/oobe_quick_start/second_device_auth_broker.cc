// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/guid.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/dbus/attestation/keystore.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "components/account_id/account_id.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/version_info/channel.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash::quick_start {

namespace {

// API endpoints.
constexpr char kDeviceSigninBaseUrl[] =
    "https://devicesignin-pa.googleapis.com";
constexpr char kGetChallengeDataApi[] = "/v1/getchallengedata";
constexpr char kStartSessionApi[] = "/v1/startsession";

// JSON keys.
constexpr char kChallengeDataKey[] = "challengeData";
constexpr char kChallengeKey[] = "challenge";
constexpr char kSessionStatusKey[] = "session_status";
constexpr char kRejectionReasonKey[] = "rejection_reason";
constexpr char kTargetFallbackUrlKey[] = "target_fallback_url";
constexpr char kSourceDeviceFallbackUrlKey[] = "source_device_fallback_url";
constexpr char kEmailKey[] = "email";
constexpr char kTargetSessionIdentifierKey[] = "target_session_identifier";
constexpr char kCredentialIdKey[] = "credential_id";
constexpr char kAuthenticatorDataKey[] = "authenticator_data";
constexpr char kClientDataKey[] = "client_data";
constexpr char kSignatureKey[] = "signature";
constexpr char kFulfilledChallengeTypeKey[] = "fulfilled_challenge_type";
constexpr char kAssertionInfoKey[] = "assertion_info";
constexpr char kFallbackOptionKey[] = "fallback_option";
constexpr char kDeviceTypeKey[] = "device_type";
constexpr char kDeviceAttestationCertificateKey[] =
    "device_attestation_certificate";
constexpr char kClientIdKey[] = "client_id";
constexpr char kChromeOsDeviceInfoKey[] = "chrome_os_device_info";
constexpr char kFulfilledChallengeKey[] = "fulfilled_challenge";
constexpr char kPlatformDataKey[] = "platform_data";
constexpr char kSourceDeviceInfoKey[] = "source_device_info";
constexpr char kTargetDeviceInfoKey[] = "target_device_info";

const int64_t kGetChallengeDataTimeoutInSeconds = 60;
const int64_t kStartSessionTimeoutInSeconds = 60;
constexpr char kHttpMethod[] = "POST";
constexpr char kHttpContentType[] = "application/json";

constexpr auto kRejectionReasonErrorMap = base::MakeFixedFlatMap<
    base::StringPiece,
    SecondDeviceAuthBroker::RefreshTokenRejectionResponse::Reason>({
    {"INVALID_OAUTH_TOKEN",
     SecondDeviceAuthBroker::RefreshTokenRejectionResponse::Reason::
         kInvalidOAuthToken},
    {"ACCOUNT_NOT_SUPPORTED",
     SecondDeviceAuthBroker::RefreshTokenRejectionResponse::Reason::
         kAccountNotSupported},
    {"LESS_SECURE_DEVICE",
     SecondDeviceAuthBroker::RefreshTokenRejectionResponse::Reason::
         kLessSecureDevice},
    {"ALREADY_AUTHENTICATED",
     SecondDeviceAuthBroker::RefreshTokenRejectionResponse::Reason::
         kAlreadyAuthenticated},
    {"SESSION_EXPIRED", SecondDeviceAuthBroker::RefreshTokenRejectionResponse::
                            Reason::kSessionExpired},
    {"CHALLENGE_EXPIRED",
     SecondDeviceAuthBroker::RefreshTokenRejectionResponse::Reason::
         kChallengeExpired},
    {"CREDENTIAL_ID_MISMATCH",
     SecondDeviceAuthBroker::RefreshTokenRejectionResponse::Reason::
         kCredentialIdMismatch},
});

// Network annotations.
constexpr net::NetworkTrafficAnnotationTag kChallengeDataAnnotation =
    net::DefineNetworkTrafficAnnotation("quick_start_challenge_bytes_fetcher",
                                        R"(
        semantics {
          sender: "Chrome OS Start Screen"
          description:
            "Gets nonce challenge bytes from Google's authentication server - "
            "which will be used to generate a FIDO assertion, and a remote "
            "attestation certificate for proving the device's integrity to "
            "Google's authentication server"
          trigger: "When the user starts the Quick Start flow from OOBE"
          data:
            "Nothing. Authentication to this API is done through Chrome's API "
            "key"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "There is no setting to disable the Quick Start flow. This request "
            "is triggered as part of user interaction in OOBE Quick Start - "
            "and is not a background request."
          policy_exception_justification:
            "Not implemented, not considered useful. This request is part of a "
            "flow which is user-initiated, and is not a background request."
        }
      )");
constexpr net::NetworkTrafficAnnotationTag kStartSessionAnnotation =
    net::DefineNetworkTrafficAnnotation("quick_start_session_auth_requester",
                                        R"(
        semantics {
          sender: "Chrome OS Start Screen"
          description:
            "Requests an OAuth authorization code from Google's authentication"
            "server, in exchange for a FIDO assertion and a remote attestation"
            "certificate"
          trigger: "When the user starts the Quick Start flow from OOBE"
          data:
            "Authentication to this API is done through Chrome's API key. "
            "Following information is sent to Google's authentication server -"
            "1. FIDO assertion information - see FidoAssertionInfo struct."
            "2. A remote attestation certificate signed by Google's Privacy CA."
            "3. Chrome's OAuth client ID."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "There is no setting to disable the Quick Start flow. This request "
            "is triggered as part of user interaction in OOBE Quick Start - "
            "and is not a background request."
          policy_exception_justification:
            "Not implemented, not considered useful. This request is part of a "
            "flow which is user-initiated, and is not a background request."
        }
      )");

bool AreChallengeBytesValid(const std::string& challenge_bytes) {
  return base::Base64Decode(challenge_bytes).has_value();
}

// Extracts challenge bytes from the parsed JSON `response` from Gaia. Produces
// an empty string in case of a parsing error. This is how the the response JSON
// is supposed to look like:
// {
//   "challengeData": {
//     "challenge": "<Base64 encoded challenge bytes>"
//   }
// }
std::string GetChallengeBytesFromParsedResponse(
    data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    return std::string();
  }

  base::Value::Dict* challenge_dict =
      response->GetDict().FindDict(kChallengeDataKey);
  if (!challenge_dict) {
    return std::string();
  }

  std::string* challenge_bytes = challenge_dict->FindString(kChallengeKey);
  if (!challenge_bytes || !AreChallengeBytesValid(*challenge_bytes)) {
    return std::string();
  }

  return *challenge_bytes;
}

void RunChallengeBytesCallback(
    SecondDeviceAuthBroker::ChallengeBytesCallback challenge_callback,
    const std::string& challenge_bytes) {
  if (challenge_bytes.empty()) {
    std::move(challenge_callback)
        .Run(base::unexpected(
            GoogleServiceAuthError::FromUnexpectedServiceResponse(
                "Could not parse response")));
    return;
  }

  std::move(challenge_callback).Run(base::ok(challenge_bytes));
}

void HandleGetChallengeBytesErrorResponse(
    SecondDeviceAuthBroker::ChallengeBytesCallback challenge_callback,
    std::unique_ptr<EndpointResponse> response) {
  LOG(ERROR) << "Could not fetch challenge bytes. HTTP status code: "
             << response->http_status_code;
  if (!response->error_type.has_value()) {
    std::move(challenge_callback)
        .Run(base::unexpected(
            GoogleServiceAuthError::FromUnexpectedServiceResponse(
                base::StringPrintf("An unknown error occurred. HTTP Status "
                                   "of the response is: %d",
                                   response->http_status_code))));
    return;
  }

  switch (response->error_type.value()) {
    case FetchErrorType::kAuthError:
      std::move(challenge_callback)
          .Run(base::unexpected(GoogleServiceAuthError::FromServiceError(
              base::StringPrintf("An auth error occurred. HTTP status "
                                 "of the response is: %d",
                                 response->http_status_code))));
      return;
    case FetchErrorType::kNetError:
      std::move(challenge_callback)
          .Run(base::unexpected(
              GoogleServiceAuthError::FromUnexpectedServiceResponse(
                  base::StringPrintf("A network error occurred. HTTP status "
                                     "of the response is: %d",
                                     response->http_status_code))));
      return;
    case FetchErrorType::kResultParseError:
      std::move(challenge_callback)
          .Run(base::unexpected(
              GoogleServiceAuthError::FromUnexpectedServiceResponse(
                  base::StringPrintf("Error parsing response. HTTP status "
                                     "of the response is: %d",
                                     response->http_status_code))));
      return;
  }
}

void RunAttestationCertificateCallback(
    SecondDeviceAuthBroker::AttestationCertificateCallback callback,
    attestation::AttestationStatus status,
    const std::string& pem_certificate_chain) {
  switch (status) {
    case attestation::ATTESTATION_SUCCESS:
      if (pem_certificate_chain.empty()) {
        LOG(ERROR) << "Got an empty certificate chain with a success response "
                      "from attestation server";
        std::move(callback).Run(base::unexpected(
            SecondDeviceAuthBroker::AttestationErrorType::kPermanentError));
        return;
      }
      std::move(callback).Run(base::ok(pem_certificate_chain));
      return;
    case attestation::ATTESTATION_UNSPECIFIED_FAILURE:
      // TODO(b/259021973): Is it safe to consider
      // `ATTESTATION_UNSPECIFIED_FAILURE` transient? Check its side effects.
      std::move(callback).Run(base::unexpected(
          SecondDeviceAuthBroker::AttestationErrorType::kTransientError));
      return;
    case attestation::ATTESTATION_SERVER_BAD_REQUEST_FAILURE:
    case attestation::ATTESTATION_NOT_AVAILABLE:
      std::move(callback).Run(base::unexpected(
          SecondDeviceAuthBroker::AttestationErrorType::kPermanentError));
      return;
  }
}

std::string CreateStartSessionRequestData(
    const FidoAssertionInfo& fido_assertion_info,
    const std::string& certificate) {
  std::string request_string;

  // This is the request format:
  // {
  //     "fulfilled_challenge": {
  //         "fulfilled_challenge_type": "FIDO",
  //         "assertion_info": {
  //             "email": <Email as string>,
  //             "credential_id": <Base64 encoded credential id as string>,
  //             "authenticator_data": <Byte array of authenticator data>,
  //             "client_data": <Byte array of client data>,
  //             "signature": <Byte array of signature generated by the
  //                           authenticator>
  //         }
  //     },
  //     "platform_data": {
  //         "fallback_option": "TARGET_ONLY"
  //     },
  //     "source_device_info": {
  //         "device_type": "ANDROID"
  //     },
  //     "target_device_info": {
  //         "chrome_os_device_info": {
  //             "device_attestation_certificate": <Byte array of cert chain>,
  //             "client_id": <Chrome's OAuth client id as string>,
  //         },
  //         "device_type": "CHROME_OS",
  //     }
  // }

  base::Value::Dict assertion_info;
  assertion_info.Set(kEmailKey, fido_assertion_info.email);
  assertion_info.Set(kCredentialIdKey, fido_assertion_info.credential_id);
  // The following fields are binary data that will be represented as a protobuf
  // `bytes` field on Google's side. Protobuf guarantees a stable translation
  // between byte arrays and Base64 encoded JSON fields.
  assertion_info.Set(
      kAuthenticatorDataKey,
      base::Base64Encode(fido_assertion_info.authenticator_data));
  assertion_info.Set(kClientDataKey,
                     base::Base64Encode(fido_assertion_info.client_data));
  assertion_info.Set(kSignatureKey,
                     base::Base64Encode(fido_assertion_info.signature));

  base::Value::Dict fulfilled_challenge;
  fulfilled_challenge.Set(kFulfilledChallengeTypeKey, "FIDO");
  fulfilled_challenge.Set(kAssertionInfoKey, std::move(assertion_info));

  base::Value::Dict platform_data;
  platform_data.Set(kFallbackOptionKey, "TARGET_ONLY");

  base::Value::Dict source_device_info;
  source_device_info.Set(kDeviceTypeKey, "ANDROID");

  // TODO(b/259021973): Figure out how to send the device model here - after
  // taking user's consent. Also change the network annotation after adding
  // this.
  base::Value::Dict chrome_os_device_info;
  chrome_os_device_info.Set(kDeviceAttestationCertificateKey, certificate);
  chrome_os_device_info.Set(
      kClientIdKey,
      google_apis::GetOAuth2ClientID(google_apis::OAuth2Client::CLIENT_MAIN));

  base::Value::Dict target_device_info;
  target_device_info.Set(kChromeOsDeviceInfoKey,
                         std::move(chrome_os_device_info));
  target_device_info.Set(kDeviceTypeKey, "CHROME_OS");

  base::Value::Dict request;
  request.Set(kFulfilledChallengeKey, std::move(fulfilled_challenge));
  request.Set(kPlatformDataKey, std::move(platform_data));
  request.Set(kSourceDeviceInfoKey, std::move(source_device_info));
  request.Set(kTargetDeviceInfoKey, std::move(target_device_info));

  base::JSONWriter::Write(request, &request_string);

  return request_string;
}

void RunRefreshTokenCallbackWithRejectionResponse(
    SecondDeviceAuthBroker::RefreshTokenCallback refresh_token_callback,
    base::Value::Dict* response) {
  SecondDeviceAuthBroker::RefreshTokenRejectionResponse rejection_response;

  // Note that email may be empty.
  rejection_response.email = *response->FindString(kEmailKey);
  rejection_response.reason = SecondDeviceAuthBroker::
      RefreshTokenRejectionResponse::Reason::kUnknownReason;
  std::string* rejection_reason = response->FindString(kRejectionReasonKey);
  if (!rejection_reason) {
    LOG(ERROR) << "Could not fetch OAuth authorization code. Request rejected "
                  "without providing a reason";
    std::move(refresh_token_callback).Run(rejection_response);
    return;
  }
  if (!kRejectionReasonErrorMap.contains(*rejection_reason)) {
    LOG(ERROR) << "Could not fetch OAuth authorization code. Request rejected "
                  "with unknown reason";
    std::move(refresh_token_callback).Run(rejection_response);
    return;
  }
  rejection_response.reason = kRejectionReasonErrorMap.at(*rejection_reason);
  std::move(refresh_token_callback).Run(rejection_response);
}

void RunRefreshTokenCallbackWithAdditionalChallengesOnTargetResponse(
    SecondDeviceAuthBroker::RefreshTokenCallback refresh_token_callback,
    base::Value::Dict* response) {
  SecondDeviceAuthBroker::RefreshTokenAdditionalChallengesOnTargetResponse
      additional_challenges_response;

  // Note that email may be empty.
  additional_challenges_response.email = *response->FindString(kEmailKey);
  std::string* target_fallback_url =
      response->FindString(kTargetFallbackUrlKey);
  if (!target_fallback_url) {
    LOG(ERROR) << "Could not fetch OAuth authorization code. Request required "
                  "additional target challenges on unknown URL";
    std::move(refresh_token_callback)
        .Run(SecondDeviceAuthBroker::RefreshTokenParsingErrorResponse());
    return;
  }
  additional_challenges_response.fallback_url = *target_fallback_url;

  std::move(refresh_token_callback).Run(additional_challenges_response);
}

void RunRefreshTokenCallbackWithAdditionalChallengesOnSourceResponse(
    SecondDeviceAuthBroker::RefreshTokenCallback refresh_token_callback,
    base::Value::Dict* response) {
  SecondDeviceAuthBroker::RefreshTokenAdditionalChallengesOnSourceResponse
      additional_challenges_response;

  // Note that email may be empty.
  additional_challenges_response.email = *response->FindString(kEmailKey);
  // May be empty.
  additional_challenges_response.target_session_identifier =
      *response->FindString(kTargetSessionIdentifierKey);
  std::string* source_device_fallback_url =
      response->FindString(kSourceDeviceFallbackUrlKey);
  if (!source_device_fallback_url) {
    LOG(ERROR) << "Could not fetch OAuth authorization code. Request required "
                  "additional source challenges on unknown URL";
    std::move(refresh_token_callback)
        .Run(SecondDeviceAuthBroker::RefreshTokenParsingErrorResponse());
    return;
  }
  additional_challenges_response.fallback_url = *source_device_fallback_url;

  std::move(refresh_token_callback).Run(additional_challenges_response);
}

void RunRefreshTokenCallbackFromParsedResponse(
    SecondDeviceAuthBroker::RefreshTokenCallback refresh_token_callback,
    std::unique_ptr<EndpointResponse> unparsed_response,
    data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    // When we can't even parse the response, it most probably is an error from
    // Google's FrontEnd (GFE) - which may not be sending JSON responses. Check
    // if it's an auth error from GFE.
    if (unparsed_response->error_type &&
        unparsed_response->error_type.value() == FetchErrorType::kAuthError) {
      SecondDeviceAuthBroker::RefreshTokenRejectionResponse rejection_response;
      rejection_response.reason = SecondDeviceAuthBroker::
          RefreshTokenRejectionResponse::Reason::kUnknownReason;
      LOG(ERROR) << "Could not fetch OAuth authorization code. Received an "
                    "auth error from server";
      std::move(refresh_token_callback).Run(rejection_response);
      return;
    }

    // We could not parse the response and it is not an auth error.
    LOG(ERROR) << "Could not fetch OAuth authorization code. Error parsing "
                  "response from server";
    std::move(refresh_token_callback)
        .Run(SecondDeviceAuthBroker::RefreshTokenParsingErrorResponse());
    return;
  }

  std::string* session_status =
      response->GetDict().FindString(kSessionStatusKey);
  if (!session_status) {
    LOG(ERROR) << "Could not fetch OAuth authorization code. Error parsing "
                  "session status";
    std::move(refresh_token_callback)
        .Run(SecondDeviceAuthBroker::RefreshTokenParsingErrorResponse());
    return;
  }

  if (*session_status == "REJECTED") {
    RunRefreshTokenCallbackWithRejectionResponse(
        std::move(refresh_token_callback), &response->GetDict());
    return;
  } else if (*session_status == "CONTINUE_ON_TARGET") {
    RunRefreshTokenCallbackWithAdditionalChallengesOnTargetResponse(
        std::move(refresh_token_callback), &response->GetDict());
    return;
  } else if (*session_status == "PENDING") {
    RunRefreshTokenCallbackWithAdditionalChallengesOnSourceResponse(
        std::move(refresh_token_callback), &response->GetDict());
    return;
  }
  // TODO(b/259021973): Handle success response.

  // Unknown session status.
  std::move(refresh_token_callback)
      .Run(SecondDeviceAuthBroker::RefreshTokenUnknownErrorResponse());
}

}  // namespace

SecondDeviceAuthBroker::SecondDeviceAuthBroker(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<attestation::AttestationFlow> attestation_flow)
    : url_loader_factory_(std::move(url_loader_factory)),
      attestation_(std::move(attestation_flow)),
      weak_ptr_factory_(this) {
  DCHECK(url_loader_factory_);
  DCHECK(attestation_);
}

SecondDeviceAuthBroker::~SecondDeviceAuthBroker() = default;

void SecondDeviceAuthBroker::GetChallengeBytes(
    ChallengeBytesCallback challenge_callback) {
  DCHECK(!endpoint_fetcher_)
      << "This class can handle only one request at a time";

  endpoint_fetcher_ = std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*url=*/GURL(kDeviceSigninBaseUrl).Resolve(kGetChallengeDataApi),
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*timeout_ms=*/kGetChallengeDataTimeoutInSeconds * 1000,
      /*post_data=*/std::string(),
      /*headers=*/std::vector<std::string>(),
      /*annotation_tag=*/kChallengeDataAnnotation,
      /*is_stable_channel=*/chrome::GetChannel() ==
          version_info::Channel::STABLE);

  endpoint_fetcher_->PerformRequest(
      base::BindOnce(&SecondDeviceAuthBroker::OnChallengeBytesFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(challenge_callback)),
      google_apis::GetAPIKey().c_str());
}

void SecondDeviceAuthBroker::OnChallengeBytesFetched(
    ChallengeBytesCallback challenge_callback,
    std::unique_ptr<EndpointResponse> response) {
  DCHECK(endpoint_fetcher_)
      << "Received a callback for challenge bytes without a pending request";
  // Reset the fetcher. Its existence is used to check for pending requests.
  endpoint_fetcher_.reset();

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    HandleGetChallengeBytesErrorResponse(std::move(challenge_callback),
                                         std::move(response));
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      response->response,
      base::BindOnce(&GetChallengeBytesFromParsedResponse)
          .Then(base::BindOnce(&RunChallengeBytesCallback,
                               std::move(challenge_callback))));
}

void SecondDeviceAuthBroker::FetchAttestationCertificate(
    const std::string& fido_credential_id,
    AttestationCertificateCallback certificate_callback) {
  // TODO(b/259021973): Figure out if we can use ECC keys where they are
  // available.
  ::attestation::DeviceSetupCertificateRequestMetadata profile_specific_data;
  profile_specific_data.set_id(base::GenerateGUID());
  profile_specific_data.set_content_binding(fido_credential_id);
  attestation_->GetCertificate(
      /*certificate_profile=*/attestation::AttestationCertificateProfile::
          PROFILE_DEVICE_SETUP_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/std::string(),
      /*force_new_key=*/true, /*key_crypto_type=*/::attestation::KEY_TYPE_RSA,
      /*key_name=*/attestation::kDeviceSetupKey,
      /*profile_specific_data=*/
      absl::make_optional(attestation::AttestationFlow::CertProfileSpecificData(
          profile_specific_data)),
      /*callback=*/
      base::BindOnce(&RunAttestationCertificateCallback,
                     std::move(certificate_callback)));
}

void SecondDeviceAuthBroker::FetchRefreshToken(
    const FidoAssertionInfo& fido_assertion_info,
    const std::string& certificate,
    RefreshTokenCallback refresh_token_callback) {
  DCHECK(!endpoint_fetcher_)
      << "This class can handle only one request at a time";

  endpoint_fetcher_ = std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*url=*/GURL(kDeviceSigninBaseUrl).Resolve(kStartSessionApi),
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*timeout_ms=*/kStartSessionTimeoutInSeconds * 1000,
      /*post_data=*/
      CreateStartSessionRequestData(fido_assertion_info, certificate),
      /*headers=*/std::vector<std::string>(),
      /*annotation_tag=*/kStartSessionAnnotation,
      /*is_stable_channel=*/chrome::GetChannel() ==
          version_info::Channel::STABLE);

  endpoint_fetcher_->PerformRequest(
      base::BindOnce(&SecondDeviceAuthBroker::OnAuthorizationCodeFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(refresh_token_callback)),
      google_apis::GetAPIKey().c_str());
}

void SecondDeviceAuthBroker::OnAuthorizationCodeFetched(
    RefreshTokenCallback refresh_token_callback,
    std::unique_ptr<EndpointResponse> response) {
  DCHECK(endpoint_fetcher_)
      << "Received a callback for authorization code without a pending request";
  // Reset the fetcher. Its existence is used to check for pending requests.
  endpoint_fetcher_.reset();

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    LOG(ERROR) << "Could not fetch OAuth authorization code. HTTP status code: "
               << response->http_status_code;
  }

  // Creating a copy here because we are going to move `response` soon.
  std::string unparsed_response = response->response;
  data_decoder::DataDecoder::ParseJsonIsolated(
      unparsed_response,
      base::BindOnce(&RunRefreshTokenCallbackFromParsedResponse,
                     std::move(refresh_token_callback), std::move(response)));
}

}  //  namespace ash::quick_start
