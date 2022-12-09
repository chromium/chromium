// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/second_device_auth_broker.h"

#include <memory>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/values.h"
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

// JSON keys.
constexpr char kChallengeDataKey[] = "challengeData";
constexpr char kChallengeKey[] = "challenge";

const int64_t kGetChallengeDataTimeoutInSeconds = 60;

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
      /*http_method=*/"POST",
      /*content_type=*/"application/json",
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
      << "Received an unexpected callback for challenge bytes";
  // Reset the fetcher. It's existence is used to check for pending requests.
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
  // Fetch auth code. If successful, fetch refresh token.
  NOTIMPLEMENTED();
}

}  //  namespace ash::quick_start
