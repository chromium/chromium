// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_redirect_url_loader.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/previews/previews_lite_page_redirect_url_loader_interceptor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/previews_state.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_util.h"
#include "services/network/public/cpp/resource_request.h"

namespace previews {

namespace {
// Used for mojo pipe size. Same constant as navigation code.
constexpr size_t kRedirectDefaultAllocationSize = 512 * 1024;
}  // namespace

PreviewsLitePageRedirectURLLoader::PreviewsLitePageRedirectURLLoader(
    content::BrowserContext* browser_context,
    const network::ResourceRequest& tentative_resource_request,
    HandleRequest callback)
    : modified_resource_request_(tentative_resource_request),
      callback_(std::move(callback)),
      binding_(this),
      origin_probe_finished_successfully_(false),
      litepage_request_finished_successfully_(false) {
  pref_service_ = browser_context
                      ? Profile::FromBrowserContext(browser_context)->GetPrefs()
                      : nullptr;
}

PreviewsLitePageRedirectURLLoader::~PreviewsLitePageRedirectURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PreviewsLitePageRedirectURLLoader::OnOriginProbeComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It is safe to delete the prober during this callback, so do so because it
  // is an expensive object to keep around.
  origin_connectivity_prober_.reset();

  if (success) {
    origin_probe_finished_successfully_ = true;
    MaybeCallOnLitePageSuccess();
    return;
  }
  OnLitePageFallback();
}

void PreviewsLitePageRedirectURLLoader::StartOriginProbe(
    const GURL& original_url,
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory) {
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("previews_litepage_origin_prober", R"(
        semantics {
          sender: "Previews Litepage Origin Prober"
          description:
            "Sends a HEAD request to the origin that the user is navigating to "
            "in order to establish network connectivity before attempting a "
            "preview of that site."
          trigger:
            "Requested on preview-eligible navigations when Lite mode and "
            "Previews are enabled and the network is slow."
          data: "None."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Lite mode on Android via the settings menu. "
            "Lite mode is not available on iOS, and on desktop only for "
            "developer testing."
          policy_exception_justification: "Not implemented."
        })");

  // This probe is a single chance with a short timeout because it blocks the
  // navigation.
  AvailabilityProber::TimeoutPolicy timeout_policy;
  timeout_policy.base_timeout =
      previews::params::LitePageRedirectPreviewOriginProbeTimeout();

  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.max_retries = 0;

  origin_connectivity_prober_ = std::make_unique<AvailabilityProber>(
      this, network_loader_factory, pref_service_,
      AvailabilityProber::ClientName::kLitepagesOriginCheck,
      original_url.GetOrigin(), AvailabilityProber::HttpMethod::kHead,
      net::HttpRequestHeaders(), retry_policy, timeout_policy,
      traffic_annotation, 10 /* max_cache_entries */,
      base::TimeDelta::FromHours(24) /* revalidate_cache_after */);
  origin_connectivity_prober_->SetOnCompleteCallback(base::BindRepeating(
      &PreviewsLitePageRedirectURLLoader::OnOriginProbeComplete,
      weak_ptr_factory_.GetWeakPtr()));
  origin_connectivity_prober_->SendNowIfInactive(
      false /* send_only_in_foreground */);
}

bool PreviewsLitePageRedirectURLLoader::ShouldSendNextProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return true;
}

bool PreviewsLitePageRedirectURLLoader::IsResponseSuccess(
    net::Error net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Any HTTP response is fine, so long as we got it.
  return net_error == net::OK && head && head->headers;
}

void PreviewsLitePageRedirectURLLoader::StartRedirectToPreview(
    const net::HttpRequestHeaders& chrome_proxy_headers,
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory,
    int frame_tree_node_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(data_reduction_proxy::DataReductionProxyRequestOptions::
             GetSessionKeyFromRequestHeaders(chrome_proxy_headers)
                 .has_value());

  std::string original_url_str;
  GURL original_url;
  GURL lite_page_url;

  if (previews::ExtractOriginalURLFromLitePageRedirectURL(
          modified_resource_request_.url, &original_url_str)) {
    // We are navigating directly to a lite pages URL. This can happen for
    // forward/back navigations.
    original_url = GURL(original_url_str);
    lite_page_url = modified_resource_request_.url;
  } else {
    // We are navigating to an origin URL, which needs to be redirected to a
    // lite pages URL.
    original_url = modified_resource_request_.url;
    lite_page_url = GetLitePageRedirectURLForURL(original_url);
  }

  CreateRedirectInformation(lite_page_url);

  modified_resource_request_.headers.MergeFrom(chrome_proxy_headers);

  if (previews::params::LitePageRedirectShouldProbeOrigin()) {
    StartOriginProbe(original_url, network_loader_factory);
  } else {
    origin_probe_finished_successfully_ = true;
  }

  serving_url_loader_ =
      std::make_unique<PreviewsLitePageRedirectServingURLLoader>(
          base::BindOnce(&PreviewsLitePageRedirectURLLoader::OnResultDetermined,
                         weak_ptr_factory_.GetWeakPtr()));
  // |serving_url_loader_| can be null after this call.
  serving_url_loader_->StartNetworkRequest(
      modified_resource_request_, network_loader_factory, frame_tree_node_id);
}

void PreviewsLitePageRedirectURLLoader::StartRedirectToOriginalURL(
    const GURL& original_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CreateRedirectInformation(original_url);

  std::move(callback_).Run(
      nullptr, base::BindOnce(&PreviewsLitePageRedirectURLLoader::
                                  StartHandlingRedirectToModifiedRequest,
                              weak_ptr_factory_.GetWeakPtr()));
}

void PreviewsLitePageRedirectURLLoader::CreateRedirectInformation(
    const GURL& redirect_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool insecure_scheme_was_upgraded = false;
  bool copy_fragment = true;

  redirect_info_ = net::RedirectInfo::ComputeRedirectInfo(
      modified_resource_request_.method, modified_resource_request_.url,
      modified_resource_request_.request_initiator,
      modified_resource_request_.site_for_cookies,
      net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT,
      modified_resource_request_.referrer_policy,
      modified_resource_request_.referrer.spec(), net::HTTP_TEMPORARY_REDIRECT,
      redirect_url, base::nullopt, insecure_scheme_was_upgraded, copy_fragment);

  bool should_clear_upload = false;
  net::RedirectUtil::UpdateHttpRequest(
      modified_resource_request_.url, modified_resource_request_.method,
      redirect_info_, base::nullopt, base::nullopt,
      &modified_resource_request_.headers, &should_clear_upload);

  DCHECK(!should_clear_upload);

  modified_resource_request_.url = redirect_info_.new_url;
  modified_resource_request_.method = redirect_info_.new_method;
  modified_resource_request_.site_for_cookies =
      redirect_info_.new_site_for_cookies;
  modified_resource_request_.referrer = GURL(redirect_info_.new_referrer);
  modified_resource_request_.referrer_policy =
      redirect_info_.new_referrer_policy;
}

void PreviewsLitePageRedirectURLLoader::OnResultDetermined(
    ServingLoaderResult result,
    base::Optional<net::RedirectInfo> redirect_info,
    scoped_refptr<network::ResourceResponse> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!redirect_info || result == ServingLoaderResult::kRedirect);
  DCHECK(!response || result == ServingLoaderResult::kRedirect);

  switch (result) {
    case ServingLoaderResult::kSuccess:
      litepage_request_finished_successfully_ = true;
      MaybeCallOnLitePageSuccess();
      return;
    case ServingLoaderResult::kFallback:
      OnLitePageFallback();
      return;
    case ServingLoaderResult::kRedirect:
      OnLitePageRedirect(redirect_info.value(), response->head);
      return;
  }
  NOTREACHED();
}

void PreviewsLitePageRedirectURLLoader::MaybeCallOnLitePageSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callback_)
    return;

  if (!origin_probe_finished_successfully_ ||
      !litepage_request_finished_successfully_) {
    return;
  }

  OnLitePageSuccess();
}

void PreviewsLitePageRedirectURLLoader::OnLitePageSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(origin_probe_finished_successfully_);
  DCHECK(litepage_request_finished_successfully_);

  std::move(callback_).Run(
      std::move(serving_url_loader_),
      base::BindOnce(&PreviewsLitePageRedirectURLLoader::
                         StartHandlingRedirectToModifiedRequest,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PreviewsLitePageRedirectURLLoader::OnLitePageRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& response_head) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  redirect_info_ = redirect_info;

  response_head_ = response_head;

  std::move(callback_).Run(
      nullptr,
      base::BindOnce(&PreviewsLitePageRedirectURLLoader::StartHandlingRedirect,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PreviewsLitePageRedirectURLLoader::OnLitePageFallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callback_)
    std::move(callback_).Run(nullptr, {});
}

void PreviewsLitePageRedirectURLLoader::StartHandlingRedirectToModifiedRequest(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  response_head_.request_start = base::TimeTicks::Now();
  response_head_.response_start = response_head_.request_start;

  std::string header_string = base::StringPrintf(
      "HTTP/1.1 %i Temporary Redirect\n"
      "Location: %s\n",
      net::HTTP_TEMPORARY_REDIRECT,
      modified_resource_request_.url.spec().c_str());

  response_head_.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(header_string));
  response_head_.encoded_data_length = 0;

  StartHandlingRedirect(resource_request, std::move(receiver),
                        std::move(client));
}

void PreviewsLitePageRedirectURLLoader::StartHandlingRedirect(
    const network::ResourceRequest& /* resource_request */,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(receiver));
  binding_.set_connection_error_handler(
      base::BindOnce(&PreviewsLitePageRedirectURLLoader::OnConnectionClosed,
                     weak_ptr_factory_.GetWeakPtr()));
  client_.Bind(std::move(client));

  mojo::DataPipe pipe(kRedirectDefaultAllocationSize);
  if (!pipe.consumer_handle.is_valid()) {
    delete this;
    return;
  }

  client_->OnReceiveRedirect(redirect_info_, response_head_);
}

void PreviewsLitePageRedirectURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Content should not hang onto old URLLoaders for redirects.
  NOTREACHED();
}

void PreviewsLitePageRedirectURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  serving_url_loader_->SetPriority(priority, intra_priority_value);
}

void PreviewsLitePageRedirectURLLoader::PauseReadingBodyFromNet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  serving_url_loader_->PauseReadingBodyFromNet();
}

void PreviewsLitePageRedirectURLLoader::ResumeReadingBodyFromNet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  serving_url_loader_->ResumeReadingBodyFromNet();
}

void PreviewsLitePageRedirectURLLoader::OnConnectionClosed() {
  // This happens when content cancels the navigation. Close the network request
  // and client handle and destroy |this|.
  binding_.Close();
  client_.reset();
  delete this;
}

}  // namespace previews
