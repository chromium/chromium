// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/detached_resource_request.h"

#include <cstdlib>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/referrer.h"
#include "net/cookies/site_for_cookies.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/url_request_job.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/origin.h"

namespace customtabs {

namespace {

void RecordParallelRequestHistograms(const std::string& suffix,
                                     int redirects,
                                     base::TimeDelta duration,
                                     int net_error) {
  bool success = net_error == net::OK;
  if (success) {
    // Max 20 redirects, 21 would be a bug.
    base::UmaHistogramCustomCounts(
        "CustomTabs.DetachedResourceRequest.RedirectsCount.Success" + suffix,
        redirects, 1, 21, 21);
    base::UmaHistogramMediumTimes(
        "CustomTabs.DetachedResourceRequest.Duration.Success" + suffix,
        duration);
  } else {
    base::UmaHistogramCustomCounts(
        "CustomTabs.DetachedResourceRequest.RedirectsCount.Failure" + suffix,
        redirects, 1, 21, 21);
    base::UmaHistogramMediumTimes(
        "CustomTabs.DetachedResourceRequest.Duration.Failure" + suffix,
        duration);
  }

  base::UmaHistogramSparse(
      "CustomTabs.DetachedResourceRequest.FinalStatus" + suffix, net_error);
}

}  // namespace

// static
void DetachedResourceRequest::CreateAndStart(
    content::BrowserContext* browser_context,
    const GURL& url,
    const GURL& site_for_referrer,
    const net::ReferrerPolicy referrer_policy,
    Motivation motivation,
    const std::string& package_name,
    DetachedResourceRequest::OnResultCallback cb) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<DetachedResourceRequest> detached_request(
      new DetachedResourceRequest(url, site_for_referrer, referrer_policy,
                                  motivation, package_name, std::move(cb)));
  Start(std::move(detached_request), browser_context);
}

DetachedResourceRequest::~DetachedResourceRequest() = default;

DetachedResourceRequest::DetachedResourceRequest(
    const GURL& url,
    const GURL& site_for_referrer,
    net::ReferrerPolicy referrer_policy,
    Motivation motivation,
    const std::string& package_name,
    DetachedResourceRequest::OnResultCallback cb)
    : url_(url),
      site_for_referrer_(site_for_referrer),
      motivation_(motivation),
      cb_(std::move(cb)),
      redirects_(0) {
  is_from_aga_ = package_name == "com.google.android.googlequicksearchbox";
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("customtabs_parallel_request",
                                          R"(
            semantics {
              sender: "Custom Tabs"
              description:
                "When a URL is opened in Custom Tabs on Android, the calling "
                "app can specify a single parallel request to be made while "
                "the main URL is loading. This allows the calling app to "
                "remove a redirect that would otherwise be needed, improving "
                "performance."
              trigger: "A page is loaded in a Custom Tabs."
              data: "Same as a regular resource request."
              destination: WEBSITE
            }
            policy {
              cookies_allowed: YES
              cookies_store: "user"
              policy_exception_justification: "Identical to a resource fetch."
            })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "GET";
  resource_request->url = url_;
  // The referrer is stripped if it's not set properly initially.
  resource_request->referrer = net::URLRequestJob::ComputeReferrerForPolicy(
      referrer_policy, site_for_referrer_, url_);
  resource_request->referrer_policy = referrer_policy;
  resource_request->site_for_cookies =
      net::SiteForCookies::FromUrl(site_for_referrer_);

  url::Origin site_for_referrer_origin =
      url::Origin::Create(site_for_referrer_);
  resource_request->request_initiator = site_for_referrer_origin;

  // Since `site_for_referrer_` has gone through digital asset links
  // verification, it should be ok to use it to compute the network isolation
  // key.
  resource_request->trusted_params = network::ResourceRequest::TrustedParams();
  resource_request->trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, site_for_referrer_origin,
      site_for_referrer_origin,
      net::SiteForCookies::FromOrigin(site_for_referrer_origin));

  resource_request->resource_type =
      static_cast<int>(blink::mojom::ResourceType::kSubResource);
  resource_request->do_not_prompt_for_login = true;
  resource_request->enable_load_timing = false;

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
}

// static
void DetachedResourceRequest::Start(
    std::unique_ptr<DetachedResourceRequest> request,
    content::BrowserContext* browser_context) {
  request->start_time_ = base::TimeTicks::Now();
  auto* storage_partition = browser_context->GetStoragePartition(nullptr);

  request->url_loader_->SetOnRedirectCallback(
      base::BindRepeating(&DetachedResourceRequest::OnRedirectCallback,
                          base::Unretained(request.get())));

  // Retry for client-side transient failures: DNS resolution errors and network
  // configuration changes. Server HTTP 5xx errors are not retried.
  //
  // This is due to seeing that network changes happen quite a bit in
  // practice. This may be due to these requests happening early in Chrome's
  // lifecycle, so perhaps when the network was otherwise idle before,
  // potentially triggering a network change as a consequence. This is only an
  // hypothesis, but happens in practice, and retrying does help lowering the
  // failure rate.
  //
  // DNS errors are both independent and linked to this. They can happen for a
  // number of reasons, including a network change. Starting with Chrome 81
  // however, a network change happening during DNS resolution is reported as a
  // DNS error, not a network configuration change. This is visible in
  // metrics. As a consequence, retry the request on DNS errors as well. Note
  // that this is harmless, since the request cannot have server-side
  // side-effects if the DNS resolution failed. See crbug.com/1078350 for
  // details.
  int retry_mode = network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE |
                   network::SimpleURLLoader::RETRY_ON_NAME_NOT_RESOLVED;
  request->url_loader_->SetRetryOptions(1 /* max_retries */, retry_mode);

  // |url_loader| is owned by the request, and must be kept alive to not cancel
  // the request. Pass the ownership of the request to the response callback,
  // ensuring that it stays alive, yet is freed upon completion or failure.
  //
  // This is also the reason for this function to be a static member function
  // instead of a regular function.
  network::SimpleURLLoader* const url_loader = request->url_loader_.get();
  url_loader->DownloadToString(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(&DetachedResourceRequest::OnResponseCallback,
                     std::move(request)),
      kMaxResponseSize);
}

void DetachedResourceRequest::OnRedirectCallback(
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  redirects_++;
}

void DetachedResourceRequest::OnResponseCallback(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int net_error = url_loader_->NetError();
  net_error = std::abs(net_error);

  if (motivation_ == Motivation::kParallelRequest) {
    auto duration = base::TimeTicks::Now() - start_time_;
    RecordParallelRequestHistograms("", redirects_, duration, net_error);
    if (is_from_aga_) {
      RecordParallelRequestHistograms(".FromAga", redirects_, duration,
                                      net_error);
    }
  }

  std::move(cb_).Run(net_error);
}

}  // namespace customtabs
