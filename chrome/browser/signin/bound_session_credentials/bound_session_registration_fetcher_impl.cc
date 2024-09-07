// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_registration_fetcher_impl.h"

#include <string_view>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/typed_macros.h"
#include "base/values.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params.pb.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_util.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/variations/net/variations_http_headers.h"
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
constexpr char kCredentials[] = "credentials";
constexpr char kRefreshUrl[] = "refresh_url";
const char kXSSIPrefix[] = ")]}'";

bound_session_credentials::Credential CreateCookieCredential(
    const std::string& name,
    const std::string& domain,
    const std::string& path) {
  bound_session_credentials::Credential credential;
  bound_session_credentials::CookieCredential* cookie_credential =
      credential.mutable_cookie_credential();
  cookie_credential->set_name(name);
  cookie_credential->set_domain(domain);
  cookie_credential->set_path(path);
  return credential;
}
}  // namespace

BoundSessionRegistrationFetcherImpl::BoundSessionRegistrationFetcherImpl(
    BoundSessionRegistrationFetcherParam registration_params,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    unexportable_keys::UnexportableKeyService& key_service,
    bool is_off_the_record_profile)
    : registration_params_(std::move(registration_params)),
      key_service_(key_service),
      is_off_the_record_profile_(is_off_the_record_profile),
      url_loader_factory_(std::move(loader_factory)) {}

BoundSessionRegistrationFetcherImpl::~BoundSessionRegistrationFetcherImpl() =
    default;

void BoundSessionRegistrationFetcherImpl::Start(
    RegistrationCompleteCallback callback) {
  TRACE_EVENT("browser", "BoundSessionRegistrationFetcherImpl::Start",
              perfetto::Flow::FromPointer(this), "endpoint",
              registration_params_.registration_endpoint());
  CHECK(!registration_duration_.has_value());
  CHECK(!callback_);
  CHECK(!registration_token_helper_);
  registration_duration_.emplace();  // Starts the timer.
  callback_ = std::move(callback);
  registration_token_helper_ = std::make_unique<RegistrationTokenHelper>(
      key_service_.get(),
      base::ToVector(registration_params_.supported_algos()));
  // base::Unretained() is safe since `this` owns
  // `registration_token_helper_`.
  registration_token_helper_->GenerateForSessionBinding(
      registration_params_.challenge(),
      registration_params_.registration_endpoint(),
      base::BindOnce(
          &BoundSessionRegistrationFetcherImpl::OnRegistrationTokenCreated,
          base::Unretained(this), base::ElapsedTimer()));
}

void BoundSessionRegistrationFetcherImpl::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  const network::mojom::URLResponseHead* head = url_loader_->ResponseInfo();
  net::Error net_error = static_cast<net::Error>(url_loader_->NetError());
  TRACE_EVENT("browser",
              "BoundSessionRegistrationFetcherImpl::OnURLLoaderComplete",
              perfetto::Flow::FromPointer(this), "net_error", net_error);

  std::optional<int> http_response_code;
  if (head && head->headers) {
    http_response_code = head->headers->response_code();
  }

  bool net_success = (net_error == net::OK ||
                      net_error == net::ERR_HTTP_RESPONSE_CODE_FAILURE) &&
                     http_response_code;
  if (!net_success) {
    RunCallbackAndRecordMetrics(
        base::unexpected(RegistrationError::kNetworkError));
    return;
  }

  if (!network::IsSuccessfulStatus(*http_response_code)) {
    RunCallbackAndRecordMetrics(
        base::unexpected(RegistrationError::kServerError));
    return;
  }

  // `response_body` may be nullptr even if there's a valid response code
  // like HTTP_OK, which could happen if there's an interruption before the
  // full response body is received.
  if (!response_body) {
    RunCallbackAndRecordMetrics(
        base::unexpected(RegistrationError::kNetworkError));
    return;
  }

  RegistrationErrorOr<bound_session_credentials::BoundSessionParams>
      params_or_error = ParseJsonResponse(url_loader_->GetFinalURL(),
                                          std::move(response_body));
  if (!params_or_error.has_value()) {
    RunCallbackAndRecordMetrics(params_or_error);
    return;
  }

  bound_session_credentials::BoundSessionParams params =
      std::move(params_or_error).value();
  params.set_site(
      net::SchemefulSite(registration_params_.registration_endpoint())
          .GetURL()
          .spec());
  params.set_wrapped_key(wrapped_key_str_);
  *params.mutable_creation_time() =
      bound_session_credentials::TimeToTimestamp(base::Time::Now());

  if (!bound_session_credentials::AreParamsValid(params)) {
    RunCallbackAndRecordMetrics(
        base::unexpected(RegistrationError::kInvalidSessionParams));
    return;
  }

  // Finish the request, object is invalid after this.
  RunCallbackAndRecordMetrics(std::move(params));
}

void BoundSessionRegistrationFetcherImpl::OnRegistrationTokenCreated(
    base::ElapsedTimer generate_registration_token_timer,
    std::optional<RegistrationTokenHelper::Result> result) {
  TRACE_EVENT("browser",
              "BoundSessionRegistrationFetcherImpl::OnRegistrationTokenCreated",
              perfetto::Flow::FromPointer(this), "success", result.has_value());
  base::UmaHistogramMediumTimes(
      "Signin.BoundSessionCredentials."
      "SessionRegistrationGenerateRegistrationTokenDuration",
      generate_registration_token_timer.Elapsed());
  if (!result.has_value()) {
    RunCallbackAndRecordMetrics(
        base::unexpected(RegistrationError::kGenerateRegistrationTokenFailed));
    return;
  }

  const std::vector<uint8_t>& wrapped_key = result->wrapped_binding_key;
  wrapped_key_str_ = std::string(wrapped_key.begin(), wrapped_key.end());

  StartFetchingRegistration(result->registration_token);
}

void BoundSessionRegistrationFetcherImpl::StartFetchingRegistration(
    const std::string& registration_token) {
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
          last_reviewed: "2024-05-30"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
             "This feature cannot be disabled in settings, but this request "
             "won't be made unless the user signs in to google.com."
          chrome_policy: {
            BoundSessionCredentialsEnabled {
              BoundSessionCredentialsEnabled: false
            }
          }
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = registration_params_.registration_endpoint();
  request->method = "POST";
  request->site_for_cookies = net::SiteForCookies::FromUrl(
      registration_params_.registration_endpoint());
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(
          url::Origin::Create(registration_params_.registration_endpoint()));

  std::string content_type = "application/jwt";

  url_loader_ = CreateSimpleURLLoaderWithVariationsHeaderUnknownSignedIn(
      std::move(request),
      is_off_the_record_profile_ ? variations::InIncognito::kYes
                                 : variations::InIncognito::kNo,
      traffic_annotation);
  url_loader_->AttachStringForUpload(registration_token, content_type);
  url_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&BoundSessionRegistrationFetcherImpl::OnURLLoaderComplete,
                     base::Unretained(this)),
      10 * 1024);
}

void BoundSessionRegistrationFetcherImpl::RunCallbackAndRecordMetrics(
    base::expected<bound_session_credentials::BoundSessionParams,
                   RegistrationError> params_or_error) {
  CHECK(params_or_error.has_value() ||
        params_or_error.error() != RegistrationError::kNone);

  RegistrationError error_for_metrics =
      params_or_error.error_or(RegistrationError::kNone);
  TRACE_EVENT(
      "browser",
      "BoundSessionRegistrationFetcherImpl::RunCallbackAndRecordMetrics",
      perfetto::TerminatingFlow::FromPointer(this), "error", error_for_metrics);
  base::UmaHistogramEnumeration(
      "Signin.BoundSessionCredentials.SessionRegistrationResult",
      error_for_metrics);
  CHECK(registration_duration_.has_value());
  base::UmaHistogramMediumTimes(
      "Signin.BoundSessionCredentials.SessionRegistrationTotalDuration",
      registration_duration_->Elapsed());
  registration_duration_.reset();

  std::move(callback_).Run(
      params_or_error.has_value()
          ? std::move(params_or_error).value()
          : std::optional<bound_session_credentials::BoundSessionParams>());
}

BoundSessionRegistrationFetcherImpl::RegistrationErrorOr<
    bound_session_credentials::BoundSessionParams>
BoundSessionRegistrationFetcherImpl::ParseJsonResponse(
    const GURL& request_url,
    std::unique_ptr<std::string> response_body) {
  // JSON responses normally should start with XSSI-protection prefix which
  // should be removed prior to parsing.
  std::string_view response_json = *response_body;
  if (base::StartsWith(*response_body, kXSSIPrefix,
                       base::CompareCase::SENSITIVE)) {
    response_json = response_json.substr(strlen(kXSSIPrefix));
  }
  std::optional<base::Value::Dict> maybe_root =
      base::JSONReader::ReadDict(response_json);
  if (!maybe_root) {
    return base::unexpected(RegistrationError::kParseJsonFailed);
  }

  std::string* session_id = maybe_root->FindString(kSessionIdentifier);
  base::Value::List* credentials_list = maybe_root->FindList(kCredentials);
  std::string* refresh_url = maybe_root->FindString(kRefreshUrl);
  if (!session_id || !credentials_list || !refresh_url) {
    // Incorrect registration params.
    return base::unexpected(RegistrationError::kRequiredFieldMissing);
  }

  bound_session_credentials::BoundSessionParams params;
  params.set_session_id(*session_id);

  RegistrationErrorOr<std::vector<bound_session_credentials::Credential>>
      credentials_or_error = ParseCredentials(*credentials_list);
  if (!credentials_or_error.has_value()) {
    return base::unexpected(credentials_or_error.error());
  }

  for (auto& credential : credentials_or_error.value()) {
    *params.add_credentials() = std::move(credential);
  }

  // The refresh URL must be a correct, same-site URL.
  GURL refresh_endpoint =
      bound_session_credentials::ResolveEndpointPath(request_url, *refresh_url);
  if (!refresh_endpoint.is_valid()) {
    return base::unexpected(RegistrationError::kInvalidSessionParams);
  }
  params.set_refresh_url(refresh_endpoint.spec());

  return params;
}

BoundSessionRegistrationFetcherImpl::RegistrationErrorOr<
    std::vector<bound_session_credentials::Credential>>
BoundSessionRegistrationFetcherImpl::ParseCredentials(
    const base::Value::List& credentials_list) {
  std::vector<bound_session_credentials::Credential> cookie_credentials;
  for (const auto& credential : credentials_list) {
    const base::Value::Dict* credential_dict = credential.GetIfDict();
    if (!credential_dict) {
      // The parser ignores unknown dictionary entries and so we can do the same
      // for unknown list entries.
      continue;
    }
    const std::string* name = credential_dict->FindString("name");
    const base::Value::Dict* scope = credential_dict->FindDict("scope");
    if (!name || !scope) {
      // Invalid credential.
      return base::unexpected(
          RegistrationError::kRequiredCredentialFieldMissing);
    }
    const std::string* domain = scope->FindString("domain");
    const std::string* path = scope->FindString("path");
    if (!domain || !path) {
      // Invalid credential.
      return base::unexpected(
          RegistrationError::kRequiredCredentialFieldMissing);
    }
    cookie_credentials.emplace_back(
        CreateCookieCredential(*name, *domain, *path));
  }
  return cookie_credentials;
}
