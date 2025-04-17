// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_observer.h"

#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tracker.h"
#include "chrome/browser/loader/from_gws_navigation_and_keep_alive_request_tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/page_load_metrics/browser/features.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/google/browser/google_url_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/resource_request.h"

FromGWSNavigationAndKeepAliveRequestObserver::
    FromGWSNavigationAndKeepAliveRequestObserver(
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

FromGWSNavigationAndKeepAliveRequestObserver::
    ~FromGWSNavigationAndKeepAliveRequestObserver() = default;

// static
std::unique_ptr<FromGWSNavigationAndKeepAliveRequestObserver>
FromGWSNavigationAndKeepAliveRequestObserver::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  if (!base::FeatureList::IsEnabled(
          page_load_metrics::features::kBeaconLeakageLogging)) {
    return nullptr;
  }
  return base::WrapUnique(
      new FromGWSNavigationAndKeepAliveRequestObserver(web_contents));
}

void FromGWSNavigationAndKeepAliveRequestObserver::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  CHECK(navigation_handle);

  if (!base::FeatureList::IsEnabled(
          page_load_metrics::features::kBeaconLeakageLogging)) {
    return;
  }

  // Ensures the navigation is initiated from a Google SRP.
  if (!navigation_handle->GetInitiatorFrameToken().has_value()) {
    return;
  }
  content::RenderFrameHost* initiator_rfh =
      content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(
              navigation_handle->GetInitiatorProcessId(),
              navigation_handle->GetInitiatorFrameToken().value()));
  if (!initiator_rfh) {
    return;
  }

  GURL url = initiator_rfh->GetLastCommittedURL();
  if (!page_load_metrics::IsGoogleSearchResultUrl(url)) {
    return;
  }

  if (initiator_rfh->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    // Skips navigations from prerendering pages, which are not eligible for
    // UKM logging.
    return;
  }

  auto navigation_category_id =
      page_load_metrics::GetCategoryIdFromUrl(navigation_handle->GetURL());
  if (!navigation_category_id.has_value()) {
    return;
  }

  auto* tracker =
      FromGWSNavigationAndKeepAliveRequestTrackerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (!tracker) {
    return;
  }

  tracker->TrackNavigation(initiator_rfh->GetGlobalId(),
                           *navigation_category_id,
                           initiator_rfh->GetPageUkmSourceId(),
                           navigation_handle->GetNavigationId());
}

void FromGWSNavigationAndKeepAliveRequestObserver::OnKeepAliveRequestCreated(
    const network::ResourceRequest& request,
    content::RenderFrameHost* initiator_rfh) {
  CHECK(initiator_rfh);
  CHECK(request.keepalive);
  CHECK(request.keepalive_token.has_value());

  if (!base::FeatureList::IsEnabled(
          page_load_metrics::features::kBeaconLeakageLogging)) {
    return;
  }

  // Ensures the request is initiated from a Google SRP.
  GURL url = initiator_rfh->GetLastCommittedURL();
  if (!page_load_metrics::IsGoogleSearchResultUrl(url)) {
    return;
  }

  if (initiator_rfh->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    // Skips navigations from prerendering pages, which are not eligible for
    // UKM logging.
    return;
  }

  // The request and the initiator page must have the same category ID.
  auto request_category_id =
      page_load_metrics::GetCategoryIdFromUrl(request.url);
  if (!request_category_id.has_value()) {
    return;
  }

  auto* tracker =
      FromGWSNavigationAndKeepAliveRequestTrackerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents()->GetBrowserContext()));
  if (!tracker) {
    return;
  }
  tracker->TrackKeepAliveRequest(
      initiator_rfh->GetGlobalId(), *request_category_id,
      initiator_rfh->GetPageUkmSourceId(), *request.keepalive_token);
}
