// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/projector_app/projector_xhr_sender.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/projector_app/projector_app_client.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom-shared.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Projector network traffic annotation tags.
constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("projector_xhr_loader", R"(
          semantics: {
            sender: "ChromeOS Projector"
            description: "ChromeOS send Projector XHR requests. This is the "
              "network call between the chromeOS Projector(Screencast) app and "
              "the server. If the screencast app is enabled, the XHR requests "
              "are made by the app to the allowlisted URLs after authorizing "
              "the request with API key."
            trigger: "When the user launches the Screencast app to create and "
              "view screencasts."
            data: "The recordings and transcripts done via the Screencast app."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy: {
            cookies_allowed: NO
            setting: "The admin can enable or disable this feature via Google "
              "Admin console. On homepage, go to Devices > Chrome. On the left,"
              "click Settings > Users & browsers. Scroll down to Content and "
              "locate Screencast to enable/diable it. The feature is enabled "
              "by default."
            chrome_policy {
              ProjectorEnabled {
                ProjectorEnabled: true
              }
            }
          })");

// Projector network traffic annotation tags.
constexpr net::NetworkTrafficAnnotationTag
    kNetworkTrafficAnnotationTagAllowCookie =
        net::DefineNetworkTrafficAnnotation("projector_xhr_loader_allow_cookie",
                                            R"(
          semantics: {
            sender: "ChromeOS Projector"
            description: "ChromeOS send Projector XHR requests. This is the "
              "network call between the chromeOS Projector(Screencast) app and "
              "the server. If the screencast app is enabled, the XHR requests "
              "are made by the app to the allowlisted URLs after authorizing "
              "the request with end user credentials or auth token."
            trigger: "When the user launches the Screencast app to create and "
              "view screencasts."
            data: "The recordings and transcripts done via the Screencast app."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy: {
            cookies_allowed: YES
            cookies_store: "user"
            setting: "The admin can enable or disable this feature via Google "
              "Admin console. On homepage, go to Devices > Chrome. On the left,"
              "click Settings > Users & browsers. Scroll down to Content and "
              "locate Screencast to enable/diable it. The feature is enabled "
              "by default."
            chrome_policy {
              ProjectorEnabled {
                ProjectorEnabled: true
              }
            }
          })");

constexpr char kAuthorizationHeaderPrefix[] = "Bearer ";

constexpr char kApiKeyParam[] = "key";

// List of URL prefix supported by `ProjectorXhrSender`.
const char* kUrlAllowlist[] = {
    "https://www.googleapis.com/drive/v3/files/",
    "https://www.googleapis.com/upload/drive/v3/files/",
    // TODO(b/229792620): Remove this URL prefix once web component is updated
    // with the base URL that force using primary account credential.
    "https://drive.google.com/get_video_info",
    "https://drive.google.com/u/0/get_video_info",
    "https://translation.googleapis.com/language/translate/v2"};

// Return true if the url matches the allowed URL prefix.
bool IsUrlAllowlisted(const std::string& url) {
  for (auto* urlPrefix : kUrlAllowlist) {
    if (base::StartsWith(url, urlPrefix, base::CompareCase::SENSITIVE))
      return true;
  }
  return false;
}

inline std::string RequestTypeToString(projector::mojom::RequestType method) {
  switch (method) {
    case projector::mojom::RequestType::kPost:
      return "POST";
    case projector::mojom::RequestType::kGet:
      return "GET";
    case projector::mojom::RequestType::kPatch:
      return "PATCH";
    case projector::mojom::RequestType::kDelete:
      return "DELETE";
  }

  NOTREACHED();
}

// The maximum number of retries for the SimpleURLLoader requests. Three times
// is an arbitrary number to start with.
const int kMaxRetries = 3;

void HandleAccessTokenErrorState(const std::string& email,
                                 const GoogleServiceAuthError& error) {
  LOG(ERROR) << "Failed to request access token, error state:" << error.state()
             << ", error detail:" << error.ToString();
  if (error.state() ==
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS) {
    ProjectorAppClient::Get()->HandleAccountReauth(email);
  }
}

// Convert net error code from loader::NetError to JS style error code to
// match xhr response from PWA.
projector::mojom::JsNetErrorCode GetJsNetErrorCodeFromNetError(
    int net_error_code) {
  switch (net_error_code) {
    case net::OK:
      return projector::mojom::JsNetErrorCode::kNoError;
    case net::ERR_ACCESS_DENIED:
      return projector::mojom::JsNetErrorCode::kAccessDenied;
    case net::ERR_ABORTED:
      return projector::mojom::JsNetErrorCode::kAbort;
    case net::ERR_TIMED_OUT:
      return projector::mojom::JsNetErrorCode::kTimeout;
    default:
      return projector::mojom::JsNetErrorCode::kHttpError;
  }
}

// Create Xhr Response with response body string and response code,
// net error code is optional and need to be set separately if required.
projector::mojom::XhrResponsePtr CreateXhrResposne(
    std::string response_body,
    projector::mojom::XhrResponseCode resposne_code) {
  auto response = projector::mojom::XhrResponse::New();
  response->response = response_body;
  response->response_code = resposne_code;
  return response;
}

}  // namespace

ProjectorXhrSender::ProjectorXhrSender(
    network::mojom::URLLoaderFactory* url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}
ProjectorXhrSender::~ProjectorXhrSender() = default;

void ProjectorXhrSender::Send(
    const GURL& url,
    projector::mojom::RequestType method,
    const std::optional<std::string>& request_body,
    bool use_credentials,
    bool use_api_key,
    SendRequestCallback callback,
    const std::optional<base::flat_map<std::string, std::string>>& headers,
    const std::optional<std::string>& account_email) {
  if (!IsUrlAllowlisted(url.spec())) {
    std::move(callback).Run(CreateXhrResposne(
        std::string(), projector::mojom::XhrResponseCode::kUnsupportedURL));
    LOG(ERROR) << "URL is not supported.";
    return;
  }
  GURL request_url = url;
  if (use_api_key) {
    request_url =
        net::AppendQueryParameter(url, kApiKeyParam, google_apis::GetAPIKey());
    // Use end user credentials or API key to authorize the request. Doesn't
    // need to fetch OAuth token.
    SendRequest(request_url, method, request_body, /*token=*/std::string(),
                headers, /*allow_cookie=*/false, std::move(callback));
    return;
  }

  // Send request with OAuth token.
  // TODO(b/288457397): Currenlty, absent of account email is considered valid
  // email so it will fallback to use primary account email. We want to clean it
  // up so that account email is required.
  if (!IsValidEmail(account_email)) {
    std::move(callback).Run(CreateXhrResposne(
        std::string(),
        projector::mojom::XhrResponseCode::kInvalidAccountEmail));
    LOG(ERROR) << "User email is invalid";
    return;
  }

  std::string email;
  if (account_email.has_value() && !account_email->empty()) {
    email = *account_email;
  } else {
    email = ProjectorAppClient::Get()
                ->GetIdentityManager()
                ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                .email;
  }

  // Fetch OAuth token for authorizing the request.
  oauth_token_fetcher_.GetAccessTokenFor(
      email, base::BindOnce(&ProjectorXhrSender::OnAccessTokenRequestCompleted,
                            weak_factory_.GetWeakPtr(), request_url, method,
                            request_body, headers, use_credentials,
                            std::move(callback)));
}

void ProjectorXhrSender::OnAccessTokenRequestCompleted(
    const GURL& url,
    projector::mojom::RequestType method,
    const std::optional<std::string>& request_body,
    const std::optional<base::flat_map<std::string, std::string>>& headers,
    bool use_credentials,
    SendRequestCallback callback,
    const std::string& email,
    GoogleServiceAuthError error,
    const signin::AccessTokenInfo& info) {
  if (error.state() != GoogleServiceAuthError::State::NONE) {
    std::move(callback).Run(CreateXhrResposne(
        std::string(), projector::mojom::XhrResponseCode::kTokenFetchFailure));
    HandleAccessTokenErrorState(email, error);
    return;
  }

  SendRequest(url, method, request_body, info.token, headers,
              /*allow_cookie=*/use_credentials, std::move(callback));
}

void ProjectorXhrSender::SendRequest(
    const GURL& url,
    projector::mojom::RequestType method,
    const std::optional<std::string>& request_body,
    const std::string& token,
    const std::optional<base::flat_map<std::string, std::string>>& headers,
    bool allow_cookie,
    SendRequestCallback callback) {
  // Build resource request.
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = RequestTypeToString(method);
  // The OAuth token will be empty if the request is using end user credentials
  // for authorization.
  if (!token.empty()) {
    resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                        kAuthorizationHeaderPrefix + token);
  }
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      "application/json");

  if (headers.has_value()) {
    for (auto [key, value] : *headers) {
      resource_request->headers.SetHeader(key, value);
    }
  }

  // Send resource request.
  auto loader = network::SimpleURLLoader::Create(
      std::move(resource_request), allow_cookie
                                       ? kNetworkTrafficAnnotationTagAllowCookie
                                       : kNetworkTrafficAnnotationTag);
  // Return response body of non-2xx response. This allows passing response body
  // for non-2xx response.
  loader->SetAllowHttpErrorResults(true);

  if (request_body.has_value() && !request_body->empty()) {
    loader->AttachStringForUpload(*request_body, "application/json");
  }

  loader->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RETRY_ON_5XX |
          network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_,
      base::BindOnce(&ProjectorXhrSender::OnSimpleURLLoaderComplete,
                     weak_factory_.GetWeakPtr(), next_request_id_,
                     std::move(callback), token));

  loader_map_.emplace(next_request_id_++, std::move(loader));
}

void ProjectorXhrSender::OnSimpleURLLoaderComplete(
    int request_id,
    SendRequestCallback callback,
    const std::string& token,
    std::unique_ptr<std::string> response_body) {
  auto& loader = loader_map_[request_id];

  auto hasHeaders = loader->ResponseInfo() && loader->ResponseInfo()->headers;
  int response_code = -1;
  if (hasHeaders) {
    response_code = loader->ResponseInfo()->headers->response_code();
  }

  // A request was successful if there is response body and the response code is
  // 2XX.
  bool is_success =
      response_body && response_code >= 200 && response_code < 300;
  auto response_body_or_empty = response_body ? *response_body : std::string();
  auto xhr_response_code =
      is_success ? projector::mojom::XhrResponseCode::kSuccess
                 : projector::mojom::XhrResponseCode::kXhrFetchFailure;
  auto response = CreateXhrResposne(response_body_or_empty, xhr_response_code);
  response->net_error_code = GetJsNetErrorCodeFromNetError(loader->NetError());

  std::move(callback).Run(std::move(response));
  if (!is_success) {
    LOG(ERROR) << "Failed to send XHR request, Http error code: "
               << response_code
               << ", response body: " << response_body_or_empty;

    if (response_code == net::HTTP_UNAUTHORIZED) {
      // We show an error message that ask user to open screencast app and try
      // again. If the user do so, `HandleAccessTokenErrorState` will be called
      // for reauth.
      oauth_token_fetcher_.InvalidateToken(token);
    }
  }

  loader_map_.erase(request_id);
}

bool ProjectorXhrSender::IsValidEmail(
    const std::optional<std::string>& email_check) {
  const auto email = email_check.value_or(std::string());
  if (email.empty()) {
    // TODO(b/288457397): Return false here and clean up to require account
    // email when sending with OAuth token.
    return true;
  }
  const std::vector<AccountInfo> accounts = oauth_token_fetcher_.GetAccounts();
  for (const auto& info : accounts) {
    if (email == info.email) {
      return true;
    }
  }
  return false;
}

}  // namespace ash
