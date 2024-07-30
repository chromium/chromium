// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_writer.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/attestation/attestation_features.h"
#include "chromeos/ash/components/attestation/attestation_flow.h"
#include "chromeos/ash/components/dbus/attestation/keystore.pb.h"
#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "components/account_id/account_id.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/version_info/channel.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
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
constexpr char kSessionStatusKey[] = "sessionStatus";
constexpr char kRejectionReasonKey[] = "rejectionReason";
constexpr char kTargetFallbackUrlKey[] = "targetFallbackUrl";
constexpr char kSourceDeviceFallbackUrlKey[] = "sourceDeviceFallbackUrl";
constexpr char kEmailKey[] = "email";
constexpr char kObfuscatedGaiaIdKey[] = "obfuscatedGaiaId";
constexpr char kTargetSessionIdentifierKey[] = "targetSessionIdentifier";
constexpr char kCredentialIdKey[] = "credentialId";
constexpr char kAuthenticatorDataKey[] = "authenticatorData";
constexpr char kClientDataKey[] = "clientData";
constexpr char kSignatureKey[] = "signature";
constexpr char kFulfilledChallengeTypeKey[] = "fulfilledChallengeType";
constexpr char kAssertionInfoKey[] = "assertionInfo";
constexpr char kFallbackOptionKey[] = "fallbackOption";
constexpr char kDeviceTypeKey[] = "deviceType";
constexpr char kDeviceAttestationCertificateKey[] =
    "deviceAttestationCertificate";
constexpr char kClientIdKey[] = "clientId";
constexpr char kChromeOsDeviceInfoKey[] = "chromeOsDeviceInfo";
constexpr char kFulfilledChallengeKey[] = "fulfilledChallenge";
constexpr char kPlatformDataKey[] = "platformData";
constexpr char kSourceDeviceInfoKey[] = "sourceDeviceInfo";
constexpr char kTargetDeviceInfoKey[] = "targetDeviceInfo";
constexpr char kCredentialDataKey[] = "credentialData";
constexpr char kOauthTokenKey[] = "oauthToken";

constexpr base::TimeDelta kGetChallengeDataTimeout = base::Minutes(3);
constexpr base::TimeDelta kStartSessionTimeout = base::Minutes(3);
constexpr char kHttpMethod[] = "POST";
constexpr char kHttpContentType[] = "application/json";

constexpr char kGetChallengeDataRequest[] = R"({
      "targetDeviceType": "CHROME_OS"
    })";

constexpr auto kRejectionReasonErrorMap = base::MakeFixedFlatMap<
    std::string_view,
    SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason>({
    {"invalid_oauth_token", SecondDeviceAuthBroker::AuthCodeRejectionResponse::
                                Reason::kInvalidOAuthToken},
    {"account_not_supported",
     SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason::
         kAccountNotSupported},
    {"less_secure_device", SecondDeviceAuthBroker::AuthCodeRejectionResponse::
                               Reason::kLessSecureDevice},
    {"already_authenticated",
     SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason::
         kAlreadyAuthenticated},
    {"session_expired", SecondDeviceAuthBroker::AuthCodeRejectionResponse::
                            Reason::kSessionExpired},
    {"challenge_expired", SecondDeviceAuthBroker::AuthCodeRejectionResponse::
                              Reason::kChallengeExpired},
    {"credential_id_mismatch",
     SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason::
         kCredentialIdMismatch},
    {"account_not_supported_federated_dasher",
     SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason::
         kFederatedEnterpriseAccountNotSupported},
    {"account_not_supported_kid",
     SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason::
         kUnicornAccountNotEnabled},
    {"account_lookup_account_not_found",
     SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason::
         kAccountNotFound},
    {"account_lookup_captcha_required",
     SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason::
         kCaptchaRequired},
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
            "A JSON dict that identifies the device type as ChromeOS. "
            "Authentication to this API is done through Chrome's API key"
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

// Extracts challenge bytes from the parsed JSON `response` from Gaia and
// returns a Base64Url representation. Produces an empty string in case of a
// parsing error. This is how the the response JSON is supposed to look like:
// {
//   "challengeData": {
//     "challenge": "<Base64 encoded challenge bytes>"
//   }
// }
Base64UrlString GetChallengeBytesFromParsedResponse(
    data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    return Base64UrlString();
  }

  base::Value::Dict* challenge_dict =
      response->GetDict().FindDict(kChallengeDataKey);
  if (!challenge_dict) {
    return Base64UrlString();
  }

  std::string* challenge_base64 = challenge_dict->FindString(kChallengeKey);
  if (!challenge_base64) {
    return Base64UrlString();
  }

  // We need to convert the Base64 encoded challenge bytes from Gaia to
  // Base64Url encoded challenge bytes to send to Android. Android doesn't
  // handle the standard Base64 encoding.
  std::optional<Base64UrlString> challenge =
      Base64UrlTranscode(Base64String(*challenge_base64));

  return challenge ? *challenge : Base64UrlString();
}

void HandleAttestationNotAvailableError(
    QuickStartMetrics& metrics,
    SecondDeviceAuthBroker::AttestationCertificateCallback callback) {
  metrics.RecordAttestationCertificateRequestEnded(
      QuickStartMetrics::AttestationCertificateRequestErrorCode::
          kAttestationNotSupportedOnDevice);
  std::move(callback).Run(base::unexpected(
      SecondDeviceAuthBroker::AttestationErrorType::kPermanentError));
}

void HandleAttestationUnknownError(
    QuickStartMetrics& metrics,
    SecondDeviceAuthBroker::AttestationCertificateCallback callback,
    const SecondDeviceAuthBroker::AttestationErrorType& error_type) {
  metrics.RecordAttestationCertificateRequestEnded(
      QuickStartMetrics::AttestationCertificateRequestErrorCode::kUnknownError);
  std::move(callback).Run(base::unexpected(error_type));
}

void HandleGaiaAuthenticationUnknownError(
    QuickStartMetrics& metrics,
    SecondDeviceAuthBroker::AuthCodeCallback callback,
    const SecondDeviceAuthBroker::AuthCodeUnknownErrorResponse& response) {
  metrics.RecordGaiaAuthenticationRequestEnded(
      QuickStartMetrics::GaiaAuthenticationResult::kUnknownError);
  std::move(callback).Run(response);
}

void HandleGaiaAuthenticationParsingError(
    QuickStartMetrics& metrics,
    SecondDeviceAuthBroker::AuthCodeCallback callback,
    const SecondDeviceAuthBroker::AuthCodeParsingErrorResponse& response) {
  metrics.RecordGaiaAuthenticationRequestEnded(
      QuickStartMetrics::GaiaAuthenticationResult::kResponseParsingError);
  std::move(callback).Run(response);
}

void HandleGaiaAuthenticationRejectionError(
    QuickStartMetrics& metrics,
    SecondDeviceAuthBroker::AuthCodeCallback callback,
    const SecondDeviceAuthBroker::AuthCodeRejectionResponse& response) {
  metrics.RecordGaiaAuthenticationRequestEnded(
      QuickStartMetrics::GaiaAuthenticationResult::kRejection);
  std::move(callback).Run(response);
}

std::string CreateStartSessionRequestData(
    const FidoAssertionInfo& fido_assertion_info,
    const PEMCertChain& certificate) {
  std::string request_string;

  // This is the request format:
  // {
  //     "fulfilledChallenge": {
  //         "fulfilledChallengeType": "FIDO",
  //         "assertionInfo": {
  //             "email": <Email as string>,
  //             "credentialId": <Base64 encoded credential id as string>,
  //             "authenticatorData": <Byte array of authenticator data>,
  //             "clientData": <Byte array of client data>,
  //             "signature": <Byte array of signature generated by the
  //                           authenticator>
  //         }
  //     },
  //     "platformData": {
  //         "fallbackOption": "TARGET_ONLY"
  //     },
  //     "sourceDeviceInfo": {
  //         "deviceType": "ANDROID"
  //     },
  //     "targetDeviceInfo": {
  //         "chromeOsDeviceInfo": {
  //             "deviceAttestationCertificate": <Byte array of cert chain>,
  //             "clientId": <Chrome's OAuth client id as string>,
  //         },
  //         "deviceType": "CHROME_OS",
  //     }
  // }

  base::Value::Dict assertion_info;
  assertion_info.Set(kEmailKey, fido_assertion_info.email);
  assertion_info.Set(kCredentialIdKey, *fido_assertion_info.credential_id);
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

  base::Value::Dict chrome_os_device_info;
  // Gaia expects a byte array of cert chain in their request proto (see request
  // format above). We need to Base64 encode the cert chain on top of the PEM
  // encoding. Gaia will then do a double decoding - one at the proto level
  // (Base64), and one at the PEM level (Base64) - to get the actual certificate
  // bytes.
  chrome_os_device_info.Set(
      kDeviceAttestationCertificateKey,
      base::Base64Encode(base::as_bytes(base::make_span((*certificate)))));
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

void RunAuthCodeCallbackWithRejectionResponse(
    QuickStartMetrics& metrics,
    SecondDeviceAuthBroker::AuthCodeCallback auth_code_callback,
    base::Value::Dict* response) {
  SecondDeviceAuthBroker::AuthCodeRejectionResponse rejection_response;

  std::string* email_ptr = response->FindString(kEmailKey);
  // Note that email may be empty.
  rejection_response.email = email_ptr ? *email_ptr : std::string();
  rejection_response.reason =
      SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason::kUnknownReason;
  std::string* rejection_reason = response->FindString(kRejectionReasonKey);
  if (!rejection_reason) {
    QS_LOG(ERROR)
        << "Could not fetch OAuth authorization code. Request rejected "
           "without providing a reason";
    HandleGaiaAuthenticationRejectionError(
        metrics, std::move(auth_code_callback), rejection_response);
    return;
  }

  std::string rejection_reason_lowercase =
      base::ToLowerASCII(*rejection_reason);
  if (!kRejectionReasonErrorMap.contains(rejection_reason_lowercase)) {
    QS_LOG(ERROR)
        << "Could not fetch OAuth authorization code. Request rejected "
           "with unknown reason - "
        << rejection_reason_lowercase;
    HandleGaiaAuthenticationRejectionError(
        metrics, std::move(auth_code_callback), rejection_response);
    return;
  }
  rejection_response.reason =
      kRejectionReasonErrorMap.at(rejection_reason_lowercase);
  HandleGaiaAuthenticationRejectionError(metrics, std::move(auth_code_callback),
                                         rejection_response);
}

void RunAuthCodeCallbackWithAdditionalChallengesOnTargetResponse(
    QuickStartMetrics& metrics,
    SecondDeviceAuthBroker::AuthCodeCallback auth_code_callback,
    base::Value::Dict* response) {
  SecondDeviceAuthBroker::AuthCodeAdditionalChallengesOnTargetResponse
      additional_challenges_response;

  // Note that email may be empty.
  additional_challenges_response.email = *response->FindString(kEmailKey);
  std::string* target_fallback_url =
      response->FindString(kTargetFallbackUrlKey);
  if (!target_fallback_url) {
    QS_LOG(ERROR)
        << "Could not fetch OAuth authorization code. Request required "
           "additional target challenges on unknown URL";
    HandleGaiaAuthenticationParsingError(
        metrics, std::move(auth_code_callback),
        SecondDeviceAuthBroker::AuthCodeParsingErrorResponse());
    return;
  }
  additional_challenges_response.fallback_url = *target_fallback_url;

  metrics.RecordGaiaAuthenticationRequestEnded(
      QuickStartMetrics::GaiaAuthenticationResult::
          kAdditionalChallengesOnTarget);
  std::move(auth_code_callback).Run(additional_challenges_response);
}

void RunAuthCodeCallbackWithAdditionalChallengesOnSourceResponse(
    QuickStartMetrics& metrics,
    SecondDeviceAuthBroker::AuthCodeCallback auth_code_callback,
    base::Value::Dict* response) {
  SecondDeviceAuthBroker::AuthCodeAdditionalChallengesOnSourceResponse
      additional_challenges_response;

  // Note that email may be empty.
  additional_challenges_response.email = *response->FindString(kEmailKey);
  // May be empty.
  additional_challenges_response.target_session_identifier =
      *response->FindString(kTargetSessionIdentifierKey);
  std::string* source_device_fallback_url =
      response->FindString(kSourceDeviceFallbackUrlKey);
  if (!source_device_fallback_url) {
    QS_LOG(ERROR)
        << "Could not fetch OAuth authorization code. Request required "
           "additional source challenges on unknown URL";
    HandleGaiaAuthenticationParsingError(
        metrics, std::move(auth_code_callback),
        SecondDeviceAuthBroker::AuthCodeParsingErrorResponse());
    return;
  }
  additional_challenges_response.fallback_url = *source_device_fallback_url;

  metrics.RecordGaiaAuthenticationRequestEnded(
      QuickStartMetrics::GaiaAuthenticationResult::
          kAdditionalChallengesOnSource);
  std::move(auth_code_callback).Run(additional_challenges_response);
}

// Runs `auth_code_callback` with the `auth_code` as success response.
void RunAuthCodeCallback(
    QuickStartMetrics& metrics,
    SecondDeviceAuthBroker::AuthCodeCallback auth_code_callback,
    const std::string& email,
    const std::string& auth_code,
    const std::string& gaia_id) {
  metrics.RecordGaiaAuthenticationRequestEnded(
      QuickStartMetrics::GaiaAuthenticationResult::kSuccess);
  SecondDeviceAuthBroker::AuthCodeSuccessResponse response;
  response.email = email;
  response.auth_code = auth_code;
  response.gaia_id = gaia_id;
  std::move(auth_code_callback).Run(response);
}

void ParseAuthCodeAndRunCallback(
    QuickStartMetrics& metrics,
    SecondDeviceAuthBroker::AuthCodeCallback auth_code_callback,
    base::Value::Dict* response) {
  base::Value::Dict* credential_data = response->FindDict(kCredentialDataKey);
  if (!credential_data) {
    QS_LOG(ERROR) << "Could not fetch OAuth auth code. Could not find "
                     "credential_data";
    HandleGaiaAuthenticationParsingError(
        metrics, std::move(auth_code_callback),
        SecondDeviceAuthBroker::AuthCodeParsingErrorResponse());
    return;
  }

  std::string* auth_code = credential_data->FindString(kOauthTokenKey);
  if (!auth_code) {
    QS_LOG(ERROR)
        << "Could not fetch OAuth auth code. Could not find oauth_token";
    HandleGaiaAuthenticationParsingError(
        metrics, std::move(auth_code_callback),
        SecondDeviceAuthBroker::AuthCodeParsingErrorResponse());
    return;
  }

  std::string* gaia_id_ptr = response->FindString(kObfuscatedGaiaIdKey);
  // Gaia id may be empty. We need to handle this gracefully.
  std::string gaia_id = gaia_id_ptr ? *gaia_id_ptr : std::string();

  RunAuthCodeCallback(metrics, std::move(auth_code_callback),
                      /*email=*/*response->FindString(kEmailKey), *auth_code,
                      gaia_id);
}

}  // namespace

SecondDeviceAuthBroker::SecondDeviceAuthBroker(
    const std::string& device_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<attestation::AttestationFlow> attestation_flow)
    : device_id_(device_id),
      url_loader_factory_(std::move(url_loader_factory)),
      attestation_(std::move(attestation_flow)),
      weak_ptr_factory_(this) {
  DCHECK(url_loader_factory_);
  DCHECK(attestation_);

  // `device_id_` is used as the Common Name (CN) of Remote Attestation
  // certificates and hence, must be between 0 (exclusive) and 64 (inclusive)
  // characters. The current device ids satisfy this requirement. If this
  // changes in the future, use a hashing algorithm that can fit the longer
  // device id into 64 characters.
  DCHECK(!device_id_.empty());
  DCHECK_LE(device_id_.size(), 64UL);
}

SecondDeviceAuthBroker::~SecondDeviceAuthBroker() = default;

void SecondDeviceAuthBroker::FetchChallengeBytes(
    ChallengeBytesCallback challenge_callback) {
  DCHECK(!endpoint_fetcher_)
      << "This class can handle only one request at a time";

  endpoint_fetcher_ = std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*url=*/GURL(kDeviceSigninBaseUrl).Resolve(kGetChallengeDataApi),
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*timeout=*/kGetChallengeDataTimeout,
      /*post_data=*/kGetChallengeDataRequest,
      /*headers=*/std::vector<std::string>(),
      /*cors_exempt_headers=*/std::vector<std::string>(),
      /*annotation_tag=*/kChallengeDataAnnotation, chrome::GetChannel());

  metrics_.RecordChallengeBytesRequested();
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
    HandleFetchChallengeBytesErrorResponse(std::move(challenge_callback),
                                           std::move(response));
    return;
  }

  data_decoder::DataDecoder::ParseJsonIsolated(
      response->response,
      base::BindOnce(&GetChallengeBytesFromParsedResponse)
          .Then(base::BindOnce(
              &SecondDeviceAuthBroker::RunChallengeBytesCallback,
              weak_ptr_factory_.GetWeakPtr(), std::move(challenge_callback))));
}

void SecondDeviceAuthBroker::FetchAttestationCertificate(
    const Base64UrlString& fido_credential_id,
    AttestationCertificateCallback certificate_callback) {
  metrics_.RecordAttestationCertificateRequested();
  attestation::AttestationFeatures::GetFeatures(base::BindOnce(
      &SecondDeviceAuthBroker::FetchAttestationCertificateInternal,
      weak_ptr_factory_.GetWeakPtr(), fido_credential_id,
      std::move(certificate_callback)));
}

void SecondDeviceAuthBroker::FetchAuthCode(
    const FidoAssertionInfo& fido_assertion_info,
    const PEMCertChain& certificate,
    AuthCodeCallback auth_code_callback) {
  DCHECK(!endpoint_fetcher_)
      << "This class can handle only one request at a time";

  endpoint_fetcher_ = std::make_unique<EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_,
      /*url=*/GURL(kDeviceSigninBaseUrl).Resolve(kStartSessionApi),
      /*http_method=*/kHttpMethod,
      /*content_type=*/kHttpContentType,
      /*timeout=*/kStartSessionTimeout,
      /*post_data=*/
      CreateStartSessionRequestData(fido_assertion_info, certificate),
      /*headers=*/std::vector<std::string>(),
      /*cors_exempt_headers=*/std::vector<std::string>(),
      /*annotation_tag=*/kStartSessionAnnotation, chrome::GetChannel());

  metrics_.RecordGaiaAuthenticationStarted();
  endpoint_fetcher_->PerformRequest(
      base::BindOnce(&SecondDeviceAuthBroker::OnAuthorizationCodeFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(auth_code_callback)),
      google_apis::GetAPIKey().c_str());
}

void SecondDeviceAuthBroker::OnAuthorizationCodeFetched(
    AuthCodeCallback auth_code_callback,
    std::unique_ptr<EndpointResponse> response) {
  DCHECK(endpoint_fetcher_)
      << "Received a callback for authorization code without a pending request";
  // Reset the fetcher. Its existence is used to check for pending requests.
  endpoint_fetcher_.reset();

  if (response->http_status_code != google_apis::ApiErrorCode::HTTP_SUCCESS) {
    QS_LOG(ERROR)
        << "Could not fetch OAuth authorization code. HTTP status code: "
        << response->http_status_code;
  }

  // Creating a copy here because we are going to move `response` soon.
  std::string unparsed_response = response->response;
  data_decoder::DataDecoder::ParseJsonIsolated(
      unparsed_response,
      base::BindOnce(
          &SecondDeviceAuthBroker::RunAuthCodeCallbackFromParsedResponse,
          weak_ptr_factory_.GetWeakPtr(), std::move(auth_code_callback),
          std::move(response)));
}

void SecondDeviceAuthBroker::FetchAttestationCertificateInternal(
    const Base64UrlString& fido_credential_id,
    AttestationCertificateCallback certificate_callback,
    const attestation::AttestationFeatures* attestation_features) {
  if (!attestation_features) {
    QS_LOG(ERROR) << "Failed to get AttestationFeatures";
    HandleAttestationUnknownError(
        metrics_, std::move(certificate_callback),
        SecondDeviceAuthBroker::AttestationErrorType::kPermanentError);
    return;
  }

  if (!attestation_features->IsAttestationAvailable()) {
    QS_LOG(ERROR) << "Attestation is not available";
    HandleAttestationNotAvailableError(metrics_,
                                       std::move(certificate_callback));
    return;
  }

  if (!attestation_features->IsEccSupported() &&
      !attestation_features->IsRsaSupported()) {
    QS_LOG(ERROR) << "Could not find any supported attestation key type";
    HandleAttestationNotAvailableError(metrics_,
                                       std::move(certificate_callback));
    return;
  }

  const ::attestation::KeyType attestation_key_type =
      attestation_features->IsEccSupported() ? ::attestation::KEY_TYPE_ECC
                                             : ::attestation::KEY_TYPE_RSA;
  ::attestation::DeviceSetupCertificateRequestMetadata profile_specific_data;
  profile_specific_data.set_id(device_id_);
  profile_specific_data.set_content_binding(*fido_credential_id);
  attestation_->GetCertificate(
      /*certificate_profile=*/attestation::AttestationCertificateProfile::
          PROFILE_DEVICE_SETUP_CERTIFICATE,
      /*account_id=*/EmptyAccountId(), /*request_origin=*/std::string(),
      /*force_new_key=*/true, /*key_crypto_type=*/attestation_key_type,
      /*key_name=*/attestation::kDeviceSetupKey,
      /*profile_specific_data=*/
      std::make_optional(attestation::AttestationFlow::CertProfileSpecificData(
          profile_specific_data)),
      /*callback=*/
      base::BindOnce(&SecondDeviceAuthBroker::RunAttestationCertificateCallback,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(certificate_callback)));
}

void SecondDeviceAuthBroker::RunAttestationCertificateCallback(
    SecondDeviceAuthBroker::AttestationCertificateCallback callback,
    attestation::AttestationStatus status,
    const std::string& pem_certificate_chain) {
  switch (status) {
    case attestation::ATTESTATION_SUCCESS:
      if (pem_certificate_chain.empty()) {
        QS_LOG(ERROR)
            << "Got an empty certificate chain with a success response "
               "from attestation server";
        HandleAttestationUnknownError(
            metrics_, std::move(callback),
            SecondDeviceAuthBroker::AttestationErrorType::kPermanentError);
        return;
      }
      metrics_.RecordAttestationCertificateRequestEnded(
          /*error_code=*/std::nullopt);
      std::move(callback).Run(PEMCertChain(pem_certificate_chain));
      return;
    case attestation::ATTESTATION_UNSPECIFIED_FAILURE:
      HandleAttestationUnknownError(
          metrics_, std::move(callback),
          SecondDeviceAuthBroker::AttestationErrorType::kTransientError);
      return;
    case attestation::ATTESTATION_SERVER_BAD_REQUEST_FAILURE:
      metrics_.RecordAttestationCertificateRequestEnded(
          QuickStartMetrics::AttestationCertificateRequestErrorCode::
              kBadRequest);
      std::move(callback).Run(base::unexpected(
          SecondDeviceAuthBroker::AttestationErrorType::kPermanentError));
      return;
    case attestation::ATTESTATION_NOT_AVAILABLE:
      HandleAttestationNotAvailableError(metrics_, std::move(callback));
      return;
  }
}

void SecondDeviceAuthBroker::RunAuthCodeCallbackFromParsedResponse(
    SecondDeviceAuthBroker::AuthCodeCallback auth_code_callback,
    std::unique_ptr<EndpointResponse> unparsed_response,
    data_decoder::DataDecoder::ValueOrError response) {
  if (!response.has_value() || !response->is_dict()) {
    // When we can't even parse the response, it most probably is an error from
    // Google's FrontEnd (GFE) - which may not be sending JSON responses. Check
    // if it's an auth error from GFE.
    if (unparsed_response->error_type &&
        unparsed_response->error_type.value() == FetchErrorType::kAuthError) {
      SecondDeviceAuthBroker::AuthCodeRejectionResponse rejection_response;
      rejection_response.reason = SecondDeviceAuthBroker::
          AuthCodeRejectionResponse::Reason::kUnknownReason;
      QS_LOG(ERROR) << "Could not fetch OAuth authorization code. Received an "
                       "auth error from server";
      HandleGaiaAuthenticationRejectionError(
          metrics_, std::move(auth_code_callback), rejection_response);
      return;
    }

    // We could not parse the response and it is not an auth error.
    QS_LOG(ERROR) << "Could not fetch OAuth authorization code. Error parsing "
                     "response from server";
    HandleGaiaAuthenticationParsingError(
        metrics_, std::move(auth_code_callback),
        SecondDeviceAuthBroker::AuthCodeParsingErrorResponse());
    return;
  }

  std::string* session_status =
      response->GetDict().FindString(kSessionStatusKey);
  if (!session_status) {
    QS_LOG(ERROR) << "Could not fetch OAuth authorization code. Error parsing "
                     "session status";
    HandleGaiaAuthenticationParsingError(
        metrics_, std::move(auth_code_callback),
        SecondDeviceAuthBroker::AuthCodeParsingErrorResponse());
    return;
  }

  if (base::ToLowerASCII(*session_status) == "rejected") {
    RunAuthCodeCallbackWithRejectionResponse(
        metrics_, std::move(auth_code_callback), &response->GetDict());
    return;
  } else if (base::ToLowerASCII(*session_status) == "continue_on_target") {
    RunAuthCodeCallbackWithAdditionalChallengesOnTargetResponse(
        metrics_, std::move(auth_code_callback), &response->GetDict());
    return;
  } else if (base::ToLowerASCII(*session_status) == "pending") {
    RunAuthCodeCallbackWithAdditionalChallengesOnSourceResponse(
        metrics_, std::move(auth_code_callback), &response->GetDict());
    return;
  } else if (base::ToLowerASCII(*session_status) == "authenticated") {
    ParseAuthCodeAndRunCallback(metrics_, std::move(auth_code_callback),
                                &response->GetDict());
    return;
  }

  // Unknown session status.
  HandleGaiaAuthenticationUnknownError(
      metrics_, std::move(auth_code_callback),
      SecondDeviceAuthBroker::AuthCodeUnknownErrorResponse());
}

void SecondDeviceAuthBroker::HandleFetchChallengeBytesErrorResponse(
    SecondDeviceAuthBroker::ChallengeBytesCallback challenge_callback,
    std::unique_ptr<EndpointResponse> response) {
  QS_LOG(ERROR) << "Could not fetch challenge bytes. HTTP status code: "
                << response->http_status_code;
  if (!response->error_type.has_value()) {
    RunChallengeBytesCallbackWithError(
        std::move(challenge_callback),
        GoogleServiceAuthError::FromUnexpectedServiceResponse(
            base::StringPrintf("An unknown error occurred. HTTP Status "
                               "of the response is: %d",
                               response->http_status_code)));
    return;
  }

  switch (response->error_type.value()) {
    case FetchErrorType::kAuthError:
      RunChallengeBytesCallbackWithError(
          std::move(challenge_callback),
          GoogleServiceAuthError::FromServiceError(
              base::StringPrintf("An auth error occurred. HTTP status "
                                 "of the response is: %d",
                                 response->http_status_code)));
      return;
    case FetchErrorType::kNetError:
      RunChallengeBytesCallbackWithError(
          std::move(challenge_callback),
          GoogleServiceAuthError::FromUnexpectedServiceResponse(
              base::StringPrintf("A network error occurred. HTTP status "
                                 "of the response is: %d",
                                 response->http_status_code)));
      return;
    case FetchErrorType::kResultParseError:
      RunChallengeBytesCallbackWithError(
          std::move(challenge_callback),
          GoogleServiceAuthError::FromUnexpectedServiceResponse(
              base::StringPrintf("Error parsing response. HTTP status "
                                 "of the response is: %d",
                                 response->http_status_code)));
      return;
  }
}

void SecondDeviceAuthBroker::RunChallengeBytesCallbackWithError(
    SecondDeviceAuthBroker::ChallengeBytesCallback challenge_callback,
    const GoogleServiceAuthError& error) {
  metrics_.RecordChallengeBytesRequestEnded(error);
  std::move(challenge_callback).Run(base::unexpected(error));
}

void SecondDeviceAuthBroker::RunChallengeBytesCallback(
    SecondDeviceAuthBroker::ChallengeBytesCallback challenge_callback,
    const Base64UrlString& challenge) {
  if (challenge->empty()) {
    RunChallengeBytesCallbackWithError(
        std::move(challenge_callback),
        GoogleServiceAuthError::FromUnexpectedServiceResponse(
            "Could not parse response"));
    return;
  }

  metrics_.RecordChallengeBytesRequestEnded(
      GoogleServiceAuthError::AuthErrorNone());
  std::move(challenge_callback).Run(challenge);
}

std::ostream& operator<<(
    std::ostream& stream,
    const SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason& reason) {
  using Reason = SecondDeviceAuthBroker::AuthCodeRejectionResponse::Reason;
  switch (reason) {
    case Reason::kInvalidOAuthToken:
      stream << "[Invalid OAuth Token]";
      break;
    case Reason::kAccountNotSupported:
      stream << "[Account not supported]";
      break;
    case Reason::kAlreadyAuthenticated:
      stream << "[Already authenticated]";
      break;
    case Reason::kChallengeExpired:
      stream << "[Challenge expired]";
      break;
    case Reason::kCredentialIdMismatch:
      stream << "[Credential ID mismatch]";
      break;
    case Reason::kSessionExpired:
      stream << "[Session expired]";
      break;
    case Reason::kLessSecureDevice:
      stream << "[Less secure device]";
      break;
    case Reason::kUnknownReason:
      stream << "[Unknown reason]";
      break;
    default:
      stream << "[Unknown Enum Value]";
  }
  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    const SecondDeviceAuthBroker::AttestationErrorType& attestation_error) {
  switch (attestation_error) {
    case SecondDeviceAuthBroker::AttestationErrorType::kPermanentError:
      stream << "[Permanent error]";
      break;
    case SecondDeviceAuthBroker::AttestationErrorType::kTransientError:
      stream << "[Transient error]";
      break;
  }
  return stream;
}

}  //  namespace ash::quick_start
