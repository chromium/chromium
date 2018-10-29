// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/auth/arc_background_auth_code_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace arc {

namespace {

constexpr int kGetAuthCodeNetworkRetry = 3;

constexpr char kConsumerName[] = "ArcAuthContext";
constexpr char kToken[] = "token";
constexpr char kErrorDescription[] = "error_description";
constexpr char kDeviceId[] = "device_id";
constexpr char kDeviceType[] = "device_type";
constexpr char kDeviceTypeArc[] = "arc_plus_plus";
constexpr char kLoginScopedToken[] = "login_scoped_token";
constexpr char kGetAuthCodeKey[] = "Content-Type";
constexpr char kGetAuthCodeValue[] = "application/json; charset=utf-8";
constexpr char kContentTypeJSON[] = "application/json";

}  // namespace

const char kAuthTokenExchangeEndPoint[] =
    "https://www.googleapis.com/oauth2/v4/ExchangeToken";

ArcBackgroundAuthCodeFetcher::ArcBackgroundAuthCodeFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    const std::string& account_id,
    bool initial_signin)
    : OAuth2TokenService::Consumer(kConsumerName),
      url_loader_factory_(std::move(url_loader_factory)),
      profile_(profile),
      context_(profile_, account_id),
      initial_signin_(initial_signin),
      weak_ptr_factory_(this) {}

ArcBackgroundAuthCodeFetcher::~ArcBackgroundAuthCodeFetcher() = default;

void ArcBackgroundAuthCodeFetcher::Fetch(FetchCallback callback) {
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);
  context_.Prepare(base::Bind(&ArcBackgroundAuthCodeFetcher::OnPrepared,
                              weak_ptr_factory_.GetWeakPtr()));
}

void ArcBackgroundAuthCodeFetcher::OnPrepared(
    net::URLRequestContextGetter* request_context_getter) {
  if (!request_context_getter) {
    ReportResult(std::string(), OptInSilentAuthCode::CONTEXT_NOT_READY);
    return;
  }

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kOAuth1LoginScope);
  login_token_request_ = context_.StartAccessTokenRequest(scopes, this);
}

void ArcBackgroundAuthCodeFetcher::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  ResetFetchers();

  const std::string device_id = user_manager::known_user::GetDeviceId(
      multi_user_util::GetAccountIdFromProfile(profile_));
  DCHECK(!device_id.empty());

  base::DictionaryValue request_data;
  request_data.SetString(kLoginScopedToken, token_response.access_token);
  request_data.SetString(kDeviceType, kDeviceTypeArc);
  request_data.SetString(kDeviceId, device_id);
  std::string request_string;
  base::JSONWriter::Write(request_data, &request_string);
  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("arc_auth_code_fetcher", R"(
      semantics {
        sender: "ARC auth code fetcher"
        description:
          "Fetches auth code to be used for Google Play Store sign-in."
        trigger:
          "The user or administrator initially enables Google Play Store on"
          "the device, and Google Play Store requests authorization code for "
          "account setup. This is also triggered when the Google Play Store "
          "detects that current credentials are revoked or invalid and "
          "requests extra authorization code for the account re-sign in."
        data: "Device id and access token."
        destination: GOOGLE_OWNED_SERVICE
        }
      policy {
        cookies_allowed: NO
        setting:
          "There's no direct Chromium's setting to disable this, but you can "
          "remove Google Play Store in Chrome's settings under the Google "
          "Play Store section if this is allowed by policy."
        policy_exception_justification: "Not implemented."
  })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kAuthTokenExchangeEndPoint);
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE |
      net::LOAD_DO_NOT_SEND_COOKIES | net::LOAD_DO_NOT_SAVE_COOKIES;
  resource_request->method = "POST";
  resource_request->headers.SetHeader(kGetAuthCodeKey, kGetAuthCodeValue);
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(request_string, kContentTypeJSON);
  simple_url_loader_->SetRetryOptions(
      kGetAuthCodeNetworkRetry,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->SetAllowHttpErrorResults(true);
  // base::Unretained is safe here since this class owns |simple_url_loader_|'s
  // lifetime.
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ArcBackgroundAuthCodeFetcher::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

void ArcBackgroundAuthCodeFetcher::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  LOG(WARNING) << "Failed to get LST " << error.ToString() << ".";
  ResetFetchers();
  ReportResult(std::string(), OptInSilentAuthCode::NO_LST_TOKEN);
}

void ArcBackgroundAuthCodeFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  std::string json_string;
  if (response_body)
    json_string = std::move(*response_body);

  ResetFetchers();

  JSONStringValueDeserializer deserializer(json_string);
  std::string error_msg;
  std::unique_ptr<base::Value> json_value =
      deserializer.Deserialize(nullptr, &error_msg);

  if (!response_body || (response_code != net::HTTP_OK)) {
    const auto* error_value =
        json_value && json_value->is_dict()
            ? json_value->FindKeyOfType(kErrorDescription,
                                        base::Value::Type::STRING)
            : nullptr;

    LOG(WARNING) << "Server returned wrong response code: " << response_code
                 << ": " << (error_value ? error_value->GetString() : "Unknown")
                 << ".";

    OptInSilentAuthCode uma_status;
    if (response_code >= 400 && response_code < 500)
      uma_status = OptInSilentAuthCode::HTTP_CLIENT_FAILURE;
    if (response_code >= 500 && response_code < 600)
      uma_status = OptInSilentAuthCode::HTTP_SERVER_FAILURE;
    else
      uma_status = OptInSilentAuthCode::HTTP_UNKNOWN_FAILURE;
    ReportResult(std::string(), uma_status);
    return;
  }

  if (!json_value) {
    LOG(WARNING) << "Unable to deserialize auth code json data: " << error_msg
                 << ".";
    ReportResult(std::string(), OptInSilentAuthCode::RESPONSE_PARSE_FAILURE);
    return;
  }

  if (!json_value->is_dict()) {
    LOG(WARNING) << "Response is not a JSON dictionary.";
    ReportResult(std::string(), OptInSilentAuthCode::RESPONSE_PARSE_FAILURE);
    return;
  }

  const auto* auth_code_value =
      json_value->FindKeyOfType(kToken, base::Value::Type::STRING);
  std::string auth_code =
      auth_code_value ? auth_code_value->GetString() : std::string();
  if (auth_code.empty()) {
    LOG(WARNING) << "Response does not contain auth code.";
    ReportResult(std::string(), OptInSilentAuthCode::NO_AUTH_CODE_IN_RESPONSE);
    return;
  }

  ReportResult(auth_code, OptInSilentAuthCode::SUCCESS);
}

void ArcBackgroundAuthCodeFetcher::ResetFetchers() {
  login_token_request_.reset();
  simple_url_loader_.reset();
}

void ArcBackgroundAuthCodeFetcher::ReportResult(
    const std::string& auth_code,
    OptInSilentAuthCode uma_status) {
  if (initial_signin_) {
    UpdateSilentAuthCodeUMA(uma_status);
  } else {
    // TODO(sinhak): Check if we need to migrate this / create a new metric.
    UpdateReauthorizationSilentAuthCodeUMA(uma_status);
  }
  std::move(callback_).Run(!auth_code.empty(), auth_code);
}

void ArcBackgroundAuthCodeFetcher::SkipMergeSessionForTesting() {
  context_.SkipMergeSessionForTesting();
}

}  // namespace arc
