// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_redirect_url_loader_interceptor.h"

#include <stdint.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/base32/base32.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_lite_page_redirect.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "content/public/common/resource_type.h"
#include "crypto/sha2.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/nqe/effective_connection_type.h"

namespace previews {

namespace {

void RecordInterceptAttempt(bool attempted) {
  UMA_HISTOGRAM_BOOLEAN("Previews.ServerLitePage.URLLoader.Attempted",
                        attempted);
}

bool ShouldCreateLoader(const network::ResourceRequest& resource_request) {
  if (!(resource_request.previews_state & content::LITE_PAGE_REDIRECT_ON))
    return false;

  // THese should not be possible but there's some evidence it may be happening
  // in production and can't be repro'd.
  if (resource_request.resource_type !=
      static_cast<int>(content::ResourceType::kMainFrame)) {
    NOTREACHED();
    return false;
  }

  if (!resource_request.url.SchemeIsHTTPOrHTTPS()) {
    NOTREACHED();
    return false;
  }

  if (resource_request.method != "GET") {
    NOTREACHED();
    return false;
  }

  return true;
}

net::HttpRequestHeaders GetChromeProxyHeaders(
    content::BrowserContext* browser_context,
    uint64_t page_id) {
  net::HttpRequestHeaders headers;
  // Return empty headers for unittests.
  if (!browser_context) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            "add-chrome-proxy-header-for-lpr-tests")) {
      headers.SetHeader(data_reduction_proxy::chrome_proxy_header(),
                        "s=secret");
    }
    return headers;
  }

  auto* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);
  if (settings) {
    DCHECK(data_reduction_proxy::params::IsEnabledWithNetworkService());
    std::string header;
    if (settings->GetProxyRequestHeaders().GetHeader(
            data_reduction_proxy::chrome_proxy_header(), &header)) {
      data_reduction_proxy::DataReductionProxyRequestOptions::AddRequestHeader(
          &headers, page_id != 0U ? page_id : 1, header);
    }

    headers.SetHeader(data_reduction_proxy::chrome_proxy_ect_header(),
                      net::GetNameForEffectiveConnectionType(
                          settings->data_reduction_proxy_service()
                              ->GetEffectiveConnectionType()));
  }

  return headers;
}

}  // namespace

void LogLitePageRedirectIneligibleReason(
    LitePageRedirectIneligibleReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Previews.ServerLitePage.IneligibleReasons",
                            reason);
}

bool HandlePreviewsLitePageRedirectURLRewrite(
    GURL* url,
    content::BrowserContext* browser_context) {
  // Don't change the |url|, just register our interest in reversing it before
  // it is displayed to the user in
  // |HandlePreviewsLitePageRedirectURLRewriteReverse|. Without returning true
  // here, |HandlePreviewsLitePageRedirectURLRewriteReverse| would not be
  // called.

  auto* data_reduction_proxy_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);

  if (!data_reduction_proxy_settings)
    return false;

  return data_reduction_proxy_settings->IsDataReductionProxyEnabled() &&
         previews::params::IsLitePageServerPreviewsEnabled();
}

bool HandlePreviewsLitePageRedirectURLRewriteReverse(
    GURL* url,
    content::BrowserContext* browser_context) {
  std::string original_url;
  if (previews::ExtractOriginalURLFromLitePageRedirectURL(*url,
                                                          &original_url)) {
    *url = GURL(original_url);
    return true;
  }
  return false;
}

GURL GetLitePageRedirectURLForURL(const GURL& original_url) {
  DCHECK(original_url.is_valid());
  const std::string experiment_id =
      data_reduction_proxy::params::GetDataSaverServerExperiments();

  std::string experiment_query;
  if (!experiment_id.empty()) {
    experiment_query =
        "&x=" + net::EscapeQueryParamValue(experiment_id, true /* use_plus */);
  }
  std::string fragment;
  if (original_url.has_ref()) {
    fragment = "#" + original_url.ref();
  }

  // Strip out the fragment so that it is not sent to the server.
  std::string origin_hash = base::ToLowerASCII(base32::Base32Encode(
      crypto::SHA256HashString(
          original_url.scheme() + "://" + original_url.host() + ":" +
          base::NumberToString(original_url.EffectiveIntPort())),
      base32::Base32EncodePolicy::OMIT_PADDING));
  GURL previews_host = previews::params::GetLitePagePreviewsDomainURL();
  GURL previews_url = GURL(
      previews_host.scheme() + "://" + origin_hash + "." +
      previews_host.host() +
      (previews_host.has_port() ? (":" + previews_host.port()) : "") + "/p?u=" +
      net::EscapeQueryParamValue(original_url.GetAsReferrer().spec(),
                                 true /* use_plus */) +
      experiment_query + fragment);
  DCHECK(previews_url.is_valid());
  DCHECK_EQ(previews_host.scheme(), previews_url.scheme());
  return previews_url;
}

PreviewsLitePageRedirectURLLoaderInterceptor::
    PreviewsLitePageRedirectURLLoaderInterceptor(
        const scoped_refptr<network::SharedURLLoaderFactory>&
            network_loader_factory,
        uint64_t page_id,
        int frame_tree_node_id)
    : network_loader_factory_(network_loader_factory),
      page_id_(page_id),
      frame_tree_node_id_(frame_tree_node_id) {}

PreviewsLitePageRedirectURLLoaderInterceptor::
    ~PreviewsLitePageRedirectURLLoaderInterceptor() {}

// static
PreviewsUserData::ServerLitePageInfo*
PreviewsLitePageRedirectURLLoaderInterceptor::GetOrCreateServerLitePageInfo(
    content::NavigationHandle* navigation_handle,
    PreviewsLitePageRedirectDecider* decider) {
  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  if (!ui_tab_helper)
    return nullptr;

  previews::PreviewsUserData* previews_data =
      ui_tab_helper->GetPreviewsUserData(navigation_handle);
  if (!previews_data)
    return nullptr;

  if (previews_data->server_lite_page_info()) {
    return previews_data->server_lite_page_info();
  }

  previews_data->set_server_lite_page_info(
      std::make_unique<previews::PreviewsUserData::ServerLitePageInfo>());

  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  base::Optional<std::string> session_id;
  if (drp_settings) {
    session_id = data_reduction_proxy::DataReductionProxyRequestOptions::
        GetSessionKeyFromRequestHeaders(drp_settings->GetProxyRequestHeaders());
  }

  previews::PreviewsUserData::ServerLitePageInfo* info =
      previews_data->server_lite_page_info();
  info->original_navigation_start = navigation_handle->NavigationStart();
  if (session_id.has_value())
    info->drp_session_key = session_id.value();

  const ChromeNavigationUIData* chrome_navigation_ui_data =
      static_cast<const ChromeNavigationUIData*>(
          navigation_handle->GetNavigationUIData());
  if (chrome_navigation_ui_data)
    info->page_id = chrome_navigation_ui_data->data_reduction_proxy_page_id();
  // The page id may not be set in some corner cases (like forward navigation),
  // so make sure it gets set here.
  if (info->page_id == 0U)
    info->page_id = decider->GeneratePageID();

  return info;
}

void PreviewsLitePageRedirectURLLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (serving_url_loader_) {
    RequestHandler handler = serving_url_loader_->ServingResponseHandler();
    // The serving loader manages its own lifetime at this point.
    serving_url_loader_.release();
    std::move(callback).Run(std::move(handler));
    return;
  }

  // Do not attempt to serve the same URL multiple times.
  if (base::Contains(urls_processed_, tentative_resource_request.url)) {
    std::move(callback).Run({});
    return;
  }

  urls_processed_.insert(tentative_resource_request.url);

  // Don't allow direct navigations to LitePageRedirect URLs, except for
  // back/forward navigations. In that case, the navigation history
  std::string original_url;
  if (!IsDisallowedFwdBackNavigation() &&
      previews::ExtractOriginalURLFromLitePageRedirectURL(
          tentative_resource_request.url, &original_url)) {
    // Add the original URL to |urls_processed_| so that we will not retrigger
    // on this navigation. This is used to allow `location.reload()` JavaScript
    // code to load the original page when a preview has been committed.
    urls_processed_.insert(GURL(original_url));
    CreateOriginalURLLoader(tentative_resource_request, GURL(original_url),
                            std::move(callback));
    return;
  }

  if (ShouldCreateLoader(tentative_resource_request)) {
    CreateRedirectLoader(tentative_resource_request, browser_context,
                         std::move(callback));
    return;
  }
  RecordInterceptAttempt(false);
  std::move(callback).Run({});
}

void PreviewsLitePageRedirectURLLoaderInterceptor::CreateRedirectLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RecordInterceptAttempt(true);

  redirect_url_loader_ = std::make_unique<PreviewsLitePageRedirectURLLoader>(
      browser_context, tentative_resource_request,
      base::BindOnce(
          &PreviewsLitePageRedirectURLLoaderInterceptor::HandleRedirectLoader,
          base::Unretained(this), std::move(callback)));

  // |redirect_url_loader_| can be null after this call.
  redirect_url_loader_->StartRedirectToPreview(
      GetChromeProxyHeaders(browser_context, page_id_), network_loader_factory_,
      frame_tree_node_id_);
}

void PreviewsLitePageRedirectURLLoaderInterceptor::CreateOriginalURLLoader(
    const network::ResourceRequest& tentative_resource_request,
    const GURL& original_url,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  redirect_url_loader_ = std::make_unique<PreviewsLitePageRedirectURLLoader>(
      nullptr, tentative_resource_request,
      base::BindOnce(
          &PreviewsLitePageRedirectURLLoaderInterceptor::HandleRedirectLoader,
          base::Unretained(this), std::move(callback)));

  // |redirect_url_loader_| can be null after this call.
  redirect_url_loader_->StartRedirectToOriginalURL(original_url);
}

void PreviewsLitePageRedirectURLLoaderInterceptor::HandleRedirectLoader(
    content::URLLoaderRequestInterceptor::LoaderCallback callback,
    std::unique_ptr<PreviewsLitePageRedirectServingURLLoader>
        serving_url_loader,
    RequestHandler handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Handle any failure by using default loader.
  if (handler.is_null()) {
    DCHECK(!serving_url_loader_);
    redirect_url_loader_.reset();
    std::move(callback).Run({});
    return;
  }

  // Save the serving loader to handle the next request. It can be null when
  // serving the original URL on a reload.
  serving_url_loader_ = std::move(serving_url_loader);

  // |redirect_url_loader_| now manages its own lifetime via a mojo channel.
  // |handler| is guaranteed to be called.
  redirect_url_loader_.release();
  std::move(callback).Run(std::move(handler));
}

bool PreviewsLitePageRedirectURLLoaderInterceptor::
    IsDisallowedFwdBackNavigation() {
  if (!previews::params::LitePageRedirectValidateForwardBackTransition()) {
    return false;
  }

  auto* web_contents =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  if (!web_contents) {
    return false;
  }

  auto* entry = web_contents->GetController().GetPendingEntry();
  return entry &&
         (entry->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK);
}

}  // namespace previews
