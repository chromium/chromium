// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_redirect/origin_robots_rules.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/subresource_redirect/subresource_redirect_util.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace subresource_redirect {

OriginRobotsRules::FetcherInfo::FetcherInfo(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    OriginRobotsRules::NotifyResponseErrorCallback response_error_callback)
    : url_loader(std::move(url_loader)),
      response_error_callback(std::move(response_error_callback)) {}

OriginRobotsRules::FetcherInfo::~FetcherInfo() = default;

OriginRobotsRules::OriginRobotsRules(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const url::Origin& origin,
    NotifyResponseErrorCallback response_error_callback) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("litepages_robots_rules",
                                          R"(
        semantics {
          sender: "LitePages"
          description:
            "Requests robots.txt rules from the LitePages robots.txt Service "
            "to use in providing data saving optimizations for Chrome."
          trigger:
            "Requested for each unique origin for the images contained in the "
            "page, and cached for certain period."
          data: "A list of allowed and disallowed robots.txt path patterns"
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Lite mode on Android via 'Lite mode' setting."
          chrome_policy {
            DataCompressionProxyEnabled {
              DataCompressionProxyEnabled: false
            }
          }
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetRobotsServerURL(origin);
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  auto url_loader = variations::CreateSimpleURLLoaderWithVariationsHeader(
      std::move(resource_request), variations::InIncognito::kNo,
      variations::SignedIn::kNo, traffic_annotation);

  // url_loader should retry on network changes, but no retries on other
  // failures such as 5xx errors.
  url_loader->SetRetryOptions(
      1 /* max_retries */, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);

  url_loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(&OriginRobotsRules::OnURLLoadComplete,
                     base::Unretained(this)));

  fetcher_info_ = std::make_unique<FetcherInfo>(
      std::move(url_loader), std::move(response_error_callback));
}

OriginRobotsRules::~OriginRobotsRules() = default;

void OriginRobotsRules::GetRobotsRules(RobotsRulesReceivedCallback callback) {
  if (fetcher_info_) {
    // Robots rules fetch is still in progress.
    fetcher_info_->pending_callbacks.push_back(std::move(callback));
    return;
  }
  std::move(callback).Run(robots_rules_);
}

void OriginRobotsRules::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  const auto response_headers =
      fetcher_info_->url_loader->ResponseInfo()
          ? fetcher_info_->url_loader->ResponseInfo()->headers
          : nullptr;
  int response_code = response_headers ? response_headers->response_code() : -1;

  int net_error = fetcher_info_->url_loader->NetError();

  UMA_HISTOGRAM_BOOLEAN(
      "SubresourceRedirect.RobotsRulesFetcher.CacheHit",
      fetcher_info_->url_loader->ResponseInfo()
          ? fetcher_info_->url_loader->ResponseInfo()->was_fetched_via_cache
          : false);
  base::UmaHistogramSparse(
      "SubresourceRedirect.RobotsRulesFetcher.NetErrorCode", -net_error);
  if (response_code != -1) {
    UMA_HISTOGRAM_ENUMERATION(
        "SubresourceRedirect.RobotsRulesFetcher.ResponseCode",
        static_cast<net::HttpStatusCode>(response_code),
        net::HTTP_VERSION_NOT_SUPPORTED);
  }

  // Treat 4xx, 5xx as failures
  if (response_code >= 400 && response_code <= 599) {
    std::string retry_after_string;
    base::TimeDelta retry_after;
    if (response_headers &&
        response_headers->EnumerateHeader(nullptr, "Retry-After",
                                          &retry_after_string) &&
        net::HttpUtil::ParseRetryAfterHeader(retry_after_string,
                                             base::Time::Now(), &retry_after)) {
      std::move(fetcher_info_->response_error_callback)
          .Run(response_code, retry_after);
    } else {
      std::move(fetcher_info_->response_error_callback)
          .Run(response_code, base::TimeDelta());
    }
  }

  if (response_body && net_error == net::OK &&
      (response_code == net::HTTP_OK ||
       response_code == net::HTTP_NOT_MODIFIED)) {
    robots_rules_ = *response_body;
  }
  for (auto& callback : fetcher_info_->pending_callbacks)
    std::move(callback).Run(robots_rules_);
  fetcher_info_.reset();
}

}  // namespace subresource_redirect
