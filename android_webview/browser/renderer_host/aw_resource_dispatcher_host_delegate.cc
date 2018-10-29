// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"

#include <memory>
#include <string>
#include <utility>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_resource_context.h"
#include "android_webview/browser/aw_safe_browsing_resource_throttle.h"
#include "android_webview/browser/net/aw_web_resource_request.h"
#include "android_webview/browser/renderer_host/auto_login_parser.h"
#include "android_webview/common/url_constants.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/android/safe_browsing_api_handler.h"
#include "components/safe_browsing/features.h"
#include "components/web_restrictions/browser/web_restrictions_resource_throttle.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_status.h"
#include "services/network/public/cpp/features.h"
#include "url/url_constants.h"

using android_webview::AwContentsIoThreadClient;
using android_webview::AwContentsClientBridge;
using android_webview::AwWebResourceRequest;
using content::BrowserThread;
using content::ResourceType;
using content::WebContents;

namespace {

base::LazyInstance<android_webview::AwResourceDispatcherHostDelegate>::
    DestructorAtExit g_webview_resource_dispatcher_host_delegate =
        LAZY_INSTANCE_INITIALIZER;

void SetCacheControlFlag(
    net::URLRequest* request, int flag) {
  const int all_cache_control_flags =
      net::LOAD_BYPASS_CACHE | net::LOAD_VALIDATE_CACHE |
      net::LOAD_SKIP_CACHE_VALIDATION | net::LOAD_ONLY_FROM_CACHE;
  DCHECK_EQ((flag & all_cache_control_flags), flag);
  int load_flags = request->load_flags();
  load_flags &= ~all_cache_control_flags;
  load_flags |= flag;
  request->SetLoadFlags(load_flags);
}

// Called when ResourceDispathcerHost detects a download request.
// The download is already cancelled when this is called, since
// relevant for DownloadListener is already extracted.
void DownloadStartingOnUIThread(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const GURL& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    int64_t content_length) {
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContentsGetter(web_contents_getter);
  if (!client)
    return;
  client->NewDownload(url, user_agent, content_disposition, mime_type,
                      content_length);
}

void NewLoginRequestOnUIThread(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const std::string& realm,
    const std::string& account,
    const std::string& args) {
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContentsGetter(web_contents_getter);
  if (!client)
    return;
  client->NewLoginRequest(realm, account, args);
}

void OnReceivedErrorOnUiThread(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const AwWebResourceRequest& request,
    int error_code,
    bool safebrowsing_hit) {
  AwContentsClientBridge* client =
      AwContentsClientBridge::FromWebContentsGetter(web_contents_getter);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedError dropped for "
                  << request.url;
    return;
  }
  client->OnReceivedError(request, error_code, safebrowsing_hit);
}

}  // namespace

namespace android_webview {

// Calls through the IoThreadClient to check the embedders settings to determine
// if the request should be cancelled. There may not always be an IoThreadClient
// available for the |render_process_id|, |render_frame_id| pair (in the case of
// newly created pop up windows, for example) and in that case the request and
// the client callbacks will be deferred the request until a client is ready.
class IoThreadClientThrottle : public content::ResourceThrottle {
 public:
  IoThreadClientThrottle(int render_process_id,
                         int render_frame_id,
                         net::URLRequest* request);
  ~IoThreadClientThrottle() override;

  // From content::ResourceThrottle
  void WillStartRequest(bool* defer) override;
  void WillRedirectRequest(const net::RedirectInfo& redirect_info,
                           bool* defer) override;
  const char* GetNameForLogging() const override;

  void OnIoThreadClientReady(int new_render_process_id,
                             int new_render_frame_id);
  bool MaybeBlockRequest();
  bool ShouldBlockRequest();
  bool GetSafeBrowsingEnabled();
  int render_process_id() const { return render_process_id_; }
  int render_frame_id() const { return render_frame_id_; }

 private:
  std::unique_ptr<AwContentsIoThreadClient> GetIoThreadClient() const;

  int render_process_id_;
  int render_frame_id_;
  net::URLRequest* request_;
};

IoThreadClientThrottle::IoThreadClientThrottle(int render_process_id,
                                               int render_frame_id,
                                               net::URLRequest* request)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      request_(request) { }

IoThreadClientThrottle::~IoThreadClientThrottle() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  g_webview_resource_dispatcher_host_delegate.Get().
      RemovePendingThrottleOnIoThread(this);
}

const char* IoThreadClientThrottle::GetNameForLogging() const {
  return "IoThreadClientThrottle";
}

std::unique_ptr<AwContentsIoThreadClient>
IoThreadClientThrottle::GetIoThreadClient() const {
  if (content::ResourceRequestInfo::OriginatedFromServiceWorker(request_))
    return AwContentsIoThreadClient::GetServiceWorkerIoThreadClient();

  if (render_process_id_ == -1 || render_frame_id_ == -1) {
    const content::ResourceRequestInfo* resourceRequestInfo =
        content::ResourceRequestInfo::ForRequest(request_);
    if (resourceRequestInfo == nullptr) {
      return nullptr;
    }
    return AwContentsIoThreadClient::FromID(
        resourceRequestInfo->GetFrameTreeNodeId());
  }

  return AwContentsIoThreadClient::FromID(render_process_id_, render_frame_id_);
}

void IoThreadClientThrottle::WillStartRequest(bool* defer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // valid render_frame_id_ implies nonzero render_processs_id_
  DCHECK((render_frame_id_ < 1) || (render_process_id_ != 0));
  *defer = false;

  // Defer all requests of a pop up that is still not associated with Java
  // client so that the client will get a chance to override requests.
  std::unique_ptr<AwContentsIoThreadClient> io_client = GetIoThreadClient();
  if (io_client && io_client->PendingAssociation()) {
    *defer = true;
    AwResourceDispatcherHostDelegate::AddPendingThrottle(
        render_process_id_, render_frame_id_, this);
  } else {
    MaybeBlockRequest();
  }
}

void IoThreadClientThrottle::WillRedirectRequest(
    const net::RedirectInfo& redirect_info,
    bool* defer) {
  WillStartRequest(defer);
}

void IoThreadClientThrottle::OnIoThreadClientReady(int new_render_process_id,
                                                   int new_render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!MaybeBlockRequest()) {
    Resume();
  }
}

bool IoThreadClientThrottle::MaybeBlockRequest() {
  if (ShouldBlockRequest()) {
    CancelWithError(net::ERR_ACCESS_DENIED);
    return true;
  }
  return false;
}

bool IoThreadClientThrottle::GetSafeBrowsingEnabled() {
  std::unique_ptr<AwContentsIoThreadClient> io_client = GetIoThreadClient();
  if (!io_client)
    return false;
  return io_client->GetSafeBrowsingEnabled();
}

bool IoThreadClientThrottle::ShouldBlockRequest() {
  std::unique_ptr<AwContentsIoThreadClient> io_client = GetIoThreadClient();
  if (!io_client)
    return false;

  // Part of implementation of WebSettings.allowContentAccess.
  if (request_->url().SchemeIs(url::kContentScheme) &&
      io_client->ShouldBlockContentUrls()) {
    return true;
  }

  // Part of implementation of WebSettings.allowFileAccess.
  if (request_->url().SchemeIsFile() &&
      io_client->ShouldBlockFileUrls()) {
    // Application's assets and resources are always available.
    return !IsAndroidSpecialFileUrl(request_->url());
  }

  if (io_client->ShouldBlockNetworkLoads()) {
    if (request_->url().SchemeIs(url::kFtpScheme)) {
      return true;
    }
    SetCacheControlFlag(
        request_, net::LOAD_ONLY_FROM_CACHE | net::LOAD_SKIP_CACHE_VALIDATION);
  } else {
    AwContentsIoThreadClient::CacheMode cache_mode = io_client->GetCacheMode();
    switch (cache_mode) {
      case AwContentsIoThreadClient::LOAD_CACHE_ELSE_NETWORK:
        SetCacheControlFlag(request_, net::LOAD_SKIP_CACHE_VALIDATION);
        break;
      case AwContentsIoThreadClient::LOAD_NO_CACHE:
        SetCacheControlFlag(request_, net::LOAD_BYPASS_CACHE);
        break;
      case AwContentsIoThreadClient::LOAD_CACHE_ONLY:
        SetCacheControlFlag(request_, net::LOAD_ONLY_FROM_CACHE |
                                          net::LOAD_SKIP_CACHE_VALIDATION);
        break;
      default:
        break;
    }
  }
  return false;
}

// static
void AwResourceDispatcherHostDelegate::ResourceDispatcherHostCreated() {
  content::ResourceDispatcherHost::Get()->SetDelegate(
      &g_webview_resource_dispatcher_host_delegate.Get());
}

AwResourceDispatcherHostDelegate::AwResourceDispatcherHostDelegate()
    : content::ResourceDispatcherHostDelegate() {}

AwResourceDispatcherHostDelegate::~AwResourceDispatcherHostDelegate() {
}

void AwResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::AppCacheService* appcache_service,
    ResourceType resource_type,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  AddExtraHeadersIfNeeded(request, resource_context);

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);

  std::unique_ptr<IoThreadClientThrottle> ioThreadThrottle =
      std::make_unique<IoThreadClientThrottle>(request_info->GetChildID(),
                                               request_info->GetRenderFrameID(),
                                               request);

  if (ioThreadThrottle->GetSafeBrowsingEnabled()) {
    DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
    if (!base::FeatureList::IsEnabled(
            safe_browsing::kCheckByURLLoaderThrottle)) {
      content::ResourceThrottle* throttle =
          MaybeCreateAwSafeBrowsingResourceThrottle(
              request, resource_type,
              AwBrowserContext::GetDefault()->GetSafeBrowsingDBManager(),
              AwBrowserContext::GetDefault()->GetSafeBrowsingUIManager(),
              AwBrowserContext::GetDefault()
                  ->GetSafeBrowsingWhitelistManager());
      if (throttle == nullptr) {
        // Should not happen
        DLOG(WARNING) << "Failed creating safebrowsing throttle";
      } else {
        throttles->push_back(base::WrapUnique(throttle));
      }
    }
  }

  // We always push the throttles here. Checking the existence of io_client
  // is racy when a popup window is created. That is because RequestBeginning
  // is called whether or not requests are blocked via BlockRequestForRoute()
  // however io_client may or may not be ready at the time depending on whether
  // webcontents is created.
  throttles->push_back(std::move(ioThreadThrottle));

  bool is_main_frame = resource_type == content::RESOURCE_TYPE_MAIN_FRAME;
  throttles->push_back(
      std::make_unique<web_restrictions::WebRestrictionsResourceThrottle>(
          AwBrowserContext::GetDefault()->GetWebRestrictionProvider(),
          request->url(), is_main_frame));
}

void AwResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    network::ResourceResponse* response) {
  AddExtraHeadersIfNeeded(request, resource_context);
}

void AwResourceDispatcherHostDelegate::RequestComplete(
    net::URLRequest* request) {
  if (request && !request->status().is_success()) {
    const content::ResourceRequestInfo* request_info =
        content::ResourceRequestInfo::ForRequest(request);

    bool safebrowsing_hit = false;
    if (IsCancelledBySafeBrowsing(request)) {
      safebrowsing_hit = true;
    }
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&OnReceivedErrorOnUiThread,
                       request_info->GetWebContentsGetterForRequest(),
                       AwWebResourceRequest(*request),
                       request->status().error(), safebrowsing_hit));
  }
}

void AwResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    bool is_content_initiated,
    bool must_download,
    bool is_new_request,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  GURL url(request->url());
  std::string user_agent;
  std::string content_disposition;
  std::string mime_type;
  int64_t content_length = request->GetExpectedContentSize();

  request->extra_request_headers().GetHeader(
      net::HttpRequestHeaders::kUserAgent, &user_agent);

  net::HttpResponseHeaders* response_headers = request->response_headers();
  if (response_headers) {
    response_headers->GetNormalizedHeader("content-disposition",
        &content_disposition);
    response_headers->GetMimeType(&mime_type);
  }

  request->Cancel();

  // POST request cannot be repeated in general, so prevent client from
  // retrying the same request, unless it is with a GET.
  if ("GET" != request->method())
    return;

  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&DownloadStartingOnUIThread,
                     request_info->GetWebContentsGetterForRequest(), url,
                     user_agent, content_disposition, mime_type,
                     content_length));
}

void AwResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    network::ResourceResponse* response) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!request_info) {
    DLOG(FATAL) << "Started request without associated info: " <<
        request->url();
    return;
  }

  if (request_info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME) {
    // Check for x-auto-login header.
    HeaderData header_data;
    if (ParserHeaderInResponse(request, ALLOW_ANY_REALM, &header_data)) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::UI},
          base::BindOnce(&NewLoginRequestOnUIThread,
                         request_info->GetWebContentsGetterForRequest(),
                         header_data.realm, header_data.account,
                         header_data.args));
    }
  }
}

void AwResourceDispatcherHostDelegate::RemovePendingThrottleOnIoThread(
    IoThreadClientThrottle* throttle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PendingThrottleMap::iterator it =
      pending_throttles_.find(content::GlobalFrameRoutingId(
          throttle->render_process_id(), throttle->render_frame_id()));
  if (it != pending_throttles_.end()) {
    pending_throttles_.erase(it);
  }
}

// static
void AwResourceDispatcherHostDelegate::OnIoThreadClientReady(
    int new_render_process_id,
    int new_render_frame_id) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AwResourceDispatcherHostDelegate::OnIoThreadClientReadyInternal,
          base::Unretained(
              g_webview_resource_dispatcher_host_delegate.Pointer()),
          new_render_process_id, new_render_frame_id));
}

// static
void AwResourceDispatcherHostDelegate::AddPendingThrottle(
    int render_process_id,
    int render_frame_id,
    IoThreadClientThrottle* pending_throttle) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &AwResourceDispatcherHostDelegate::AddPendingThrottleOnIoThread,
          base::Unretained(
              g_webview_resource_dispatcher_host_delegate.Pointer()),
          render_process_id, render_frame_id, pending_throttle));
}

void AwResourceDispatcherHostDelegate::AddPendingThrottleOnIoThread(
    int render_process_id,
    int render_frame_id_id,
    IoThreadClientThrottle* pending_throttle) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  pending_throttles_.insert(
      std::pair<content::GlobalFrameRoutingId, IoThreadClientThrottle*>(
          content::GlobalFrameRoutingId(render_process_id, render_frame_id_id),
          pending_throttle));
}

void AwResourceDispatcherHostDelegate::OnIoThreadClientReadyInternal(
    int new_render_process_id,
    int new_render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PendingThrottleMap::iterator it =
      pending_throttles_.find(content::GlobalFrameRoutingId(
          new_render_process_id, new_render_frame_id));

  if (it != pending_throttles_.end()) {
    IoThreadClientThrottle* throttle = it->second;
    throttle->OnIoThreadClientReady(new_render_process_id, new_render_frame_id);
    pending_throttles_.erase(it);
  }
}

void AwResourceDispatcherHostDelegate::AddExtraHeadersIfNeeded(
    net::URLRequest* request,
    content::ResourceContext* resource_context) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(request);
  if (!request_info)
    return;
  if (request_info->GetResourceType() != content::RESOURCE_TYPE_MAIN_FRAME)
    return;

  const ui::PageTransition transition = request_info->GetPageTransition();
  const bool is_load_url =
      transition & ui::PAGE_TRANSITION_FROM_API;
  const bool is_go_back_forward =
      transition & ui::PAGE_TRANSITION_FORWARD_BACK;
  const bool is_reload = ui::PageTransitionCoreTypeIs(
      transition, ui::PAGE_TRANSITION_RELOAD);
  if (is_load_url || is_go_back_forward || is_reload) {
    AwResourceContext* awrc = static_cast<AwResourceContext*>(resource_context);
    std::string extra_headers = awrc->GetExtraHeaders(request->url());
    if (!extra_headers.empty()) {
      net::HttpRequestHeaders headers;
      headers.AddHeadersFromString(extra_headers);
      for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext(); ) {
        request->SetExtraRequestHeaderByName(it.name(), it.value(), false);
      }
    }
  }
}

}  // namespace android_webview
