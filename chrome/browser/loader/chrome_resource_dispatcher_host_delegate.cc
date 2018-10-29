// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/chrome_resource_dispatcher_host_delegate.h"

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/component_updater/component_updater_resource_throttle.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_resource_throttle.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/loader/safe_browsing_resource_throttle.h"
#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/plugins/plugin_utils.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_resource_throttle.h"
#include "chrome/browser/prerender/prerender_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/google/core/common/google_util.h"
#include "components/nacl/common/buildflags.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/offline_pages/core/request_header/offline_page_navigation_ui_data.h"
#include "components/policy/content/policy_blacklist_navigation_throttle.h"
#include "components/safe_browsing/features.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/plugin_service_filter.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/stream_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "extensions/buildflags/buildflags.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_endpoint.h"
#include "net/base/load_flags.h"
#include "net/base/load_timing_info.h"
#include "net/base/request_priority.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/component_updater/pnacl_component_installer.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/user_script.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/downloads/resource_throttle.h"
#include "chrome/browser/offline_pages/offliner_user_data.h"
#include "chrome/browser/offline_pages/resource_loading_observer.h"
#endif

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/intercept_download_resource_throttle.h"
#include "chrome/browser/loader/data_reduction_proxy_resource_throttle_android.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/signin/merge_session_resource_throttle.h"
#include "chrome/browser/chromeos/login/signin/merge_session_throttling_utils.h"
#endif

using content::BrowserThread;
using content::LoginDelegate;
using content::RenderViewHost;
using content::ResourceRequestInfo;
using content::ResourceType;

#if BUILDFLAG(ENABLE_EXTENSIONS)
using extensions::Extension;
using extensions::StreamsPrivateAPI;
#endif

namespace {

prerender::PrerenderManager* GetPrerenderManager(
    content::WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!web_contents)
    return NULL;

  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return NULL;

  return prerender::PrerenderManagerFactory::GetForBrowserContext(
      browser_context);
}

void UpdatePrerenderNetworkBytesCallback(content::WebContents* web_contents,
                                         int64_t bytes) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // PrerenderContents::FromWebContents handles the NULL case.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);

  if (prerender_contents)
    prerender_contents->AddNetworkBytes(bytes);

  prerender::PrerenderManager* prerender_manager =
      GetPrerenderManager(web_contents);
  if (prerender_manager)
    prerender_manager->AddProfileNetworkBytesIfEnabled(bytes);
}

#if BUILDFLAG(ENABLE_NACL)
void AppendComponentUpdaterThrottles(
    net::URLRequest* request,
    const ResourceRequestInfo& info,
    content::ResourceContext* resource_context,
    ResourceType resource_type,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  if (info.IsPrerendering())
    return;

  const char* crx_id = NULL;
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  if (!cus)
    return;
  // Check for PNaCl pexe request.
  if (resource_type == content::RESOURCE_TYPE_OBJECT) {
    const net::HttpRequestHeaders& headers = request->extra_request_headers();
    std::string accept_headers;
    if (headers.GetHeader("Accept", &accept_headers)) {
      if (accept_headers.find("application/x-pnacl") != std::string::npos &&
          pnacl::NeedsOnDemandUpdate())
        crx_id = "hnimpnehoodheedghdeeijklkeaacbdc";
    }
  }

  if (crx_id) {
    // We got a component we need to install, so throttle the resource
    // until the component is installed.
    throttles->push_back(base::WrapUnique(
        component_updater::GetOnDemandResourceThrottle(cus, crx_id)));
  }
}
#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
// Translate content::ResourceType to a type to use for Offliners.
offline_pages::ResourceLoadingObserver::ResourceDataType
ConvertResourceTypeToResourceDataType(content::ResourceType type) {
  switch (type) {
    case content::RESOURCE_TYPE_STYLESHEET:
      return offline_pages::ResourceLoadingObserver::ResourceDataType::TEXT_CSS;
    case content::RESOURCE_TYPE_IMAGE:
      return offline_pages::ResourceLoadingObserver::ResourceDataType::IMAGE;
    case content::RESOURCE_TYPE_XHR:
      return offline_pages::ResourceLoadingObserver::ResourceDataType::XHR;
    default:
      return offline_pages::ResourceLoadingObserver::ResourceDataType::OTHER;
  }
}

void NotifyUIThreadOfRequestStarted(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // TODO(petewil) We're not sure why yet, but we do sometimes see that
  // web_contents_getter returning null.  Until we find out why, avoid crashing.
  // crbug.com/742370
  if (web_contents_getter.is_null())
    return;

  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  // If we are producing an offline version of the page, track resource loading.
  offline_pages::ResourceLoadingObserver* resource_tracker =
      offline_pages::OfflinerUserData::ResourceLoadingObserverFromWebContents(
          web_contents);
  if (resource_tracker) {
    offline_pages::ResourceLoadingObserver::ResourceDataType data_type =
        ConvertResourceTypeToResourceDataType(resource_type);
    resource_tracker->ObserveResourceLoading(data_type, true /* STARTED */);
  }
}
#endif

void NotifyUIThreadOfRequestComplete(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const content::ResourceRequestInfo::FrameTreeNodeIdGetter&
        frame_tree_node_id_getter,
    const GURL& url,
    const net::HostPortPair& host_port_pair,
    const content::GlobalRequestID& request_id,
    int render_process_id,
    int render_frame_id,
    ResourceType resource_type,
    bool is_download,
    bool was_cached,
    std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
        data_reduction_proxy_data,
    int net_error,
    int64_t total_received_bytes,
    int64_t raw_body_bytes,
    int64_t original_content_length,
    base::TimeTicks request_creation_time,
    std::unique_ptr<net::LoadTimingInfo> load_timing_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::WebContents* web_contents = web_contents_getter.Run();
  if (!web_contents)
    return;

  if (!was_cached) {
    UpdatePrerenderNetworkBytesCallback(web_contents, total_received_bytes);
  }

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // If we are producing an offline version of the page, track resource loading.
  offline_pages::ResourceLoadingObserver* resource_tracker =
      offline_pages::OfflinerUserData::ResourceLoadingObserverFromWebContents(
          web_contents);
  if (resource_tracker) {
    offline_pages::ResourceLoadingObserver::ResourceDataType data_type =
        ConvertResourceTypeToResourceDataType(resource_type);
    resource_tracker->ObserveResourceLoading(data_type, false /* COMPLETED */);
    if (!was_cached)
      resource_tracker->OnNetworkBytesChanged(total_received_bytes);
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  if (!is_download) {
    page_load_metrics::MetricsWebContentsObserver* metrics_observer =
        page_load_metrics::MetricsWebContentsObserver::FromWebContents(
            web_contents);
    if (metrics_observer) {
      // Will be null for main or sub frame resources, when browser-side
      // navigation is enabled.
      content::RenderFrameHost* render_frame_host_or_null =
          content::RenderFrameHost::FromID(render_process_id, render_frame_id);
      metrics_observer->OnRequestComplete(
          url, host_port_pair, frame_tree_node_id_getter.Run(), request_id,
          render_frame_host_or_null, resource_type, was_cached,
          std::move(data_reduction_proxy_data), raw_body_bytes,
          original_content_length, request_creation_time, net_error,
          std::move(load_timing_info));
    }
  }
}

}  // namespace

ChromeResourceDispatcherHostDelegate::ChromeResourceDispatcherHostDelegate()
    : download_request_limiter_(g_browser_process->download_request_limiter()),
      safe_browsing_(g_browser_process->safe_browsing_service()) {}

ChromeResourceDispatcherHostDelegate::~ChromeResourceDispatcherHostDelegate() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  CHECK(stream_target_info_.empty());
#endif
}

void ChromeResourceDispatcherHostDelegate::RequestBeginning(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    content::AppCacheService* appcache_service,
    ResourceType resource_type,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  if (safe_browsing_.get())
    safe_browsing_->OnResourceRequest(request);
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

#if BUILDFLAG(ENABLE_OFFLINE_PAGES) || BUILDFLAG(ENABLE_NACL)
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // TODO(petewil): Unify the safe browsing request and the metrics observer
  // request if possible so we only have to cross to the main thread once.
  // http://crbug.com/712312.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&NotifyUIThreadOfRequestStarted,
                     info->GetWebContentsGetterForRequest(),
                     info->GetResourceType()));
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

#if defined(OS_CHROMEOS)
  // Check if we need to add merge session throttle. This throttle will postpone
  // loading of XHR requests.
  if (resource_type == content::RESOURCE_TYPE_XHR) {
    // Add interstitial page while merge session process (cookie
    // reconstruction from OAuth2 refresh token in ChromeOS login) is still in
    // progress while we are attempting to load a google property.
    if (!merge_session_throttling_utils::AreAllSessionMergedAlready() &&
        request->url().SchemeIsHTTPOrHTTPS()) {
      throttles->push_back(
          std::make_unique<MergeSessionResourceThrottle>(request));
    }
  }
#endif

  // Don't attempt to append headers to requests that have already started.
  // TODO(stevet): Remove this once the request ordering issues are resolved
  // in crbug.com/128048.
  if (!request->is_pending()) {
    net::HttpRequestHeaders headers;
    headers.CopyFrom(request->extra_request_headers());
    bool is_off_the_record = io_data->IsOffTheRecord();
    bool is_signed_in =
        !is_off_the_record &&
        !io_data->google_services_account_id()->GetValue().empty();
    variations::AppendVariationHeaders(
        request->url(),
        is_off_the_record ? variations::InIncognito::kYes
                          : variations::InIncognito::kNo,
        is_signed_in ? variations::SignedIn::kYes : variations::SignedIn::kNo,
        &headers);
    request->SetExtraRequestHeaders(headers);
  }

  signin::ChromeRequestAdapter signin_request_adapter(request);
  signin::FixAccountConsistencyRequestHeader(
      &signin_request_adapter, GURL() /* redirect_url */, io_data);

  AppendStandardResourceThrottles(request,
                                  resource_context,
                                  resource_type,
                                  throttles);
#if BUILDFLAG(ENABLE_NACL)
  AppendComponentUpdaterThrottles(request, *info, resource_context,
                                  resource_type, throttles);
#endif  // BUILDFLAG(ENABLE_NACL)
}

void ChromeResourceDispatcherHostDelegate::DownloadStarting(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    bool is_content_initiated,
    bool must_download,
    bool is_new_request,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  const content::ResourceRequestInfo* info =
        content::ResourceRequestInfo::ForRequest(request);
  // If it's from the web, we don't trust it, so we push the throttle on.
  if (is_content_initiated) {
    throttles->push_back(std::make_unique<DownloadResourceThrottle>(
        download_request_limiter_, info->GetWebContentsGetterForRequest(),
        request->url(), request->method()));
  }

  // If this isn't a new request, the standard resource throttles have already
  // been added, so no need to add them again.
  if (is_new_request) {
    AppendStandardResourceThrottles(request,
                                    resource_context,
                                    content::RESOURCE_TYPE_MAIN_FRAME,
                                    throttles);
#if defined(OS_ANDROID)
    // On Android, forward text/html downloads to OfflinePages backend.
    throttles->push_back(
        std::make_unique<offline_pages::downloads::ResourceThrottle>(request));
#endif
  }

#if defined(OS_ANDROID)
  // Add the InterceptDownloadResourceThrottle after calling
  // AppendStandardResourceThrottles so the download will not bypass
  // safebrowsing checks.
  if (is_content_initiated) {
    throttles->push_back(std::make_unique<InterceptDownloadResourceThrottle>(
        request, info->GetWebContentsGetterForRequest()));
  }
#endif
}

void ChromeResourceDispatcherHostDelegate::AppendStandardResourceThrottles(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    ResourceType resource_type,
    std::vector<std::unique_ptr<content::ResourceThrottle>>* throttles) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

  // Insert either safe browsing or data reduction proxy throttle at the front
  // of the list, so one of them gets to decide if the resource is safe.
  content::ResourceThrottle* first_throttle = NULL;
#if defined(OS_ANDROID)
  first_throttle = DataReductionProxyResourceThrottle::MaybeCreate(
      request, resource_context, resource_type, safe_browsing_.get());
#endif  // defined(OS_ANDROID)

#if defined(SAFE_BROWSING_DB_LOCAL) || defined(SAFE_BROWSING_DB_REMOTE)
  if (!first_throttle && io_data->safe_browsing_enabled()->GetValue() &&
      !base::FeatureList::IsEnabled(safe_browsing::kCheckByURLLoaderThrottle)) {
    first_throttle = MaybeCreateSafeBrowsingResourceThrottle(
        request, resource_type, safe_browsing_.get(), io_data);
  }
#endif  // defined(SAFE_BROWSING_DB_LOCAL) || defined(SAFE_BROWSING_DB_REMOTE)

  if (first_throttle)
    throttles->push_back(base::WrapUnique(first_throttle));

  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  if (info->IsPrerendering()) {
    // TODO(jam): remove this throttle once http://crbug.com/740130 is fixed and
    // PrerendererURLLoaderThrottle can be used for frame requests in the
    // network-service-disabled mode.
    if (!base::FeatureList::IsEnabled(network::features::kNetworkService) &&
        content::IsResourceTypeFrame(info->GetResourceType())) {
      throttles->push_back(
          std::make_unique<prerender::PrerenderResourceThrottle>(request));
    }
  }
}

bool ChromeResourceDispatcherHostDelegate::ShouldInterceptResourceAsStream(
    net::URLRequest* request,
    const std::string& mime_type,
    GURL* origin,
    std::string* payload) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  std::string extension_id =
      PluginUtils::GetExtensionIdForMimeType(info->GetContext(), mime_type);
  if (!extension_id.empty()) {
    StreamTargetInfo target_info;
    *origin = Extension::GetBaseURLFromExtensionId(extension_id);
    target_info.extension_id = extension_id;
    target_info.view_id = base::GenerateGUID();
    *payload = target_info.view_id;
    stream_target_info_[request] = target_info;
    return true;
  }
#endif
  return false;
}

void ChromeResourceDispatcherHostDelegate::OnStreamCreated(
    net::URLRequest* request,
    std::unique_ptr<content::StreamInfo> stream) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);
  auto ix = stream_target_info_.find(request);
  CHECK(ix != stream_target_info_.end());
  bool embedded = info->GetResourceType() != content::RESOURCE_TYPE_MAIN_FRAME;
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &extensions::StreamsPrivateAPI::SendExecuteMimeTypeHandlerEvent,
          ix->second.extension_id, ix->second.view_id, embedded,
          info->GetFrameTreeNodeId(), info->GetChildID(),
          info->GetRenderFrameID(), std::move(stream),
          nullptr /* transferrable_loader */, GURL()));
  stream_target_info_.erase(request);
#endif
}

void ChromeResourceDispatcherHostDelegate::OnResponseStarted(
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    network::ResourceResponse* response) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

  signin::ResponseAdapter signin_response_adapter(request);
  signin::ProcessAccountConsistencyResponseHeaders(
      &signin_response_adapter, GURL(), io_data->IsOffTheRecord());

  // Built-in additional protection for the chrome web store origin.
#if BUILDFLAG(ENABLE_EXTENSIONS)
  GURL webstore_url(extension_urls::GetWebstoreLaunchURL());
  if (request->url().SchemeIsHTTPOrHTTPS() &&
      request->url().DomainIs(webstore_url.host_piece())) {
    net::HttpResponseHeaders* response_headers = request->response_headers();
    if (response_headers &&
        !response_headers->HasHeaderValue("x-frame-options", "deny") &&
        !response_headers->HasHeaderValue("x-frame-options", "sameorigin")) {
      response_headers->RemoveHeader("x-frame-options");
      response_headers->AddHeader("x-frame-options: sameorigin");
    }
  }
#endif
}

void ChromeResourceDispatcherHostDelegate::OnRequestRedirected(
    const GURL& redirect_url,
    net::URLRequest* request,
    content::ResourceContext* resource_context,
    network::ResourceResponse* response) {
  ProfileIOData* io_data = ProfileIOData::FromResourceContext(resource_context);

  // Chrome tries to ensure that the identity is consistent between Chrome and
  // the content area.
  //
  // For example, on Android, for users that are signed in to Chrome, the
  // identity is mirrored into the content area. To do so, Chrome appends a
  // X-Chrome-Connected header to all Gaia requests from a connected profile so
  // Gaia could return a 204 response and let Chrome handle the action with
  // native UI.
  signin::ChromeRequestAdapter signin_request_adapter(request);
  signin::FixAccountConsistencyRequestHeader(&signin_request_adapter,
                                             redirect_url, io_data);
  signin::ResponseAdapter signin_response_adapter(request);
  signin::ProcessAccountConsistencyResponseHeaders(
      &signin_response_adapter, redirect_url, io_data->IsOffTheRecord());
}

// Notification that a request has completed.
void ChromeResourceDispatcherHostDelegate::RequestComplete(
    net::URLRequest* url_request) {
  if (!url_request)
    return;
  // TODO(maksims): remove this and use net_error argument in RequestComplete
  // once ResourceDispatcherHostDelegate is modified.
  int net_error = url_request->status().error();
  const ResourceRequestInfo* info =
      ResourceRequestInfo::ForRequest(url_request);

  ProfileIOData* io_data =
      ProfileIOData::FromResourceContext(info->GetContext());
  data_reduction_proxy::DataReductionProxyIOData* data_reduction_proxy_io_data =
      io_data->data_reduction_proxy_io_data();
  data_reduction_proxy::LoFiDecider* lofi_decider = nullptr;
  if (data_reduction_proxy_io_data)
    lofi_decider = data_reduction_proxy_io_data->lofi_decider();

  data_reduction_proxy::DataReductionProxyData* data =
      data_reduction_proxy::DataReductionProxyData::GetData(*url_request);
  std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
      data_reduction_proxy_data;
  if (data)
    data_reduction_proxy_data = data->DeepCopy();
  int64_t original_content_length =
      data && data->used_data_reduction_proxy()
          ? data_reduction_proxy::util::EstimateOriginalBodySize(*url_request,
                                                                 lofi_decider)
          : url_request->GetRawBodyBytes();

  net::HostPortPair request_host_port;
  // We want to get the IP address of the response if it was returned, and the
  // last endpoint that was checked if it failed.
  if (url_request->response_headers()) {
    request_host_port = url_request->GetSocketAddress();
  }
  if (request_host_port.IsEmpty()) {
    net::IPEndPoint request_ip_endpoint;
    bool was_successful = url_request->GetRemoteEndpoint(&request_ip_endpoint);
    if (was_successful) {
      request_host_port =
          net::HostPortPair::FromIPEndPoint(request_ip_endpoint);
    }
  }

  auto load_timing_info = std::make_unique<net::LoadTimingInfo>();
  url_request->GetLoadTimingInfo(load_timing_info.get());

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &NotifyUIThreadOfRequestComplete,
          info->GetWebContentsGetterForRequest(),
          info->GetFrameTreeNodeIdGetterForRequest(), url_request->url(),
          request_host_port, info->GetGlobalRequestID(), info->GetChildID(),
          info->GetRenderFrameID(), info->GetResourceType(), info->IsDownload(),
          url_request->was_cached(), std::move(data_reduction_proxy_data),
          net_error, url_request->GetTotalReceivedBytes(),
          url_request->GetRawBodyBytes(), original_content_length,
          url_request->creation_time(), std::move(load_timing_info)));
}

content::NavigationData*
ChromeResourceDispatcherHostDelegate::GetNavigationData(
    net::URLRequest* request) const {
  ChromeNavigationData* data =
      ChromeNavigationData::GetDataAndCreateIfNecessary(request);
  if (!request)
    return data;

  data_reduction_proxy::DataReductionProxyData* data_reduction_proxy_data =
      data_reduction_proxy::DataReductionProxyData::GetData(*request);
  // DeepCopy the DataReductionProxyData from the URLRequest to prevent the
  // URLRequest and DataReductionProxyData from both having ownership of the
  // same object. This copy will be shortlived as it will be deep copied again
  // when content makes a clone of NavigationData for the UI thread.
  if (data_reduction_proxy_data)
    data->SetDataReductionProxyData(data_reduction_proxy_data->DeepCopy());

  return data;
}
