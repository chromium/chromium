// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/oauth_http_fetcher.h"

#include "ash/quick_pair/common/fast_pair/fast_pair_http_result.h"
#include "ash/quick_pair/common/quick_pair_browser_delegate.h"
#include "components/cross_device/logging/logging.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace ash {
namespace quick_pair {

OAuthHttpFetcher::OAuthHttpFetcher(
    const net::PartialNetworkTrafficAnnotationTag& traffic_annotation,
    const std::string& oauth_scope)
    : traffic_annotation_(traffic_annotation) {
  oauth_scopes_.insert(oauth_scope);
}

OAuthHttpFetcher::~OAuthHttpFetcher() = default;

void OAuthHttpFetcher::ExecuteGetRequest(const GURL& url,
                                         FetchCompleteCallback callback) {
  request_type_ = RequestType::GET;
  StartRequest(url, std::move(callback));
}

void OAuthHttpFetcher::ExecutePostRequest(const GURL& url,
                                          const std::string& body,
                                          FetchCompleteCallback callback) {
  request_type_ = RequestType::POST;
  body_ = body;
  StartRequest(url, std::move(callback));
}

void OAuthHttpFetcher::ExecuteDeleteRequest(const GURL& url,
                                            FetchCompleteCallback callback) {
  request_type_ = RequestType::DELETE;
  StartRequest(url, std::move(callback));
}

void OAuthHttpFetcher::StartRequest(const GURL& url,
                                    FetchCompleteCallback callback) {
  CD_LOG(VERBOSE, Feature::FP) << __func__ << ": executing request to: " << url;

  CHECK(!has_call_started_)
      << __func__
      << ": Attempted to make an API call, but there is already a "
         "request in progress.";

  signin::IdentityManager* const identity_manager =
      QuickPairBrowserDelegate::Get()->GetIdentityManager();
  CHECK(identity_manager) << __func__ << ": IdentityManager is not available.";

  has_call_started_ = true;
  url_ = url;
  callback_ = std::move(callback);
  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          "fastpair_client", identity_manager, oauth_scopes_,
          base::BindOnce(&OAuthHttpFetcher::OnAccessTokenFetched,
                         weak_ptr_factory_.GetWeakPtr()),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
}

void OAuthHttpFetcher::OnAccessTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": Failed to retrieve access token. "
        << error.ToString();
    std::move(callback_).Run(nullptr, nullptr);
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      QuickPairBrowserDelegate::Get()->GetURLLoaderFactory();
  if (!url_loader_factory) {
    CD_LOG(WARNING, Feature::FP)
        << __func__ << ": URLLoaderFactory is not available.";
    std::move(callback_).Run(nullptr, nullptr);
    return;
  }

  CD_LOG(VERBOSE, Feature::FP) << "Access token fetched successfully.";

  OAuth2ApiCallFlow::Start(std::move(url_loader_factory),
                           access_token_info.token);
}

GURL OAuthHttpFetcher::CreateApiCallUrl() {
  return url_;
}

std::string OAuthHttpFetcher::CreateApiCallBody() {
  switch (request_type_) {
    case RequestType::GET:
    case RequestType::DELETE:
      return std::string();

    case RequestType::POST:
      return body_;
  }
}

std::string OAuthHttpFetcher::CreateApiCallBodyContentType() {
  switch (request_type_) {
    case RequestType::GET:
    case RequestType::DELETE:
      return std::string();

    case RequestType::POST:
      return "application/x-protobuf";
  }
}

std::string OAuthHttpFetcher::GetRequestTypeForBody(const std::string& body) {
  switch (request_type_) {
    case RequestType::GET:
      return "GET";

    case RequestType::POST:
      return "POST";

    case RequestType::DELETE:
      return "DELETE";
  }
}

void OAuthHttpFetcher::ProcessApiCallSuccess(
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  CD_LOG(INFO, Feature::FP) << __func__;

  std::move(callback_).Run(
      std::move(body),
      std::make_unique<FastPairHttpResult>(/*net_error=*/net::OK,
                                           /*head=*/head));
}

void OAuthHttpFetcher::ProcessApiCallFailure(
    int net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  CD_LOG(WARNING, Feature::FP) << __func__ << ": net_err=" << net_error;

  std::move(callback_).Run(
      nullptr, std::make_unique<FastPairHttpResult>(/*net_error=*/net_error,
                                                    /*head=*/head));
}

net::PartialNetworkTrafficAnnotationTag
OAuthHttpFetcher::GetNetworkTrafficAnnotationTag() {
  return traffic_annotation_;
}

}  // namespace quick_pair
}  // namespace ash
