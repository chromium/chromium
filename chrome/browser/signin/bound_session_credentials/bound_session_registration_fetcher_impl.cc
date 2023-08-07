// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_impl.h"

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_params.pb.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {
constexpr char kSessionIdentifier[] = "session_identifier";
const char kXSSIPrefix[] = ")]}'";

bound_session_credentials::RegistrationParams CreateRegistrationParams(
    const std::string& url,
    const std::string& session_id,
    const std::string& wrapped_key) {
  bound_session_credentials::RegistrationParams params;
  params.set_site(url);
  params.set_session_id(session_id);
  params.set_wrapped_key(wrapped_key);
  return params;
}

std::string GetAlgoName(crypto::SignatureVerifier::SignatureAlgorithm algo) {
  if (algo == crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256) {
    return "ES256";
  }

  if (algo == crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256) {
    return "RS256";
  }

  return std::string();
}
}  // namespace

BoundSessionRegistrationFetcherImpl::BoundSessionRegistrationFetcherImpl(
    BoundSessionRegistrationFetcherParam registration_params,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    unexportable_keys::UnexportableKeyService* key_service)
    : registration_params_(std::move(registration_params)),
      key_service_(key_service),
      url_loader_factory_(std::move(loader_factory)) {}

BoundSessionRegistrationFetcherImpl::~BoundSessionRegistrationFetcherImpl() =
    default;

void BoundSessionRegistrationFetcherImpl::Start(
    RegistrationCompleteCallback callback) {
  callback_ = std::move(callback);
  if (key_service_) {
    key_service_->GenerateSigningKeySlowlyAsync(
        registration_params_.SupportedAlgos(),
        unexportable_keys::BackgroundTaskPriority::kBestEffort,
        base::BindOnce(&BoundSessionRegistrationFetcherImpl::OnKeyCreated,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Early fail of request, object is invalid after this
    std::move(callback_).Run(absl::nullopt);
  }
}

void BoundSessionRegistrationFetcherImpl::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  const network::mojom::URLResponseHead* head = url_loader_->ResponseInfo();
  net::Error net_error = static_cast<net::Error>(url_loader_->NetError());

  absl::optional<int> http_response_code;
  absl::optional<bound_session_credentials::RegistrationParams> return_value =
      absl::nullopt;

  if (head && head->headers) {
    http_response_code = head->headers->response_code();
  }

  bool net_success = (net_error == net::OK ||
                      net_error == net::ERR_HTTP_RESPONSE_CODE_FAILURE) &&
                     http_response_code;

  // Parse JSON response
  if (net_success && network::IsSuccessfulStatus(*http_response_code)) {
    // JSON responses should start with XSSI-protection prefix which will be
    // removed prior to parsing.
    if (!base::StartsWith(*response_body, kXSSIPrefix,
                          base::CompareCase::SENSITIVE)) {
      // Incorrect format of response, XSSI prefix missing.
      std::move(callback_).Run(absl::nullopt);
      return;
    }
    *response_body = response_body->substr(strlen(kXSSIPrefix));
    absl::optional<base::Value::Dict> maybe_root =
        base::JSONReader::ReadDict(*response_body);

    std::string* session_id = nullptr;
    if (maybe_root) {
      // TODO(b/293985274): Also parse credentials field
      session_id = maybe_root->FindString(kSessionIdentifier);
    }
    if (!session_id) {
      // Incorrect registration params.
      std::move(callback_).Run(absl::nullopt);
      return;
    }

    return_value = CreateRegistrationParams(
        net::SchemefulSite(registration_params_.RegistrationEndpoint())
            .Serialize(),
        *session_id, wrapped_key_str_);
  }

  // Finish the request, object is invalid after this
  std::move(callback_).Run(return_value);
}

void BoundSessionRegistrationFetcherImpl::OnKeyCreated(
    unexportable_keys::ServiceErrorOr<unexportable_keys::UnexportableKeyId>
        created_key) {
  if (!created_key.has_value()) {
    std::move(callback_).Run(absl::nullopt);
    return;
  }
  unexportable_keys::UnexportableKeyId key_id = *created_key;

  unexportable_keys::ServiceErrorOr<std::vector<uint8_t>> public_key =
      key_service_->GetSubjectPublicKeyInfo(key_id);
  std::string pkey(reinterpret_cast<const char*>(public_key->data()),
                   public_key->size());
  std::string pkey_base64;
  base::Base64Encode(pkey, &pkey_base64);

  unexportable_keys::ServiceErrorOr<
      crypto::SignatureVerifier::SignatureAlgorithm>
      algo_used = key_service_->GetAlgorithm(key_id);
  std::string algo_string = GetAlgoName(algo_used.value());
  if (algo_string.empty()) {
    std::move(callback_).Run(absl::nullopt);
    return;
  }

  std::vector<uint8_t> wrapped_key = *key_service_->GetWrappedKey(key_id);
  wrapped_key_str_ = std::string(wrapped_key.begin(), wrapped_key.end());

  StartFetchingRegistration(std::move(pkey_base64), std::move(algo_string));
}

void BoundSessionRegistrationFetcherImpl::StartFetchingRegistration(
    std::string public_key,
    std::string algo_used) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("device_bound_session_register",
                                          R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request is used to rotate bound Google authentication "
            "cookies."
          trigger:
            "This request is triggered in a bound session when the bound Google"
            " authentication cookies are soon to expire."
          user_data {
            type: ACCESS_TOKEN
          }
          data: "Request includes cookies and a signed token proving that a"
                " request comes from the same device as was registered before."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
                email: "chrome-signin-team@google.com"
            }
          }
          last_reviewed: "2023-06-15"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
             "This is a new feature being developed behind a flag that is"
             " disabled by default (kEnableBoundSessionCredentials). This"
             " request will only be sent if the feature is enabled and once"
             " a server requests it with a special header."
          policy_exception_justification:
            "Not implemented. "
            "If the feature is on, this request must be made to ensure the user"
            " maintains their signed in status on the web for Google owned"
            " domains."
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = registration_params_.RegistrationEndpoint();
  request->method = "POST";
  request->site_for_cookies =
      net::SiteForCookies::FromUrl(registration_params_.RegistrationEndpoint());
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(
          url::Origin::Create(registration_params_.RegistrationEndpoint()));

  std::string body = *base::WriteJson(
      base::Value::Dict()
          .Set("binding_alg", std::move(algo_used))  // Algorithm
          .Set("key",
               std::move(public_key))  // Public key
          .Set("client_constraints",
               base::Value::Dict().Set("signature_quota_per_minute", 1)));
  std::string content_type = "application/json";

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->AttachStringForUpload(body, content_type);
  url_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&BoundSessionRegistrationFetcherImpl::OnURLLoaderComplete,
                     base::Unretained(this)),
      10 * 1024);
}
