// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_refresh_cookie_fetcher_impl.h"

#include <memory>

#include "components/signin/public/base/signin_client.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

BoundSessionRefreshCookieFetcherImpl::BoundSessionRefreshCookieFetcherImpl(
    SigninClient* client)
    : client_(client), url_loader_factory_(client->GetURLLoaderFactory()) {}

BoundSessionRefreshCookieFetcherImpl::~BoundSessionRefreshCookieFetcherImpl() =
    default;

void BoundSessionRefreshCookieFetcherImpl::Start(
    RefreshCookieCompleteCallback callback) {
  CHECK(!callback_);
  CHECK(callback);
  callback_ = std::move(callback);
  client_->DelayNetworkCall(
      base::BindOnce(&BoundSessionRefreshCookieFetcherImpl::StartRefreshRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BoundSessionRefreshCookieFetcherImpl::StartRefreshRequest() {
  // TODO(b/273920907): Update the `traffic_annotation` setting once a mechanism
  // allowing the user to disable the feature is implemented.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gaia_auth_rotate_bound_cookies",
                                          R"(
        semantics {
          sender: "Chrome - Google authentication API"
          description:
            "This request is used to rotate bound Google authentication "
            "cookies."
          trigger:
            "This request is triggered in a bound session when the bound Google"
            " authentication cookies are soon to expire."
          user_data {
            type: ACCESS_TOKEN
          }
          data: "Request includes cookies and a signed token proving that a"
                " request comes from the same device as was registered before."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
                email: "chrome-signin-team@google.com"
            }
          }
          last_reviewed: "2023-05-09"
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user"
          setting:
            "This feature is under development and must be enabled by user"
            " action."
          policy_exception_justification:
            "Not implemented. "
            "If the feature is on, this request must be made to ensure the user"
            " maintains their signed in status on the web for Google owned"
            " domains."
        })");

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = GaiaUrls::GetInstance()->rotate_bound_cookies_url();
  request->method = "GET";

  url::Origin origin = GaiaUrls::GetInstance()->gaia_origin();
  request->site_for_cookies = net::SiteForCookies::FromOrigin(origin);
  request->trusted_params = network::ResourceRequest::TrustedParams();
  request->trusted_params->isolation_info =
      net::IsolationInfo::CreateForInternalRequest(origin);

  // TODO(b/273920907): Figure out how to handle redirects. Currently
  // `network::SimpleURLLoader::SetOnRedirectCallback()` doesn't support
  // modifying the headers nor asynchronously resuming the reguest.
  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);
  url_loader_->SetRetryOptions(
      3, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  // TODO(b/273920907): Download the response body to support in refresh DBSC
  // instructions update.
  // `base::Unretained(this)` is safe as `this` owns `url_loader_`.
  url_loader_->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&BoundSessionRefreshCookieFetcherImpl::OnURLLoaderComplete,
                     base::Unretained(this)));
}

void BoundSessionRefreshCookieFetcherImpl::OnURLLoaderComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  net::Error net_error = static_cast<net::Error>(url_loader_->NetError());

  std::move(callback_).Run(
      Result(net_error, headers ? absl::optional<int>(headers->response_code())
                                : absl::nullopt));
}
