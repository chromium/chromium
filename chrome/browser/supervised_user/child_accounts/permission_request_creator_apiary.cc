// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/child_accounts/permission_request_creator_apiary.h"

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/child_accounts/kids_management_api.h"
#include "chrome/common/chrome_switches.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace {

const char kPermissionRequestApiPath[] = "people/me/permissionRequests";

const int kNumPermissionRequestRetries = 1;

// Request keys.
const char kEventTypeKey[] = "eventType";
const char kObjectRefKey[] = "objectRef";
const char kStateKey[] = "state";

// Request values.
const char kEventTypeURLRequest[] = "PERMISSION_CHROME_URL";
const char kState[] = "PENDING";

// Response keys.
const char kPermissionRequestKey[] = "permissionRequest";
const char kIdKey[] = "id";

}  // namespace

struct PermissionRequestCreatorApiary::Request {
  Request(const std::string& request_type,
          const std::string& object_ref,
          SuccessCallback callback);
  ~Request();

  std::string request_type;
  std::string object_ref;
  SuccessCallback callback;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher;
  std::string access_token;
  bool access_token_expired;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader;
};

PermissionRequestCreatorApiary::Request::Request(
    const std::string& request_type,
    const std::string& object_ref,
    SuccessCallback callback)
    : request_type(request_type),
      object_ref(object_ref),
      callback(std::move(callback)),
      access_token_expired(false) {}

PermissionRequestCreatorApiary::Request::~Request() {}

PermissionRequestCreatorApiary::PermissionRequestCreatorApiary(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      retry_on_network_change_(true) {}

PermissionRequestCreatorApiary::~PermissionRequestCreatorApiary() {}

// static
std::unique_ptr<PermissionRequestCreator>
PermissionRequestCreatorApiary::CreateWithProfile(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<PermissionRequestCreatorApiary>(
      identity_manager, profile->GetDefaultStoragePartition()
                            ->GetURLLoaderFactoryForBrowserProcess());
}

bool PermissionRequestCreatorApiary::IsEnabled() const {
  return true;
}

void PermissionRequestCreatorApiary::CreateURLAccessRequest(
    const GURL& url_requested,
    SuccessCallback callback) {
  CreateRequest(kEventTypeURLRequest, url_requested.spec(),
                std::move(callback));
}

GURL PermissionRequestCreatorApiary::GetApiUrl() const {
  return kids_management_api::GetURL(kPermissionRequestApiPath);
}

std::string PermissionRequestCreatorApiary::GetApiScope() const {
  return GaiaConstants::kClassifyUrlKidPermissionOAuth2Scope;
}

void PermissionRequestCreatorApiary::CreateRequest(
    const std::string& request_type,
    const std::string& object_ref,
    SuccessCallback callback) {
  requests_.push_back(
      std::make_unique<Request>(request_type, object_ref, std::move(callback)));
  StartFetching(requests_.back().get());
}

void PermissionRequestCreatorApiary::StartFetching(Request* request) {
  signin::ScopeSet scopes;
  scopes.insert(GetApiScope());
  // It is safe to use Unretained(this) here given that the callback
  // will not be invoked if this object is deleted. Likewise, |request|
  // only comes from |requests_|, which are owned by this object too.
  request->access_token_fetcher =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "permissions_creator", identity_manager_, scopes,
          base::BindOnce(
              &PermissionRequestCreatorApiary::OnAccessTokenFetchComplete,
              base::Unretained(this), request),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          // This class doesn't care about browser sync consent.
          signin::ConsentLevel::kSignin);
}

void PermissionRequestCreatorApiary::OnAccessTokenFetchComplete(
    Request* request,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo token_info) {
  auto it = requests_.begin();
  while (it != requests_.end()) {
    if (request->access_token_fetcher.get() ==
        (*it)->access_token_fetcher.get()) {
      break;
    }
    ++it;
  }
  DCHECK(it != requests_.end());

  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(WARNING) << "Token error: " << error.ToString();
    DispatchResult(it, false);
    return;
  }

  (*it)->access_token = token_info.token;

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("permission_request_creator", R"(
        semantics {
          sender: "Supervised Users"
          description:
            "Requests permission for the user to access a blocked site."
          trigger: "Initiated by the user."
          data:
            "The request is authenticated with an OAuth2 access token "
            "identifying the Google account and contains the URL that the user "
            "requests access to."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled in settings and is only enabled "
            "for child accounts. If sign-in is restricted to accounts from a "
            "managed domain, those accounts are not going to be child accounts."
          chrome_policy {
            RestrictSigninToPattern {
              policy_options {mode: MANDATORY}
              RestrictSigninToPattern: "*@manageddomain.com"
            }
          }
        })");

  base::Value::Dict dict;
  dict.Set(kEventTypeKey, base::Value((*it)->request_type));
  dict.Set(kObjectRefKey, base::Value((*it)->object_ref));
  dict.Set(kStateKey, base::Value(kState));
  std::string body;
  base::JSONWriter::Write(dict, &body);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetApiUrl();
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(supervised_users::kAuthorizationHeaderFormat,
                         token_info.token.c_str()));
  (*it)->simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);
  (*it)->simple_url_loader->AttachStringForUpload(body, "application/json");
  if (retry_on_network_change_) {
    (*it)->simple_url_loader->SetRetryOptions(
        kNumPermissionRequestRetries,
        network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  }
  auto* const simple_url_loader_ptr = (*it)->simple_url_loader.get();
  simple_url_loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PermissionRequestCreatorApiary::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(it)));
}

void PermissionRequestCreatorApiary::OnSimpleLoaderComplete(
    RequestList::iterator it,
    std::unique_ptr<std::string> response_body) {
  Request* request = it->get();
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      std::move(request->simple_url_loader);
  int net_error = simple_url_loader->NetError();
  int response_code = -1;
  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    response_code = simple_url_loader->ResponseInfo()->headers->response_code();
  }

  if (response_code == net::HTTP_UNAUTHORIZED &&
      !request->access_token_expired) {
    request->access_token_expired = true;
    signin::ScopeSet scopes;
    scopes.insert(GetApiScope());
    // "Unconsented" because this class doesn't care about browser sync consent.
    identity_manager_->RemoveAccessTokenFromCache(
        identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
        scopes, request->access_token);
    StartFetching(request);
    return;
  }

  if (response_code != net::HTTP_OK) {
    LOG(WARNING) << "HTTP error " << response_code;
    DispatchResult(std::move(it), false);
    return;
  }

  if (net_error != net::OK) {
    LOG(WARNING) << "Network error " << net_error;
    DispatchResult(std::move(it), false);
    return;
  }

  std::string body;
  if (response_body)
    body = std::move(*response_body);

  absl::optional<base::Value> value = base::JSONReader::Read(body);
  if (!value || !value->is_dict()) {
    LOG(WARNING) << "Invalid top-level dictionary";
    DispatchResult(std::move(it), false);
    return;
  }
  const base::Value::Dict& dict = value->GetDict();
  const base::Value::Dict* permission_dict =
      dict.FindDict(kPermissionRequestKey);
  if (!permission_dict) {
    LOG(WARNING) << "Permission request not found";
    DispatchResult(std::move(it), false);
    return;
  }
  if (!permission_dict->FindString(kIdKey)) {
    LOG(WARNING) << "ID not found";
    DispatchResult(std::move(it), false);
    return;
  }
  DispatchResult(std::move(it), true);
}

void PermissionRequestCreatorApiary::DispatchResult(RequestList::iterator it,
                                                    bool success) {
  std::move((*it)->callback).Run(success);
  requests_.erase(it);
}
