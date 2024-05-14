// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "content/public/browser/browser_context.h"
#include "google_apis/credentials_mode.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {
namespace {

// These values should not be renumbered and numeric values should never
// be reused. This must be kept in sync with SamlInSessionPasswordSyncEvent
// in tools/metrics/histograms/enums.xml
enum class InSessionPasswordSyncEvent {
  kStartPollingInSession = 0,
  kStartPollingOnLogin = 1,
  kTokenValidationSuccess = 2,
  kTokenValidationFailure = 3,
  kErrorMissingAccessToken = 4,
  kErrorWrongResponseCode = 5,
  kErrorInSerializedResponse = 6,
  kErrorNoTokenInCreateResponse = 7,
  kErrorNoTokenInGetResponse = 8,
  kMaxValue = kErrorNoTokenInGetResponse,
};

constexpr int kGetAuthCodeNetworkRetry = 1;
constexpr int kMaxResponseSize = 5 * 1024;
const char kAccessTokenFetchId[] = "sync_token_fetcher";

const char kErrorKey[] = "error";
const char kErrorDescription[] = "message";
const char kToken[] = "name";
const char kTokenEntry[] = "token";
const char kTokenStatusKey[] = "tokenStatus";
const char kTokenStatusValid[] = "VALID";
const char kAuthorizationHeaderFormat[] = "Bearer %s";
const char kContentTypeJSON[] = "application/json";
const char kTokenTypeKey[] = "token_type";
const char kTokenTypeValue[] = "SAML_PASSWORD";
const char kAcceptValue[] =
    "Accept=text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";

const char kPasswordSyncTokenBaseEndPoint[] =
    "https://chromedevicetoken.googleapis.com/v1/tokens";

const char kPasswordSyncTokenCreateEndPoint[] = "";

const char kPasswordSyncTokenGetEndPoint[] = "?token_type=SAML_PASSWORD";

const char kPasswordSyncTokenVerifyEndPoint[] =
    "/%s:verify?token_type=SAML_PASSWORD&key=%s";

std::string GetBaseEndPoint() {
  return kPasswordSyncTokenBaseEndPoint;
}

GURL GetSyncTokenCreateUrl() {
  return GURL(GetBaseEndPoint() +
              std::string(kPasswordSyncTokenCreateEndPoint));
}

GURL GetSyncTokenGetUrl() {
  return GURL(GetBaseEndPoint() + std::string(kPasswordSyncTokenGetEndPoint));
}

GURL GetSyncTokenVerifyUrl(const std::string& sync_token,
                           const std::string& escaped_api_key) {
  return GURL(GetBaseEndPoint() +
              base::StringPrintf(kPasswordSyncTokenVerifyEndPoint,
                                 sync_token.c_str(), escaped_api_key.c_str()));
}

void RecordEvent(InSessionPasswordSyncEvent event) {
  base::UmaHistogramEnumeration("ChromeOS.SAML.InSessionPasswordSyncEvent",
                                event);
}

}  // namespace

void RecordStartOfSyncTokenPollingUMA(bool in_session) {
  RecordEvent(in_session ? InSessionPasswordSyncEvent::kStartPollingInSession
                         : InSessionPasswordSyncEvent::kStartPollingOnLogin);
}

PasswordSyncTokenFetcher::Consumer::Consumer() = default;

PasswordSyncTokenFetcher::Consumer::~Consumer() = default;

PasswordSyncTokenFetcher::PasswordSyncTokenFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile* profile,
    Consumer* consumer)
    : url_loader_factory_(std::move(url_loader_factory)),
      profile_(profile),
      consumer_(consumer),
      request_type_(RequestType::kNone) {
  DCHECK(consumer_);
}

PasswordSyncTokenFetcher::~PasswordSyncTokenFetcher() = default;

void PasswordSyncTokenFetcher::StartTokenCreate() {
  DCHECK_EQ(request_type_, RequestType::kNone);
  request_type_ = RequestType::kCreateToken;
  StartAccessTokenFetch();
}

void PasswordSyncTokenFetcher::StartTokenGet() {
  DCHECK_EQ(request_type_, RequestType::kNone);
  request_type_ = RequestType::kGetToken;
  StartAccessTokenFetch();
}

void PasswordSyncTokenFetcher::StartTokenVerify(const std::string& sync_token) {
  DCHECK_EQ(request_type_, RequestType::kNone);
  request_type_ = RequestType::kVerifyToken;
  sync_token_ = sync_token;
  FetchSyncToken(/*access_token=*/std::string());
}

void PasswordSyncTokenFetcher::StartAccessTokenFetch() {
  DCHECK(profile_);
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  DCHECK(identity_manager);

  // Now we can request the token, knowing that it will be immediately requested
  // if the refresh token is available, or that it will be requested once the
  // refresh token is available for the primary account.
  signin::ScopeSet scopes;
  scopes.insert(GaiaConstants::kGoogleUserInfoEmail);
  scopes.insert(GaiaConstants::kDeviceManagementServiceOAuth);

  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          kAccessTokenFetchId, identity_manager, scopes,
          base::BindOnce(&PasswordSyncTokenFetcher::OnAccessTokenFetchComplete,
                         weak_ptr_factory_.GetWeakPtr()),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kWaitUntilAvailable,
          signin::ConsentLevel::kSignin);
}

void PasswordSyncTokenFetcher::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR)
        << "Could not get access token to authorize sync token operation: "
        << error.ToString();
    RecordEvent(InSessionPasswordSyncEvent::kErrorMissingAccessToken);
    consumer_->OnApiCallFailed(ErrorType::kMissingAccessToken);
    return;
  }
  FetchSyncToken(token_info.token);
}

void PasswordSyncTokenFetcher::FetchSyncToken(const std::string& access_token) {
  auto request_data = base::Value::Dict().Set(kTokenTypeKey, kTokenTypeValue);
  std::string request_string;
  if (!base::JSONWriter::Write(request_data, &request_string)) {
    LOG(ERROR) << "Not able to serialize token request body.";
    consumer_->OnApiCallFailed(ErrorType::kRequestBodyNotSerialized);
    return;
  }
  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("password_sync_token_fetcher", R"(
      semantics {
        sender: "Chrome OS sync token fetcher"
        description:
          "Call password sync token API used to synchronize SAML credentials"
          "between multiple user devices."
        trigger:
          "When SAML password is changed in session or device initiates check "
          "of the local version of password sync token. When the token is "
          "invalid device requests online re-authentication of the user in "
          "order to sync user's password and update the token."
        data: "Access token and token_type."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting : "Only Admins can enable/disable this feature from the admin"
                  "dashboard."
        chrome_policy {
          SamlInSessionPasswordChangeEnabled {
            SamlInSessionPasswordChangeEnabled : false
          }
        }
      })");
  auto resource_request = std::make_unique<network::ResourceRequest>();
  switch (request_type_) {
    case RequestType::kCreateToken:
      resource_request->url = GetSyncTokenCreateUrl();
      break;
    case RequestType::kGetToken:
      resource_request->url = GetSyncTokenGetUrl();
      break;
    case RequestType::kVerifyToken:
      resource_request->url = GetSyncTokenVerifyUrl(
          sync_token_, base::EscapeQueryParamValue(google_apis::GetAPIKey(),
                                                   /*use_plus=*/true));
      break;
    case RequestType::kNone:
      // Error: request type needs to be already set.
      NOTREACHED_IN_MIGRATION();
  }
  resource_request->load_flags =
      net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_CACHE;
  resource_request->credentials_mode =
      google_apis::GetOmitCredentialsModeForGaiaRequests();
  if (request_type_ == RequestType::kCreateToken) {
    resource_request->method = net::HttpRequestHeaders::kPostMethod;
  } else {
    resource_request->method = net::HttpRequestHeaders::kGetMethod;
  }
  if (request_type_ != RequestType::kVerifyToken) {
    resource_request->headers.SetHeader(
        net::HttpRequestHeaders::kAuthorization,
        base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));
  }
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                      kContentTypeJSON);
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      kAcceptValue);
  DCHECK(!simple_url_loader_);

  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  if (request_type_ == RequestType::kCreateToken) {
    simple_url_loader_->AttachStringForUpload(request_string, kContentTypeJSON);
  }
  simple_url_loader_->SetRetryOptions(
      kGetAuthCodeNetworkRetry,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->SetAllowHttpErrorResults(true);
  simple_url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&PasswordSyncTokenFetcher::OnSimpleLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr()),
      kMaxResponseSize);
}

void PasswordSyncTokenFetcher::OnSimpleLoaderComplete(
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
  simple_url_loader_.reset();

  JSONStringValueDeserializer deserializer(json_string);
  std::string error_msg;
  std::unique_ptr<base::Value> json_value =
      deserializer.Deserialize(/*error_code=*/nullptr, &error_msg);

  if (!response_body || (response_code != net::HTTP_OK)) {
    const auto* error_json = json_value && json_value->is_dict()
                                 ? json_value->GetDict().FindDict(kErrorKey)
                                 : nullptr;
    const std::string* error =
        error_json ? error_json->FindString(kErrorDescription) : nullptr;

    LOG(WARNING) << "Server returned wrong response code: " << response_code
                 << ": " << (error ? *error : "Unknown") << ".";
    RecordEvent(InSessionPasswordSyncEvent::kErrorWrongResponseCode);
    consumer_->OnApiCallFailed(ErrorType::kServerError);
    return;
  }

  if (!json_value) {
    LOG(WARNING) << "Unable to deserialize json data.";
    RecordEvent(InSessionPasswordSyncEvent::kErrorInSerializedResponse);
    consumer_->OnApiCallFailed(ErrorType::kInvalidJson);
    return;
  }

  if (!json_value->is_dict()) {
    LOG(WARNING) << "Response is not a JSON dictionary.";
    RecordEvent(InSessionPasswordSyncEvent::kErrorInSerializedResponse);
    consumer_->OnApiCallFailed(ErrorType::kNotJsonDict);
    return;
  }

  ProcessValidTokenResponse(std::move(json_value->GetDict()));
}

void PasswordSyncTokenFetcher::ProcessValidTokenResponse(
    base::Value::Dict json_response) {
  switch (request_type_) {
    case RequestType::kCreateToken: {
      const std::string* sync_token = json_response.FindString(kToken);
      if (!sync_token || sync_token->empty()) {
        LOG(WARNING) << "Response does not contain sync token.";
        RecordEvent(InSessionPasswordSyncEvent::kErrorNoTokenInCreateResponse);
        consumer_->OnApiCallFailed(ErrorType::kCreateNoToken);
        return;
      }
      consumer_->OnTokenCreated(*sync_token);
      break;
    }
    case RequestType::kGetToken: {
      std::string sync_token;
      const auto* token_list_entry = json_response.FindList(kTokenEntry);
      if (!token_list_entry) {
        LOG(WARNING) << "Response does not contain list of sync tokens.";
        RecordEvent(InSessionPasswordSyncEvent::kErrorNoTokenInGetResponse);
        consumer_->OnApiCallFailed(ErrorType::kGetNoList);
        return;
      }
      const base::Value::List& list_of_tokens = *token_list_entry;
      if (list_of_tokens.size() > 0) {
        const std::string* sync_token_string =
            list_of_tokens[0].GetDict().FindString(kToken);
        if (!sync_token_string || sync_token_string->empty()) {
          LOG(WARNING) << "Response does not contain sync token.";
          RecordEvent(InSessionPasswordSyncEvent::kErrorNoTokenInGetResponse);
          consumer_->OnApiCallFailed(ErrorType::kGetNoToken);
          return;
        }
        sync_token = *sync_token_string;
      }
      // list_of_tokens.size() == 0 is still a valid case here - it means we
      // have not created any token for this user yet.
      consumer_->OnTokenFetched(sync_token);
      break;
    }
    case RequestType::kVerifyToken: {
      const std::string* sync_token_status =
          json_response.FindString(kTokenStatusKey);
      bool is_valid = false;
      if (sync_token_status && *sync_token_status == kTokenStatusValid) {
        is_valid = true;
      }
      RecordEvent(is_valid
                      ? InSessionPasswordSyncEvent::kTokenValidationSuccess
                      : InSessionPasswordSyncEvent::kTokenValidationFailure);
      consumer_->OnTokenVerified(is_valid);
      break;
    }
    case RequestType::kNone:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace ash
