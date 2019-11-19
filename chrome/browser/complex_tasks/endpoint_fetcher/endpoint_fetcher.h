// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPLEX_TASKS_ENDPOINT_FETCHER_ENDPOINT_FETCHER_H_
#define CHROME_BROWSER_COMPLEX_TASKS_ENDPOINT_FETCHER_ENDPOINT_FETCHER_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {
struct ResourceRequest;
class SimpleURLLoader;
}  // namespace network

namespace signin {
struct AccessTokenInfo;
class IdentityManager;
}  // namespace signin

class GoogleServiceAuthError;
class GURL;
class Profile;

struct EndpointResponse {
  std::string response;
  // TODO(crbug.com/993393) Add more detailed error messaging
};

using EndpointFetcherCallback =
    base::OnceCallback<void(std::unique_ptr<EndpointResponse>)>;

// EndpointFetcher calls an endpoint and returns the response.
// EndpointFetcher is not thread safe and it is up to the caller
// to wait until the callback function passed to Fetch() completes
// before invoking Fetch() again.
// Destroying an EndpointFetcher will result in the in-flight request being
// cancelled.
// EndpointFetcher performs authentication via the signed in user to
// Chrome.
// If the request times out an empty response will be returned. There will also
// be an error code indicating timeout once more detailed error messaging is
// added TODO(crbug.com/993393).
class EndpointFetcher {
 public:
  // Preferred constructor - forms identity_manager and url_loader_factory.
  EndpointFetcher(Profile* const profile,
                  const std::string& oauth_consumer_name,
                  const GURL& url,
                  const std::string& http_method,
                  const std::string& content_type,
                  const std::vector<std::string>& scopes,
                  int64_t timeout_ms,
                  const std::string& post_data,
                  const net::NetworkTrafficAnnotationTag& annotation_tag);
  // Used for tests. Can be used if caller constructs their own
  // url_loader_factory and identity_manager.
  EndpointFetcher(
      const std::string& oauth_consumer_name,
      const GURL& url,
      const std::string& http_method,
      const std::string& content_type,
      const std::vector<std::string>& scopes,
      int64_t timeout_ms,
      const std::string& post_data,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
      signin::IdentityManager* const identity_manager);

  EndpointFetcher(const EndpointFetcher& endpoint_fetcher) = delete;

  EndpointFetcher& operator=(const EndpointFetcher& endpoint_fetcher) = delete;

  ~EndpointFetcher();

  // TODO(crbug.com/999256) enable cancellation support
  void Fetch(EndpointFetcherCallback callback);

 private:
  void OnAuthTokenFetched(EndpointFetcherCallback callback,
                          GoogleServiceAuthError error,
                          signin::AccessTokenInfo access_token_info);
  void OnResponseFetched(EndpointFetcherCallback callback,
                         std::unique_ptr<std::string> response_body);

  // Members set in constructor to be passed to network::ResourceRequest or
  // network::SimpleURLLoader.
  const std::string oauth_consumer_name_;
  const GURL url_;
  const std::string http_method_;
  const std::string content_type_;
  int64_t timeout_ms_;
  const std::string post_data_;
  const net::NetworkTrafficAnnotationTag annotation_tag_;
  identity::ScopeSet oauth_scopes_;

  // Members set in constructor
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  signin::IdentityManager* const identity_manager_;

  // Members set in Fetch
  std::unique_ptr<const signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  base::WeakPtrFactory<EndpointFetcher> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_COMPLEX_TASKS_ENDPOINT_FETCHER_ENDPOINT_FETCHER_H_
