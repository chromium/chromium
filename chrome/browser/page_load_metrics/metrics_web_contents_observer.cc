// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/metrics_web_contents_observer.h"

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/page_load_metrics/browser_page_track_decider.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_embedder_interface.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_update_dispatcher.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/page_load_metrics/page_load_tracker.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/features.h"
#include "ui/base/page_transition_types.h"

namespace page_load_metrics {

namespace {

content::RenderFrameHost* GetMainFrame(content::RenderFrameHost* rfh) {
  // Don't use rfh->GetRenderViewHost()->GetMainFrame() here because
  // RenderViewHost is being deprecated and because in OOPIF,
  // RenderViewHost::GetMainFrame() returns nullptr for child frames hosted in a
  // different process from the main frame.
  while (rfh->GetParent() != nullptr)
    rfh = rfh->GetParent();
  return rfh;
}

UserInitiatedInfo CreateUserInitiatedInfo(
    content::NavigationHandle* navigation_handle,
    PageLoadTracker* committed_load) {
  if (!navigation_handle->IsRendererInitiated())
    return UserInitiatedInfo::BrowserInitiated();

  return UserInitiatedInfo::RenderInitiated(
      navigation_handle->HasUserGesture(),
      committed_load &&
          committed_load->input_tracker()->FindAndConsumeInputEventsBefore(
              navigation_handle->NavigationStart()));
}

}  // namespace

// static
void MetricsWebContentsObserver::RecordFeatureUsage(
    content::RenderFrameHost* render_frame_host,
    const mojom::PageLoadFeatures& new_features) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  MetricsWebContentsObserver* observer =
      MetricsWebContentsObserver::FromWebContents(web_contents);
  if (observer)
    observer->OnBrowserFeatureUsage(render_frame_host, new_features);
}

MetricsWebContentsObserver::MetricsWebContentsObserver(
    content::WebContents* web_contents,
    std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface)
    : content::WebContentsObserver(web_contents),
      in_foreground_(web_contents->GetVisibility() !=
                     content::Visibility::HIDDEN),
      embedder_interface_(std::move(embedder_interface)),
      has_navigated_(false),
      page_load_metrics_binding_(web_contents, this) {
  // Prerenders erroneously report that they are initially visible, so we
  // manually override visibility state for prerender.
  const bool is_prerender =
      prerender::PrerenderContents::FromWebContents(web_contents) != nullptr;
  if (is_prerender)
    in_foreground_ = false;

  RegisterInputEventObserver(web_contents->GetRenderViewHost());
}

// static
MetricsWebContentsObserver* MetricsWebContentsObserver::CreateForWebContents(
    content::WebContents* web_contents,
    std::unique_ptr<PageLoadMetricsEmbedderInterface> embedder_interface) {
  DCHECK(web_contents);

  MetricsWebContentsObserver* metrics = FromWebContents(web_contents);
  if (!metrics) {
    metrics = new MetricsWebContentsObserver(web_contents,
                                             std::move(embedder_interface));
    web_contents->SetUserData(UserDataKey(), base::WrapUnique(metrics));
  }
  return metrics;
}

MetricsWebContentsObserver::~MetricsWebContentsObserver() {}

void MetricsWebContentsObserver::WebContentsWillSoonBeDestroyed() {
  web_contents_will_soon_be_destroyed_ = true;
}

void MetricsWebContentsObserver::WebContentsDestroyed() {
  // TODO(csharrison): Use a more user-initiated signal for CLOSE.
  NotifyPageEndAllLoads(END_CLOSE, UserInitiatedInfo::NotUserInitiated());

  // We tear down PageLoadTrackers in WebContentsDestroyed, rather than in the
  // destructor, since |web_contents()| returns nullptr in the destructor, and
  // PageLoadMetricsObservers can cause code to execute that wants to be able to
  // access the current WebContents.
  committed_load_ = nullptr;
  provisional_loads_.clear();
  aborted_provisional_loads_.clear();

  for (auto& observer : testing_observers_)
    observer.OnGoingAway();
}

void MetricsWebContentsObserver::RegisterInputEventObserver(
    content::RenderViewHost* host) {
  if (host != nullptr)
    host->GetWidget()->AddInputEventObserver(this);
}

void MetricsWebContentsObserver::UnregisterInputEventObserver(
    content::RenderViewHost* host) {
  if (host != nullptr)
    host->GetWidget()->RemoveInputEventObserver(this);
}

void MetricsWebContentsObserver::RenderViewHostChanged(
    content::RenderViewHost* old_host,
    content::RenderViewHost* new_host) {
  UnregisterInputEventObserver(old_host);
  RegisterInputEventObserver(new_host);
}

void MetricsWebContentsObserver::MediaStartedPlaying(
    const content::WebContentsObserver::MediaPlayerInfo& video_type,
    const content::WebContentsObserver::MediaPlayerId& id) {
  if (GetMainFrame(id.render_frame_host) != web_contents()->GetMainFrame()) {
    // Ignore media that starts playing in a document that was navigated away
    // from.
    return;
  }
  if (committed_load_)
    committed_load_->MediaStartedPlaying(
        video_type, id.render_frame_host == web_contents()->GetMainFrame());
}

void MetricsWebContentsObserver::WillStartNavigationRequest(
    content::NavigationHandle* navigation_handle) {
  // Same-document navigations should never go through
  // WillStartNavigationRequest.
  DCHECK(!navigation_handle->IsSameDocument());

  if (!navigation_handle->IsInMainFrame())
    return;

  UserInitiatedInfo user_initiated_info(
      CreateUserInitiatedInfo(navigation_handle, committed_load_.get()));
  std::unique_ptr<PageLoadTracker> last_aborted =
      NotifyAbortedProvisionalLoadsNewNavigation(navigation_handle,
                                                 user_initiated_info);

  int chain_size_same_url = 0;
  int chain_size = 0;
  if (last_aborted) {
    if (last_aborted->MatchesOriginalNavigation(navigation_handle)) {
      chain_size_same_url = last_aborted->aborted_chain_size_same_url() + 1;
    } else if (last_aborted->aborted_chain_size_same_url() > 0) {
      LogAbortChainSameURLHistogram(
          last_aborted->aborted_chain_size_same_url());
    }
    chain_size = last_aborted->aborted_chain_size() + 1;
  }

  if (!ShouldTrackNavigation(navigation_handle))
    return;

  // Pass in the last committed url to the PageLoadTracker. If the MWCO has
  // never observed a committed load, use the last committed url from this
  // WebContent's opener. This is more accurate than using referrers due to
  // referrer sanitizing and origin referrers. Note that this could potentially
  // be inaccurate if the opener has since navigated.
  content::RenderFrameHost* opener = web_contents()->GetOpener();
  const GURL& opener_url = !has_navigated_ && opener
                               ? opener->GetLastCommittedURL()
                               : GURL::EmptyGURL();
  const GURL& currently_committed_url =
      committed_load_ ? committed_load_->url() : opener_url;
  has_navigated_ = true;

  // We can have two provisional loads in some cases. E.g. a same-site
  // navigation can have a concurrent cross-process navigation started
  // from the omnibox.
  DCHECK_GT(2ul, provisional_loads_.size());
  // Passing raw pointers to observers_ and embedder_interface_ is safe because
  // the MetricsWebContentsObserver owns them both list and they are torn down
  // after the PageLoadTracker. The PageLoadTracker does not hold on to
  // committed_load_ or navigation_handle beyond the scope of the constructor.
  auto insertion_result = provisional_loads_.insert(std::make_pair(
      navigation_handle,
      std::make_unique<PageLoadTracker>(
          in_foreground_, embedder_interface_.get(), currently_committed_url,
          navigation_handle, user_initiated_info, chain_size,
          chain_size_same_url)));
  DCHECK(insertion_result.second)
      << "provisional_loads_ already contains NavigationHandle.";
  for (auto& observer : testing_observers_)
    observer.OnTrackerCreated(insertion_result.first->second.get());
}

void MetricsWebContentsObserver::WillProcessNavigationResponse(
    content::NavigationHandle* navigation_handle) {
  auto it = provisional_loads_.find(navigation_handle);
  if (it == provisional_loads_.end())
    return;
  it->second->WillProcessNavigationResponse(navigation_handle);
}

PageLoadTracker* MetricsWebContentsObserver::GetTrackerOrNullForRequest(
    const content::GlobalRequestID& request_id,
    content::RenderFrameHost* render_frame_host_or_null,
    content::ResourceType resource_type,
    base::TimeTicks creation_time) {
  if (resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    DCHECK(request_id != content::GlobalRequestID());
    // The main frame request can complete either before or after commit, so we
    // look at both provisional loads and the committed load to find a
    // PageLoadTracker with a matching request id. See https://goo.gl/6TzCYN for
    // more details.
    for (const auto& kv : provisional_loads_) {
      PageLoadTracker* candidate = kv.second.get();
      if (candidate->HasMatchingNavigationRequestID(request_id)) {
        return candidate;
      }
    }
    if (committed_load_ &&
        committed_load_->HasMatchingNavigationRequestID(request_id)) {
      return committed_load_.get();
    }
  } else {
    // Non main frame resources are always associated with the currently
    // committed load. If the resource request was started before this
    // navigation then it should be ignored.
    if (!committed_load_ || creation_time < committed_load_->navigation_start())
      return nullptr;

    // Sub-frame resources have a null RFH when browser-side navigation is
    // enabled, so we can't perform the RFH check below for them.
    //
    // TODO(bmcquade): consider tracking GlobalRequestIDs for sub-frame
    // navigations in each PageLoadTracker, and performing a lookup for
    // sub-frames similar to the main-frame lookup above.
    if (resource_type == content::RESOURCE_TYPE_SUB_FRAME)
      return committed_load_.get();

    // There is a race here: a completed resource for the previously committed
    // page can arrive after the new page has committed. In this case, we may
    // attribute the resource to the wrong page load. We do our best to guard
    // against this by verifying that the RFH for the resource matches the RFH
    // for the currently committed load, however there are cases where the same
    // RFH is used across page loads (same origin navigations, as well as some
    // cross-origin render-initiated navigations).
    //
    // TODO(crbug.com/738577): use a DocumentId here instead, to eliminate this
    // race.
    DCHECK(render_frame_host_or_null != nullptr);
    content::RenderFrameHost* main_frame_for_resource =
        GetMainFrame(render_frame_host_or_null);
    if (main_frame_for_resource == web_contents()->GetMainFrame())
      return committed_load_.get();
  }
  return nullptr;
}
void MetricsWebContentsObserver::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const content::mojom::ResourceLoadInfo& resource_load_info) {
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService))
    return;

  if (!resource_load_info.url.SchemeIsHTTPOrHTTPS())
    return;

  PageLoadTracker* tracker = GetTrackerOrNullForRequest(
      request_id, render_frame_host, resource_load_info.resource_type,
      resource_load_info.load_timing_info.request_start);
  if (tracker) {
    // TODO(crbug.com/721403): Fill in data reduction proxy fields when this is
    // available in the network service.
    // int original_content_length =
    //     was_cached ? 0
    //                : data_reduction_proxy::util::EstimateOriginalBodySize(
    //                      request, lofi_decider);
    int original_content_length = 0;
    std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
        data_reduction_proxy_data;

    const content::mojom::CommonNetworkInfoPtr& network_info =
        resource_load_info.network_info;
    ExtraRequestCompleteInfo extra_request_complete_info(
        resource_load_info.url, network_info->ip_port_pair.value(),
        render_frame_host->GetFrameTreeNodeId(), resource_load_info.was_cached,
        resource_load_info.raw_body_bytes, original_content_length,
        std::move(data_reduction_proxy_data), resource_load_info.resource_type,
        resource_load_info.net_error,
        std::make_unique<net::LoadTimingInfo>(
            resource_load_info.load_timing_info));
    tracker->OnLoadedResource(extra_request_complete_info);
  }
}

void MetricsWebContentsObserver::OnRequestComplete(
    const GURL& url,
    const net::HostPortPair& host_port_pair,
    int frame_tree_node_id,
    const content::GlobalRequestID& request_id,
    content::RenderFrameHost* render_frame_host_or_null,
    content::ResourceType resource_type,
    bool was_cached,
    std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
        data_reduction_proxy_data,
    int64_t raw_body_bytes,
    int64_t original_content_length,
    base::TimeTicks creation_time,
    int net_error,
    std::unique_ptr<net::LoadTimingInfo> load_timing_info) {
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));

  // Ignore non-HTTP(S) resources (blobs, data uris, etc).
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  PageLoadTracker* tracker = GetTrackerOrNullForRequest(
      request_id, render_frame_host_or_null, resource_type, creation_time);
  if (tracker) {
    ExtraRequestCompleteInfo extra_request_complete_info(
        url, host_port_pair, frame_tree_node_id, was_cached, raw_body_bytes,
        was_cached ? 0 : original_content_length,
        std::move(data_reduction_proxy_data), resource_type, net_error,
        std::move(load_timing_info));
    tracker->OnLoadedResource(extra_request_complete_info);
  }
}

const PageLoadExtraInfo
MetricsWebContentsObserver::GetPageLoadExtraInfoForCommittedLoad() {
  DCHECK(committed_load_);
  return committed_load_->ComputePageLoadExtraInfo();
}

void MetricsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    if (committed_load_ && navigation_handle->GetParentFrame() &&
        GetMainFrame(navigation_handle->GetParentFrame()) ==
            web_contents()->GetMainFrame()) {
      committed_load_->DidFinishSubFrameNavigation(navigation_handle);
      committed_load_->metrics_update_dispatcher()->DidFinishSubFrameNavigation(
          navigation_handle);
    }
    return;
  }

  std::unique_ptr<PageLoadTracker> finished_nav(
      std::move(provisional_loads_[navigation_handle]));
  provisional_loads_.erase(navigation_handle);

  // Ignore same-document navigations.
  if (navigation_handle->HasCommitted() &&
      navigation_handle->IsSameDocument()) {
    if (finished_nav)
      finished_nav->StopTracking();
    if (committed_load_)
      committed_load_->DidCommitSameDocumentNavigation(navigation_handle);
    return;
  }

  // Ignore internally generated aborts for navigations with HTTP responses that
  // don't commit, such as HTTP 204 responses and downloads.
  if (!navigation_handle->HasCommitted() &&
      navigation_handle->GetNetErrorCode() == net::ERR_ABORTED &&
      navigation_handle->GetResponseHeaders()) {
    if (finished_nav) {
      finished_nav->DidInternalNavigationAbort(navigation_handle);
      finished_nav->StopTracking();
    }
    return;
  }

  const bool should_track =
      finished_nav && ShouldTrackNavigation(navigation_handle);

  if (finished_nav && !should_track)
    finished_nav->StopTracking();

  if (navigation_handle->HasCommitted()) {
    UserInitiatedInfo user_initiated_info =
        finished_nav
            ? finished_nav->user_initiated_info()
            : CreateUserInitiatedInfo(navigation_handle, committed_load_.get());

    // Notify other loads that they may have been aborted by this committed
    // load. is_certainly_browser_timestamp is set to false because
    // NavigationStart() could be set in either the renderer or browser process.
    NotifyPageEndAllLoadsWithTimestamp(
        EndReasonForPageTransition(navigation_handle->GetPageTransition()),
        user_initiated_info, navigation_handle->NavigationStart(), false);

    if (should_track) {
      HandleCommittedNavigationForTrackedLoad(navigation_handle,
                                              std::move(finished_nav));
    } else {
      committed_load_.reset();
    }
  } else if (should_track) {
    HandleFailedNavigationForTrackedLoad(navigation_handle,
                                         std::move(finished_nav));
  }
}

// Handle a pre-commit error. Navigations that result in an error page will be
// ignored.
void MetricsWebContentsObserver::HandleFailedNavigationForTrackedLoad(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PageLoadTracker> tracker) {
  const base::TimeTicks now = base::TimeTicks::Now();
  tracker->FailedProvisionalLoad(navigation_handle, now);

  const net::Error error = navigation_handle->GetNetErrorCode();

  // net::OK: This case occurs when the NavigationHandle finishes and reports
  // !HasCommitted(), but reports no net::Error. This should not occur
  // pre-PlzNavigate, but afterwards it should represent the navigation stopped
  // by the user before it was ready to commit.
  // net::ERR_ABORTED: An aborted provisional load has error net::ERR_ABORTED.
  const bool is_aborted_provisional_load =
      error == net::OK || error == net::ERR_ABORTED;

  // If is_aborted_provisional_load, the page end reason is not yet known, and
  // will be updated as additional information is available from subsequent
  // navigations.
  tracker->NotifyPageEnd(
      is_aborted_provisional_load ? END_OTHER : END_PROVISIONAL_LOAD_FAILED,
      UserInitiatedInfo::NotUserInitiated(), now, true);

  if (is_aborted_provisional_load)
    aborted_provisional_loads_.push_back(std::move(tracker));
}

void MetricsWebContentsObserver::HandleCommittedNavigationForTrackedLoad(
    content::NavigationHandle* navigation_handle,
    std::unique_ptr<PageLoadTracker> tracker) {
  if (!IsNavigationUserInitiated(navigation_handle) &&
      (navigation_handle->GetPageTransition() &
       ui::PAGE_TRANSITION_CLIENT_REDIRECT) != 0 &&
      committed_load_) {
    // TODO(bmcquade): consider carrying the user_gesture bit forward to the
    // redirected navigation.
    committed_load_->NotifyClientRedirectTo(*tracker);
  }

  committed_load_ = std::move(tracker);
  committed_load_->Commit(navigation_handle);
  DCHECK(committed_load_->did_commit());

  for (auto& observer : testing_observers_)
    observer.OnCommit(committed_load_.get());
}

void MetricsWebContentsObserver::NavigationStopped() {
  // TODO(csharrison): Use a more user-initiated signal for STOP.
  NotifyPageEndAllLoads(END_STOP, UserInitiatedInfo::NotUserInitiated());
}

void MetricsWebContentsObserver::OnInputEvent(
    const blink::WebInputEvent& event) {
  // Ignore browser navigation or reload which comes with type Undefined.
  if (event.GetType() == blink::WebInputEvent::Type::kUndefined)
    return;

  if (committed_load_)
    committed_load_->OnInputEvent(event);
}

void MetricsWebContentsObserver::FlushMetricsOnAppEnterBackground() {
  // Note that, while a call to FlushMetricsOnAppEnterBackground usually
  // indicates that the app is about to be backgrounded, there are cases where
  // the app may not end up getting backgrounded. Thus, we should not assume
  // anything about foreground / background state of the associated tab as part
  // of this method call.

  if (committed_load_)
    committed_load_->FlushMetricsOnAppEnterBackground();
  for (const auto& kv : provisional_loads_) {
    kv.second->FlushMetricsOnAppEnterBackground();
  }
  for (const auto& tracker : aborted_provisional_loads_) {
    tracker->FlushMetricsOnAppEnterBackground();
  }
}

void MetricsWebContentsObserver::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame())
    return;
  auto it = provisional_loads_.find(navigation_handle);
  if (it == provisional_loads_.end())
    return;
  it->second->Redirect(navigation_handle);
}

void MetricsWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  if (web_contents_will_soon_be_destroyed_)
    return;

  // TODO(bmcquade): Consider handling an OCCLUDED tab as not in foreground.
  bool was_in_foreground = in_foreground_;
  in_foreground_ = visibility != content::Visibility::HIDDEN;
  if (in_foreground_ == was_in_foreground)
    return;

  if (in_foreground_) {
    if (committed_load_)
      committed_load_->WebContentsShown();
    for (const auto& kv : provisional_loads_) {
      kv.second->WebContentsShown();
    }
  } else {
    if (committed_load_)
      committed_load_->WebContentsHidden();
    for (const auto& kv : provisional_loads_) {
      kv.second->WebContentsHidden();
    }
  }
}

// This will occur when the process for the main RenderFrameHost exits, either
// normally or from a crash. We eagerly log data from the last committed load if
// we have one.
void MetricsWebContentsObserver::RenderProcessGone(
    base::TerminationStatus status) {
  // Other code paths will be run for normal renderer shutdown. Note that we
  // sometimes get the STILL_RUNNING value on fast shutdown.
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_STILL_RUNNING) {
    return;
  }

  // RenderProcessGone is associated with the render frame host for the
  // currently committed load. We don't know if the pending navs or aborted
  // pending navs are associated w/ the render process that died, so we can't be
  // sure the info should propagate to them.
  if (committed_load_) {
    committed_load_->NotifyPageEnd(END_RENDER_PROCESS_GONE,
                                   UserInitiatedInfo::NotUserInitiated(),
                                   base::TimeTicks::Now(), true);
  }

  // If this is a crash, eagerly log the aborted provisional loads and the
  // committed load. |provisional_loads_| don't need to be destroyed here
  // because their lifetime is tied to the NavigationHandle.
  committed_load_.reset();
  aborted_provisional_loads_.clear();
}

void MetricsWebContentsObserver::NotifyPageEndAllLoads(
    PageEndReason page_end_reason,
    UserInitiatedInfo user_initiated_info) {
  NotifyPageEndAllLoadsWithTimestamp(page_end_reason, user_initiated_info,
                                     base::TimeTicks::Now(), true);
}

void MetricsWebContentsObserver::NotifyPageEndAllLoadsWithTimestamp(
    PageEndReason page_end_reason,
    UserInitiatedInfo user_initiated_info,
    base::TimeTicks timestamp,
    bool is_certainly_browser_timestamp) {
  if (committed_load_) {
    committed_load_->NotifyPageEnd(page_end_reason, user_initiated_info,
                                   timestamp, is_certainly_browser_timestamp);
  }
  for (const auto& kv : provisional_loads_) {
    kv.second->NotifyPageEnd(page_end_reason, user_initiated_info, timestamp,
                             is_certainly_browser_timestamp);
  }
  for (const auto& tracker : aborted_provisional_loads_) {
    if (tracker->IsLikelyProvisionalAbort(timestamp)) {
      tracker->UpdatePageEnd(page_end_reason, user_initiated_info, timestamp,
                             is_certainly_browser_timestamp);
    }
  }
  aborted_provisional_loads_.clear();
}

std::unique_ptr<PageLoadTracker>
MetricsWebContentsObserver::NotifyAbortedProvisionalLoadsNewNavigation(
    content::NavigationHandle* new_navigation,
    UserInitiatedInfo user_initiated_info) {
  // If there are multiple aborted loads that can be attributed to this one,
  // just count the latest one for simplicity. Other loads will fall into the
  // OTHER bucket, though there shouldn't be very many.
  if (aborted_provisional_loads_.size() == 0)
    return nullptr;
  if (aborted_provisional_loads_.size() > 1)
    RecordInternalError(ERR_NAVIGATION_SIGNALS_MULIPLE_ABORTED_LOADS);

  std::unique_ptr<PageLoadTracker> last_aborted_load =
      std::move(aborted_provisional_loads_.back());
  aborted_provisional_loads_.pop_back();

  base::TimeTicks timestamp = new_navigation->NavigationStart();
  if (last_aborted_load->IsLikelyProvisionalAbort(timestamp)) {
    last_aborted_load->UpdatePageEnd(
        EndReasonForPageTransition(new_navigation->GetPageTransition()),
        user_initiated_info, timestamp, false);
  }

  aborted_provisional_loads_.clear();
  return last_aborted_load;
}

void MetricsWebContentsObserver::OnTimingUpdated(
    content::RenderFrameHost* render_frame_host,
    const mojom::PageLoadTiming& timing,
    const mojom::PageLoadMetadata& metadata,
    const mojom::PageLoadFeatures& new_features,
    const std::vector<mojom::ResourceDataUpdatePtr>& resources) {
  // We may receive notifications from frames that have been navigated away
  // from. We simply ignore them.
  if (GetMainFrame(render_frame_host) != web_contents()->GetMainFrame()) {
    RecordInternalError(ERR_IPC_FROM_WRONG_FRAME);
    return;
  }

  const bool is_main_frame = (render_frame_host->GetParent() == nullptr);
  if (is_main_frame) {
    // While timings arriving for the wrong frame are expected, we do not expect
    // any of the errors below for main frames. Thus, we track occurrences of
    // all errors below, rather than returning early after encountering an
    // error.
    bool error = false;
    if (!committed_load_) {
      RecordInternalError(ERR_IPC_WITH_NO_RELEVANT_LOAD);
      error = true;
    }

    if (!web_contents()->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
      RecordInternalError(ERR_IPC_FROM_BAD_URL_SCHEME);
      error = true;
    }

    if (error)
      return;
  } else if (!committed_load_) {
    RecordInternalError(ERR_SUBFRAME_IPC_WITH_NO_RELEVANT_LOAD);
  }

  if (committed_load_) {
    committed_load_->metrics_update_dispatcher()->UpdateMetrics(
        render_frame_host, timing, metadata, new_features, resources);
  }
}

void MetricsWebContentsObserver::UpdateTiming(
    const mojom::PageLoadTimingPtr timing,
    const mojom::PageLoadMetadataPtr metadata,
    const mojom::PageLoadFeaturesPtr new_features,
    const std::vector<mojom::ResourceDataUpdatePtr> resources) {
  content::RenderFrameHost* render_frame_host =
      page_load_metrics_binding_.GetCurrentTargetFrame();
  OnTimingUpdated(render_frame_host, *timing, *metadata, *new_features,
                  resources);
}

bool MetricsWebContentsObserver::ShouldTrackNavigation(
    content::NavigationHandle* navigation_handle) const {
  DCHECK(navigation_handle->IsInMainFrame());
  DCHECK(!navigation_handle->HasCommitted() ||
         !navigation_handle->IsSameDocument());

  return BrowserPageTrackDecider(embedder_interface_.get(), navigation_handle)
      .ShouldTrack();
}

void MetricsWebContentsObserver::OnBrowserFeatureUsage(
    content::RenderFrameHost* render_frame_host,
    const mojom::PageLoadFeatures& new_features) {
  // Since this call is coming directly from the browser, it should not pass us
  // data from frames that have already been navigated away from.
  DCHECK_EQ(GetMainFrame(render_frame_host), web_contents()->GetMainFrame());

  if (!committed_load_) {
    RecordInternalError(ERR_BROWSER_USAGE_WITH_NO_RELEVANT_LOAD);
    return;
  }

  committed_load_->metrics_update_dispatcher()->UpdateFeatures(
      render_frame_host, new_features);
}

void MetricsWebContentsObserver::AddTestingObserver(TestingObserver* observer) {
  if (!testing_observers_.HasObserver(observer))
    testing_observers_.AddObserver(observer);
}

void MetricsWebContentsObserver::RemoveTestingObserver(
    TestingObserver* observer) {
  testing_observers_.RemoveObserver(observer);
}

MetricsWebContentsObserver::TestingObserver::TestingObserver(
    content::WebContents* web_contents)
    : observer_(page_load_metrics::MetricsWebContentsObserver::FromWebContents(
          web_contents)) {
  observer_->AddTestingObserver(this);
}

MetricsWebContentsObserver::TestingObserver::~TestingObserver() {
  if (observer_) {
    observer_->RemoveTestingObserver(this);
    observer_ = nullptr;
  }
}

void MetricsWebContentsObserver::TestingObserver::OnGoingAway() {
  observer_ = nullptr;
}

void MetricsWebContentsObserver::BroadcastEventToObservers(
    const void* const event_key) {
  if (committed_load_)
    committed_load_->BroadcastEventToObservers(event_key);
}

}  // namespace page_load_metrics
