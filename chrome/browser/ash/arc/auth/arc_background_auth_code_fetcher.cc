// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/auth/arc_background_auth_code_fetcher.h"

#include <utility>

#include "ash/components/arc/arc_features.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "components/account_id/account_id.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace arc {

namespace {

constexpr int kGetAuthCodeNetworkRetry = 3;

constexpr char kConsumerName[] = "ArcAuthContext";
constexpr char kToken[] = "token";
constexpr char kErrorDescription[] = "error_description";
constexpr char kDeviceId[] = "device_id";
constexpr char kDeviceType[] = "device_type";
constexpr char kDeviceTypeArc[] = "arc_plus_plus";
constexpr char kClientId[] = "client_id";
constexpr char kClientIdArc[] =
    "1070009224336-sdh77n7uot3oc99ais00jmuft6sk2fg9.apps.googleusercontent.com";
constexpr char kRefreshToken[] = "refresh_token";
constexpr char kGetAuthCodeKey[] = "Content-Type";
constexpr char kGetAuthCodeValue[] = "application/json; charset=utf-8";
constexpr char kContentTypeJSON[] = "application/json";

}  // namespace

namespace {

signin::ScopeSet GetAccessTokenScopes() {
  return signin::ScopeSet({GaiaConstants::kOAuth1LoginScope});
}

}  // namespace

const char kTokenBootstrapEndPoint[] =
    "https://oauthtokenbootstrap.googleapis.com/v1/tokenbootstrap";

ArcBackgroundAuthCodeFetcher::ArcBackgroundAuthCodeFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    const CoreAccountId& account_id,
    bool initial_signin,
    bool is_primary_account)
    : url_loader_factory_(std::move(url_loader_factory)),
      profile_(profile),
      context_(profile_, account_id),
      initial_signin_(initial_signin),
      is_primary_account_(is_primary_account) {}

ArcBackgroundAuthCodeFetcher::~ArcBackgroundAuthCodeFetcher() = default;

void ArcBackgroundAuthCodeFetcher::Fetch(FetchCallback callback) {
  bypass_proxy_ = false;
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);
  context_.Prepare(base::BindOnce(&ArcBackgroundAuthCodeFetcher::OnPrepared,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void ArcBackgroundAuthCodeFetcher::OnPrepared(bool success) {
  if (!success) {
    ReportResult(std::string(), OptInSilentAuthCode::CONTEXT_NOT_READY);
    return;
  }

  StartFetchingAccessToken();
}

void ArcBackgroundAuthCodeFetcher::AttemptToRecoverAccessToken(
    const signin::AccessTokenInfo& token_info) {
  DCHECK(!attempted_to_recover_access_token_);
  attempted_to_recover_access_token_ = true;
  context_.RemoveAccessTokenFromCache(GetAccessTokenScopes(), token_info.token);
  StartFetchingAccessToken();
}

void ArcBackgroundAuthCodeFetcher::StartFetchingAccessToken() {
  DCHECK(!simple_url_loader_);
  DCHECK(!access_token_fetcher_);
  access_token_fetcher_ = context_.CreateAccessTokenFetcher(
      kConsumerName, GetAccessTokenScopes(),
      base::BindOnce(&ArcBackgroundAuthCodeFetcher::OnAccessTokenFetchComplete,
                     base::Unretained(this)));
}

void ArcBackgroundAuthCodeFetcher::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(WARNING) << "Failed to get LST " << error.ToString() << ".";
    ReportResult(std::string(), OptInSilentAuthCode::NO_LST_TOKEN);
    return;
  }

  user_manager::KnownUser known_user(g_browser_process->local_state());
  const std::string device_id = known_user.GetDeviceId(
      multi_user_util::GetAccountIdFromProfile(profile_));
  DCHECK(!device_id.empty());

  base::Value::Dict request_data;
  request_data.Set(kRefreshToken, token_info.token);
  request_data.Set(kClientId, kClientIdArc);
  request_data.Set(kDeviceType, kDeviceTypeArc);
  request_data.Set(kDeviceId, device_id);
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
        data: "Device id, access token, and hardcoded client id."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
            email: "arc-core@google.com"
          }
        }
        user_data {
          type: ACCESS_TOKEN
        }
        last_reviewed: "2023-01-24"
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
  resource_request->url = GURL(kTokenBootstrapEndPoint);
  resource_request->load_flags = net::LOAD_DISABLE_CACHE |
                                 net::LOAD_BYPASS_CACHE |
                                 (bypass_proxy_ ? net::LOAD_BYPASS_PROXY : 0);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  resource_request->headers.SetHeader(kGetAuthCodeKey, kGetAuthCodeValue);

  DCHECK(!simple_url_loader_);

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
                     base::Unretained(this), token_info));
}

void ArcBackgroundAuthCodeFetcher::OnSimpleLoaderComplete(
    signin::AccessTokenInfo token_info,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  int net_error = simple_url_loader_->NetError();
  bool mandatory_proxy_failed = net_error ==
                                net::ERR_MANDATORY_PROXY_CONFIGURATION_FAILED;

  // If the network request has failed because of an unreachable PAC script,
  // retry the request without the proxy.
  if (mandatory_proxy_failed && !bypass_proxy_) {
    bypass_proxy_ = true;
    simple_url_loader_.reset();
    access_token_fetcher_.reset();
    StartFetchingAccessToken();
    return;
  }

  std::string json_string;
  if (response_body)
    json_string = std::move(*response_body);

  simple_url_loader_.reset();

  JSONStringValueDeserializer deserializer(json_string);
  std::string error_msg;
  std::unique_ptr<base::Value> json_value =
      deserializer.Deserialize(nullptr, &error_msg);

  if (!response_body || (response_code != net::HTTP_OK)) {
    const std::string* error =
        json_value && json_value->is_dict()
            ? json_value->GetDict().FindString(kErrorDescription)
            : nullptr;

    LOG(WARNING) << "Server request failed."
                 << " Net error: " << net_error
                 << ": " << net::ErrorToString(net_error)
                 << ", response code: " << response_code
                 << ": " << (error ? *error : "Unknown") << ".";

    OptInSilentAuthCode uma_status;
    if (response_code >= 400 && response_code < 500) {
      if (!attempted_to_recover_access_token_) {
        AttemptToRecoverAccessToken(token_info);
        return;
      }
      uma_status = OptInSilentAuthCode::HTTP_CLIENT_FAILURE;
    } else if (response_code >= 500 && response_code < 600) {
      uma_status = OptInSilentAuthCode::HTTP_SERVER_FAILURE;
    } else if (mandatory_proxy_failed) {
      uma_status = OptInSilentAuthCode::MANDATORY_PROXY_CONFIGURATION_FAILED;
    } else {
      uma_status = OptInSilentAuthCode::HTTP_UNKNOWN_FAILURE;
    }
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

  const std::string* auth_code = json_value->GetDict().FindString(kToken);
  if (!auth_code || auth_code->empty()) {
    LOG(WARNING) << "Response does not contain auth code.";
    ReportResult(std::string(), OptInSilentAuthCode::NO_AUTH_CODE_IN_RESPONSE);
    return;
  }

  ReportResult(*auth_code, OptInSilentAuthCode::SUCCESS);
}

void ArcBackgroundAuthCodeFetcher::ReportResult(
    const std::string& auth_code,
    OptInSilentAuthCode uma_status) {
  if (initial_signin_) {
    UpdateSilentAuthCodeUMA(uma_status);
  } else {
    // Not the initial provisioning.
    if (is_primary_account_)
      UpdateReauthorizationSilentAuthCodeUMA(uma_status);
    else
      UpdateSecondaryAccountSilentAuthCodeUMA(uma_status);
  }
  std::move(callback_).Run(!auth_code.empty(), auth_code);
}

}  // namespace arc
