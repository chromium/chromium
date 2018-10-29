// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/data_reduction_proxy_resource_throttle_android.h"

#include "base/logging.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/data_reduction_proxy_util.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::ResourceThrottle;
using safe_browsing::SafeBrowsingService;
using safe_browsing::SafeBrowsingUIManager;
using safe_browsing::SBThreatType;

// TODO(eroman): Downgrade these CHECK()s to DCHECKs once there is more
//               unit test coverage.
// TODO(sgurun) following the comment above, also provide tests for
// checking whether the headers are injected correctly and the SPDY proxy
// origin is tested properly.

namespace {

const char kUnsafeUrlProceedHeader[] = "X-Unsafe-Url-Proceed";

}  // namespace

// static
DataReductionProxyResourceThrottle*
DataReductionProxyResourceThrottle::MaybeCreate(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::ResourceType resource_type,
    SafeBrowsingService* sb_service) {
  if (!IsDataReductionProxyResourceThrottleEnabledForUrl(resource_context,
                                                         request->url()))
    return nullptr;

  return new DataReductionProxyResourceThrottle(request, resource_type,
                                                sb_service);
}

DataReductionProxyResourceThrottle::DataReductionProxyResourceThrottle(
    net::URLRequest* request,
    content::ResourceType resource_type,
    SafeBrowsingService* safe_browsing)
    : state_(STATE_NONE),
      safe_browsing_(safe_browsing),
      request_(request),
      is_subresource_(resource_type != content::RESOURCE_TYPE_MAIN_FRAME),
      is_subframe_(resource_type == content::RESOURCE_TYPE_SUB_FRAME) {
}

DataReductionProxyResourceThrottle::~DataReductionProxyResourceThrottle() { }

void DataReductionProxyResourceThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  CHECK(state_ == STATE_NONE);
  DCHECK(!data_reduction_proxy::params::
             IsIncludedInOnDeviceSafeBrowsingFieldTrial());

  // Save the redirect urls for possible malware detail reporting later.
  redirect_urls_.push_back(redirect_info.new_url);

  // We need to check the new URL before following the redirect.
  SBThreatType threat_type = CheckUrl();
  if (threat_type == safe_browsing::SB_THREAT_TYPE_SAFE)
    return;

  if (request_->load_flags() & net::LOAD_PREFETCH) {
    Cancel();
    return;
  }
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request_);

  state_ = STATE_DISPLAYING_BLOCKING_PAGE;
  security_interstitials::UnsafeResource unsafe_resource;
  unsafe_resource.url = redirect_info.new_url;
  unsafe_resource.original_url = request_->original_url();
  unsafe_resource.redirect_urls = redirect_urls_;
  unsafe_resource.is_subresource = is_subresource_;
  unsafe_resource.is_subframe = is_subframe_;
  unsafe_resource.threat_type = threat_type;
  unsafe_resource.callback = base::Bind(
      &DataReductionProxyResourceThrottle::OnBlockingPageComplete, AsWeakPtr());
  unsafe_resource.callback_thread =
      base::CreateSingleThreadTaskRunnerWithTraits(
          {content::BrowserThread::IO});
  unsafe_resource.web_contents_getter = info->GetWebContentsGetterForRequest();
  unsafe_resource.threat_source = safe_browsing::ThreatSource::DATA_SAVER;

  *defer = true;

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(
          &DataReductionProxyResourceThrottle::StartDisplayingBlockingPage,
          AsWeakPtr(), safe_browsing_->ui_manager(), unsafe_resource));
}

const char* DataReductionProxyResourceThrottle::GetNameForLogging() const {
    return "DataReductionProxyResourceThrottle";
}

// static
void DataReductionProxyResourceThrottle::StartDisplayingBlockingPage(
    const base::WeakPtr<DataReductionProxyResourceThrottle>& throttle,
    scoped_refptr<SafeBrowsingUIManager> ui_manager,
    const security_interstitials::UnsafeResource& resource) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::WebContents* web_contents = resource.web_contents_getter.Run();
  if (web_contents) {
    prerender::PrerenderContents* prerender_contents =
        prerender::PrerenderContents::FromWebContents(web_contents);
    if (prerender_contents) {
      prerender_contents->Destroy(prerender::FINAL_STATUS_SAFE_BROWSING);
      base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::IO},
                               base::Bind(resource.callback, false));
      return;
    }
  }
  ui_manager->DisplayBlockingPage(resource);
}

// SafeBrowsingService::UrlCheckCallback implementation, called on the IO
// thread when the user has decided to proceed with the current request, or
// go back.
void DataReductionProxyResourceThrottle::OnBlockingPageComplete(bool proceed) {
  CHECK(state_ == STATE_DISPLAYING_BLOCKING_PAGE);
  state_ = STATE_NONE;

  if (proceed)
    ResumeRequest();
  else
    Cancel();
}

SBThreatType DataReductionProxyResourceThrottle::CheckUrl() {
  SBThreatType result = safe_browsing::SB_THREAT_TYPE_SAFE;
  DCHECK(!data_reduction_proxy::params::
             IsIncludedInOnDeviceSafeBrowsingFieldTrial());

  // TODO(sgurun) Check for spdy proxy origin.
  if (request_->response_headers() == NULL)
    return result;

  if (request_->response_headers()->HasHeader("X-Phishing-Url"))
    result = safe_browsing::SB_THREAT_TYPE_URL_PHISHING;
  else if (request_->response_headers()->HasHeader("X-Malware-Url"))
    result = safe_browsing::SB_THREAT_TYPE_URL_MALWARE;

  // If safe browsing is disabled and the request is sent to the DRP server,
  // we need to break the redirect loop by setting the extra header.
  if (result != safe_browsing::SB_THREAT_TYPE_SAFE &&
      !safe_browsing_->enabled()) {
    request_->SetExtraRequestHeaderByName(kUnsafeUrlProceedHeader, "1", true);
    result = safe_browsing::SB_THREAT_TYPE_SAFE;
  }

  return result;
}

void DataReductionProxyResourceThrottle::ResumeRequest() {
  CHECK(state_ == STATE_NONE);

  // Inject the header before resuming the request.
  request_->SetExtraRequestHeaderByName(kUnsafeUrlProceedHeader, "1", true);
  Resume();
}
