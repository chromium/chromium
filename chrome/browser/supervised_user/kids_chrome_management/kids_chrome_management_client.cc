// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_chrome_management_client.h"

#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_status.h"
#include "services/identity/public/cpp/scope_set.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

enum class RequestMethod {
  kClassifyUrl,
  kListFamilyMembers,
  kRequestRestrictedUrlAccess,
};

constexpr char kClassifyUrlDataContentType[] =
    "application/x-www-form-urlencoded";

// Constants for ClassifyURL.
constexpr char kClassifyUrlRequestApiPath[] =
    "https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/people/"
    "me:classifyUrl";
constexpr char kClassifyUrlKidPermissionScope[] =
    "https://www.googleapis.com/auth/kid.permission";
constexpr char kClassifyUrlOauthConsumerName[] = "kids_url_classifier";
constexpr char kClassifyUrlDataFormat[] = "url=%s&region_code=%s";
constexpr char kClassifyUrlAllowed[] = "allowed";
constexpr char kClassifyUrlRestricted[] = "restricted";

// TODO(crbug.com/980273): remove conversion methods when experiment flag is
// fully flipped. More info on crbug.com/978130.

// Converts the ClassifyUrlRequest proto to a serialized string in the
// format that the Kids Management API receives.
std::string GetClassifyURLRequestString(
    kids_chrome_management::ClassifyUrlRequest* request_proto) {
  std::string query =
      net::EscapeQueryParamValue(request_proto->url(), true /* use_plus */);
  return base::StringPrintf(kClassifyUrlDataFormat, query.c_str(),
                            request_proto->region_code().c_str());
}

// Converts the serialized string returned by the Kids Management API to a
// ClassifyUrlResponse proto object.
std::unique_ptr<kids_chrome_management::ClassifyUrlResponse>
GetClassifyURLResponseProto(const std::string& response) {
  base::Optional<base::Value> optional_value = base::JSONReader::Read(response);
  const base::DictionaryValue* dict = nullptr;

  auto response_proto =
      std::make_unique<kids_chrome_management::ClassifyUrlResponse>();

  if (!optional_value || !optional_value.value().GetAsDictionary(&dict)) {
    DLOG(WARNING)
        << "GetClassifyURLResponseProto failed to parse response dictionary";
    response_proto->set_display_classification(
        kids_chrome_management::ClassifyUrlResponse::
            UNKNOWN_DISPLAY_CLASSIFICATION);
    return response_proto;
  }

  const base::Value* classification_value =
      dict->FindKey("displayClassification");
  if (!classification_value) {
    DLOG(WARNING)
        << "GetClassifyURLResponseProto failed to parse displayClassification";
    response_proto->set_display_classification(
        kids_chrome_management::ClassifyUrlResponse::
            UNKNOWN_DISPLAY_CLASSIFICATION);
    return response_proto;
  }

  const std::string classification_string = classification_value->GetString();
  if (classification_string == kClassifyUrlAllowed) {
    response_proto->set_display_classification(
        kids_chrome_management::ClassifyUrlResponse::ALLOWED);
  } else if (classification_string == kClassifyUrlRestricted) {
    response_proto->set_display_classification(
        kids_chrome_management::ClassifyUrlResponse::RESTRICTED);
  } else {
    DLOG(WARNING)
        << "GetClassifyURLResponseProto expected a valid displayClassification";
    response_proto->set_display_classification(
        kids_chrome_management::ClassifyUrlResponse::
            UNKNOWN_DISPLAY_CLASSIFICATION);
  }

  return response_proto;
}

}  // namespace

struct KidsChromeManagementClient::KidsChromeManagementRequest {
  KidsChromeManagementRequest(
      std::unique_ptr<google::protobuf::MessageLite> request_proto,
      KidsChromeManagementClient::KidsChromeManagementCallback callback,
      std::unique_ptr<network::ResourceRequest> resource_request,
      const net::NetworkTrafficAnnotationTag traffic_annotation,
      const char* oauth_consumer_name,
      const char* scope,
      const RequestMethod method)
      : request_proto(std::move(request_proto)),
        callback(std::move(callback)),
        resource_request(std::move(resource_request)),
        traffic_annotation(traffic_annotation),
        access_token_expired(false),
        oauth_consumer_name(oauth_consumer_name),
        scope(scope),
        method(method) {}

  KidsChromeManagementRequest(KidsChromeManagementRequest&&) = default;

  ~KidsChromeManagementRequest() { DCHECK(!callback); }

  std::unique_ptr<google::protobuf::MessageLite> request_proto;
  KidsChromeManagementCallback callback;
  std::unique_ptr<network::ResourceRequest> resource_request;
  const net::NetworkTrafficAnnotationTag traffic_annotation;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher;
  bool access_token_expired;
  const char* oauth_consumer_name;
  const char* scope;
  const RequestMethod method;
};

KidsChromeManagementClient::KidsChromeManagementClient(Profile* profile) {
  url_loader_factory_ =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();

  identity_manager_ = IdentityManagerFactory::GetForProfile(profile);
}

KidsChromeManagementClient::~KidsChromeManagementClient() = default;

void KidsChromeManagementClient::ClassifyURL(
    std::unique_ptr<kids_chrome_management::ClassifyUrlRequest> request_proto,
    KidsChromeManagementCallback callback) {
  DVLOG(1) << "Checking URL:  " << request_proto->url();

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kClassifyUrlRequestApiPath);
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  const net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "kids_chrome_management_client_classify_url", R"(
        semantics {
          sender: "Supervised Users"
          description:
            "Checks whether a given URL (or set of URLs) is considered safe by "
            "a Google Family Link web restrictions API."
          trigger:
            "If the parent enabled this feature for the child account, this is "
            "sent for every navigation."
          data:
            "An OAuth2 access token identifying and authenticating the "
            "Google account, and the URL to be checked."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature is only used in child accounts and cannot be "
            "disabled by settings. Parent accounts can disable it in the "
            "family dashboard."
          policy_exception_justification:
            "Enterprise admins don't have control over this feature "
            "because it can't be enabled on enterprise environements."
        })");

  auto kids_chrome_request = std::make_unique<KidsChromeManagementRequest>(
      std::move(request_proto), std::move(callback),
      std::move(resource_request), traffic_annotation,
      kClassifyUrlOauthConsumerName, kClassifyUrlKidPermissionScope,
      RequestMethod::kClassifyUrl);

  MakeHTTPRequest(std::move(kids_chrome_request));
}

void KidsChromeManagementClient::MakeHTTPRequest(
    std::unique_ptr<KidsChromeManagementRequest> kids_chrome_request) {
  requests_in_progress_.push_front(std::move(kids_chrome_request));

  StartFetching(requests_in_progress_.begin());
}

// Helpful reading for the next 4 methods:
// https://chromium.googlesource.com/chromium/src.git/+/master/docs/callback.md#partial-binding-of-parameters-currying

void KidsChromeManagementClient::StartFetching(
    KidsChromeRequestList::iterator it) {
  KidsChromeManagementRequest* req = it->get();

  identity::ScopeSet scopes{req->scope};

  req->access_token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          req->oauth_consumer_name, identity_manager_, scopes,
          base::BindOnce(
              &KidsChromeManagementClient::OnAccessTokenFetchComplete,
              base::Unretained(this), it),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate);
}

void KidsChromeManagementClient::OnAccessTokenFetchComplete(
    KidsChromeRequestList::iterator it,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    DLOG(WARNING) << "Token error: " << error.ToString();

    std::unique_ptr<google::protobuf::MessageLite> response_proto = nullptr;
    DispatchResult(it, std::move(response_proto),
                   KidsChromeManagementClient::ErrorCode::kTokenError);
    return;
  }

  KidsChromeManagementRequest* req = it->get();

  req->resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(supervised_users::kAuthorizationHeaderFormat,
                         token_info.token.c_str()));

  std::string request_data;
  // TODO(crbug.com/980273): remove this when experiment flag is fully flipped.
  if (req->method == RequestMethod::kClassifyUrl) {
    request_data = GetClassifyURLRequestString(
        static_cast<kids_chrome_management::ClassifyUrlRequest*>(
            req->request_proto.get()));
  } else {
    DVLOG(1) << "Could not detect the request proto's class.";
    std::unique_ptr<google::protobuf::MessageLite> response_proto = nullptr;
    DispatchResult(it, std::move(response_proto),
                   KidsChromeManagementClient::ErrorCode::kServiceError);
    return;
  }

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(req->resource_request),
                                       req->traffic_annotation);

  simple_url_loader->AttachStringForUpload(request_data,
                                           kClassifyUrlDataContentType);

  simple_url_loader->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&KidsChromeManagementClient::OnSimpleLoaderComplete,
                     base::Unretained(this), it, std::move(simple_url_loader),
                     token_info),
      /*max_body_size*/ 128);
}

void KidsChromeManagementClient::OnSimpleLoaderComplete(
    KidsChromeRequestList::iterator it,
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
    signin::AccessTokenInfo token_info,
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;

  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    response_code = simple_url_loader->ResponseInfo()->headers->response_code();

    KidsChromeManagementRequest* req = it->get();
    // Handle first HTTP_UNAUTHORIZED response by removing access token and
    // restarting the request from the beginning (fetching access token).
    if (response_code == net::HTTP_UNAUTHORIZED && !req->access_token_expired) {
      DLOG(WARNING) << "Access token expired:\n" << token_info.token;
      req->access_token_expired = true;
      identity::ScopeSet scopes{req->scope};
      identity_manager_->RemoveAccessTokenFromCache(
          identity_manager_->GetPrimaryAccountId(), scopes, token_info.token);
      StartFetching(it);
      return;
    }
  }

  std::unique_ptr<google::protobuf::MessageLite> response_proto = nullptr;

  int net_error = simple_url_loader->NetError();

  if (net_error != net::OK) {
    DLOG(WARNING) << "Network error " << net_error;
    DispatchResult(it, std::move(response_proto),
                   KidsChromeManagementClient::ErrorCode::kNetworkError);
    return;
  }

  if (response_code != net::HTTP_OK) {
    DLOG(WARNING) << "Response: " << response_body.get();
    DispatchResult(it, std::move(response_proto),
                   KidsChromeManagementClient::ErrorCode::kHttpError);
    return;
  }

  // |response_body| is nullptr only in case of failure.
  if (!response_body) {
    DLOG(WARNING) << "URL request failed! Letting through...";
    DispatchResult(it, std::move(response_proto),
                   KidsChromeManagementClient::ErrorCode::kNetworkError);
    return;
  }

  if (it->get()->method == RequestMethod::kClassifyUrl) {
    response_proto = GetClassifyURLResponseProto(*response_body);
  } else {
    DVLOG(1) << "Could not detect the request proto class.";
    DispatchResult(it, std::move(response_proto),
                   KidsChromeManagementClient::ErrorCode::kServiceError);
    return;
  }

  DispatchResult(it, std::move(response_proto),
                 KidsChromeManagementClient::ErrorCode::kSuccess);
}

void KidsChromeManagementClient::DispatchResult(
    KidsChromeRequestList::iterator it,
    std::unique_ptr<google::protobuf::MessageLite> response_proto,
    ErrorCode error) {
  std::move(it->get()->callback).Run(std::move(response_proto), error);

  requests_in_progress_.erase(it);
}
