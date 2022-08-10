// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/kids_chrome_management/kids_external_fetcher.h"

#include <memory>
#include <string>

#include "base/memory/safe_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace chrome::kids {

namespace {
// Controls the retry count of the simple url loader.
const int kNumFamilyInfoFetcherRetries = 1;

using ::network::ResourceRequest;

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

std::string GetFailureMessageFromResponseBody(
    std::unique_ptr<std::string> response_body) {
  return response_body ? *response_body : "No response body";
}

// A fetcher with underlying network::SharedURLLoaderFactory.
template <typename Request, typename Response>
class FetcherImpl final : public Fetcher<Request, Response> {
 public:
  FetcherImpl(FetcherDelegate<Response>& delegate,
              scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : delegate_(delegate), url_loader_factory_(url_loader_factory) {}

  void StartRequest(const Request& request,
                    base::StringPiece access_token,
                    base::StringPiece url) override {
    net::NetworkTrafficAnnotationTag traffic_annotation =
        GetDefaultNetworkTrafficAnnotationTag<Request>();
    DCHECK(!simple_url_loader_);
    std::string serialized_request = request.SerializeAsString();
    const GURL gurl(url);
    simple_url_loader_ = InitializeSimpleUrlLoader(
        serialized_request, access_token, gurl, traffic_annotation);

    DCHECK(simple_url_loader_);
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&FetcherImpl::OnSimpleUrlLoaderComplete,
                       weak_ptr_factory_.GetSafeRef()));
  };

  // Not copyable
  FetcherImpl(const FetcherImpl&) = delete;
  FetcherImpl& operator=(const FetcherImpl&) = delete;

 private:
  void OnSimpleUrlLoaderComplete(std::unique_ptr<std::string> response_body) {
    if (!IsLoadingSuccessful(*simple_url_loader_) ||
        !HasHttpOkResponse(*simple_url_loader_)) {
      delegate_.OnFailure(
          GetFailureMessageFromResponseBody(std::move(response_body)));
      return;
    }

    std::unique_ptr<Response> response = std::make_unique<Response>();
    if (!response->ParseFromString(*response_body)) {
      delegate_.OnMalformedResponse(
          GetFailureMessageFromResponseBody(std::move(response_body)));
      return;
    }

    delegate_.OnSuccess(std::move(response));
  }

  FetcherDelegate<Response>& delegate_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  base::WeakPtrFactory<FetcherImpl> weak_ptr_factory_{this};
};

template class FetcherImpl<kids_chrome_management::ListFamilyMembersRequest,
                           kids_chrome_management::ListFamilyMembersResponse>;

}  // namespace

std::unique_ptr<Fetcher<kids_chrome_management::ListFamilyMembersRequest,
                        kids_chrome_management::ListFamilyMembersResponse>>
CreateListFamilyMembersFetcher(
    FetcherDelegate<kids_chrome_management::ListFamilyMembersResponse>&
        delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return std::make_unique<
      FetcherImpl<kids_chrome_management::ListFamilyMembersRequest,
                  kids_chrome_management::ListFamilyMembersResponse>>(
      delegate, url_loader_factory);
}

}  // namespace chrome::kids
