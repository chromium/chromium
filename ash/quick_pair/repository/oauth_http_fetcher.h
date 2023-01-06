// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_REPOSITORY_OAUTH_HTTP_FETCHER_H_
#define ASH_QUICK_PAIR_REPOSITORY_OAUTH_HTTP_FETCHER_H_

#include "ash/quick_pair/repository/http_fetcher.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "google_apis/gaia/oauth2_api_call_flow.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace ash {
namespace quick_pair {

// Makes HTTP GET requests and returns the response.
class OAuthHttpFetcher : public HttpFetcher, public OAuth2ApiCallFlow {
 public:
  explicit OAuthHttpFetcher(
      const net::PartialNetworkTrafficAnnotationTag& traffic_annotation,
      const std::string& oauth_scope);
  OAuthHttpFetcher(const OAuthHttpFetcher&) = delete;
  OAuthHttpFetcher& operator=(const OAuthHttpFetcher&) = delete;
  ~OAuthHttpFetcher() override;

  // HttpFetcher::
  void ExecuteGetRequest(const GURL& url,
                         FetchCompleteCallback callback) override;
  void ExecutePostRequest(const GURL& url,
                          const std::string& body,
                          FetchCompleteCallback callback) override;
  void ExecuteDeleteRequest(const GURL& url,
                            FetchCompleteCallback callback) override;

 protected:
  // Reduce the visibility of OAuth2ApiCallFlow::Start() to avoid exposing
  // overloaded methods.
  using OAuth2ApiCallFlow::Start;

  // google_apis::OAuth2ApiCallFlow:
  GURL CreateApiCallUrl() override;
  std::string CreateApiCallBody() override;
  std::string CreateApiCallBodyContentType() override;
  void ProcessApiCallSuccess(const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  void ProcessApiCallFailure(int net_error,
                             const network::mojom::URLResponseHead* head,
                             std::unique_ptr<std::string> body) override;
  net::PartialNetworkTrafficAnnotationTag GetNetworkTrafficAnnotationTag()
      override;
  std::string GetRequestTypeForBody(const std::string& body) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(OAuthHttpFetcherTest,
                           ExecuteGetRequest_MultipleRaceCondition);

  void StartRequest(const GURL& url, FetchCompleteCallback callback);
  void OnAccessTokenFetched(GoogleServiceAuthError error,
                            signin::AccessTokenInfo access_token_info);

  net::PartialNetworkTrafficAnnotationTag traffic_annotation_;
  OAuth2AccessTokenManager::ScopeSet oauth_scopes_;

  bool has_call_started_ = false;
  GURL url_;
  std::string body_;
  RequestType request_type_;
  FetchCompleteCallback callback_;

  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;

  base::WeakPtrFactory<OAuthHttpFetcher> weak_ptr_factory_{this};
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_REPOSITORY_OAUTH_HTTP_FETCHER_H_
