// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_external_fetcher.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/safe_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "chrome/browser/supervised_user/kids_chrome_management/kids_access_token_fetcher.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {
// Controls the retry count of the simple url loader.
const int kNumFamilyInfoFetcherRetries = 1;

using ::base::BindOnce;
using ::base::Unretained;
using ::kids_chrome_management::ListFamilyMembersRequest;
using ::kids_chrome_management::ListFamilyMembersResponse;
using ::network::ResourceRequest;
using ::signin::IdentityManager;

bool IsLoadingSuccessful(const network::SimpleURLLoader& loader) {
  return loader.NetError() == net::OK;
}

bool HasHttpOkResponse(const network::SimpleURLLoader& loader) {
  if (!loader.ResponseInfo()) {
    return false;
  }
  if (!loader.ResponseInfo()->headers) {
    return false;
  }
  return net::HttpStatusCode(loader.ResponseInfo()->headers->response_code()) ==
         net::HTTP_OK;
}

std::unique_ptr<network::SimpleURLLoader> InitializeSimpleUrlLoader(
    base::StringPiece payload,
    base::StringPiece access_token,
    const GURL& url,
    net::NetworkTrafficAnnotationTag traffic_annotation) {
  std::unique_ptr<ResourceRequest> resource_request =
      std::make_unique<ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "POST";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(supervised_users::kAuthorizationHeaderFormat,
                         access_token));
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  simple_url_loader->SetRetryOptions(
      kNumFamilyInfoFetcherRetries,
      network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader->AttachStringForUpload(std::string(payload),
                                           "application/x-protobuf");
  return simple_url_loader;
}

template <typename Request>
net::NetworkTrafficAnnotationTag GetDefaultNetworkTrafficAnnotationTag();

template <>
net::NetworkTrafficAnnotationTag GetDefaultNetworkTrafficAnnotationTag<
    kids_chrome_management::ListFamilyMembersRequest>() {
  return net::DefineNetworkTrafficAnnotation(
      "kids_chrome_management_list_family_members", R"(
        semantics {
          sender: "Supervised Users"
          description:
            "Fetches information about the user's family group from the Google "
            "Family API."
          trigger:
            "Triggered in regular intervals to update profile information."
          data:
            "The request is authenticated with an OAuth2 access token "
            "identifying the Google account. No other information is sent."
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
}

// A fetcher with underlying network::SharedURLLoaderFactory.
template <typename Request, typename Response>
class FetcherImpl final : public KidsExternalFetcher<Request, Response> {
 private:
  using Callback = typename KidsExternalFetcher<Request, Response>::Callback;

 public:
  FetcherImpl() = delete;
  explicit FetcherImpl(
      IdentityManager& identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::StringPiece url,
      Request request,
      Callback callback) {
    access_token_fetcher_ = std::make_unique<KidsAccessTokenFetcher>(
        identity_manager,
        BindOnce(&FetcherImpl::StartRequest, Unretained(this),
                 url_loader_factory, GURL(url), request,
                 std::move(callback)));  // Unretained() is safe because `this`
                                         // owns `access_token_fetcher_`.
  }

  // Not copyable
  FetcherImpl(const FetcherImpl&) = delete;
  FetcherImpl& operator=(const FetcherImpl&) = delete;

 private:
  void StartRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      GURL gurl,
      Request request,
      Callback callback,
      base::expected<signin::AccessTokenInfo, GoogleServiceAuthError>
          access_token) {
    DCHECK(
        callback);  // https://chromium.googlesource.com/chromium/src/+/main/docs/callback.md#creating-a-callback-that-does-nothing

    if (!access_token.has_value()) {
      std::move(callback).Run(KidsExternalFetcherStatus::GoogleServiceAuthError(
                                  access_token.error()),
                              std::make_unique<Response>());
      return;
    }
    base::StringPiece token_value = access_token.value().token;
    net::NetworkTrafficAnnotationTag traffic_annotation =
        GetDefaultNetworkTrafficAnnotationTag<Request>();
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
        InitializeSimpleUrlLoader(request.SerializeAsString(), token_value,
                                  gurl, traffic_annotation);

    DCHECK(simple_url_loader);
    auto* simple_url_loader_ptr = simple_url_loader.get();
    simple_url_loader_ptr->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory.get(),
        base::BindOnce(&FetcherImpl::OnSimpleUrlLoaderComplete,
                       weak_ptr_factory_.GetSafeRef(), std::move(callback),
                       std::move(simple_url_loader)));
  }

  void OnSimpleUrlLoaderComplete(
      Callback callback,
      std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
      std::unique_ptr<std::string> response_body) {
    if (!IsLoadingSuccessful(*simple_url_loader) ||
        !HasHttpOkResponse(*simple_url_loader)) {
      std::move(callback).Run(KidsExternalFetcherStatus::HttpError(),
                              std::make_unique<Response>());
      return;
    }

    std::unique_ptr<Response> response = std::make_unique<Response>();
    if (!response->ParseFromString(*response_body)) {
      std::move(callback).Run(KidsExternalFetcherStatus::InvalidResponse(),
                              std::move(response));
      return;
    }

    std::move(callback).Run(std::move(KidsExternalFetcherStatus::Ok()),
                            std::move(response));
  }

  std::unique_ptr<KidsAccessTokenFetcher> access_token_fetcher_;
  base::WeakPtrFactory<FetcherImpl> weak_ptr_factory_{this};
};

template class FetcherImpl<ListFamilyMembersRequest, ListFamilyMembersResponse>;

ListFamilyMembersRequest CreateListFamilyMembersRequest() {
  ListFamilyMembersRequest request;
  request.set_family_id("mine");  // Required by the contract of the protocol,
                                  // see proto definition.
  return request;
}
}  // namespace

// Main constructor, referenced by the rest.
KidsExternalFetcherStatus::KidsExternalFetcherStatus(
    State state,
    class GoogleServiceAuthError google_service_auth_error)
    : state_(state), google_service_auth_error_(google_service_auth_error) {}
KidsExternalFetcherStatus::~KidsExternalFetcherStatus() = default;

KidsExternalFetcherStatus::KidsExternalFetcherStatus(State state)
    : state_(state) {
  DCHECK(state != State::GOOGLE_SERVICE_AUTH_ERROR);
}
KidsExternalFetcherStatus::KidsExternalFetcherStatus(
    class GoogleServiceAuthError google_service_auth_error)
    : KidsExternalFetcherStatus(GOOGLE_SERVICE_AUTH_ERROR,
                                google_service_auth_error) {}

KidsExternalFetcherStatus::KidsExternalFetcherStatus(
    const KidsExternalFetcherStatus& other) = default;
KidsExternalFetcherStatus& KidsExternalFetcherStatus::operator=(
    const KidsExternalFetcherStatus& other) = default;

KidsExternalFetcherStatus KidsExternalFetcherStatus::Ok() {
  return KidsExternalFetcherStatus(State::NO_ERROR);
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::GoogleServiceAuthError(
    class GoogleServiceAuthError error) {
  return KidsExternalFetcherStatus(error);
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::HttpError() {
  return KidsExternalFetcherStatus(State::HTTP_ERROR);
}
KidsExternalFetcherStatus KidsExternalFetcherStatus::InvalidResponse() {
  return KidsExternalFetcherStatus(State::INVALID_RESPONSE);
}

bool KidsExternalFetcherStatus::IsOk() const {
  return state_ == State::NO_ERROR;
}
KidsExternalFetcherStatus::State KidsExternalFetcherStatus::state() const {
  return state_;
}
const GoogleServiceAuthError&
KidsExternalFetcherStatus::google_service_auth_error() const {
  return google_service_auth_error_;
}

std::unique_ptr<
    KidsExternalFetcher<ListFamilyMembersRequest, ListFamilyMembersResponse>>
FetchListFamilyMembers(
    IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::StringPiece url,
    KidsExternalFetcher<ListFamilyMembersRequest,
                        ListFamilyMembersResponse>::Callback callback) {
  return std::make_unique<
      FetcherImpl<ListFamilyMembersRequest, ListFamilyMembersResponse>>(
      identity_manager, url_loader_factory, url,
      CreateListFamilyMembersRequest(), std::move(callback));
}
