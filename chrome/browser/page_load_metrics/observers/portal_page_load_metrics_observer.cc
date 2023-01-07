// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/portal_page_load_metrics_observer.h"

#include <cmath>
#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/protocol_util.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "ui/events/blink/blink_features.h"

// static
std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
PortalPageLoadMetricsObserver::CreateIfNeeded() {
  if (!ukm::UkmRecorder::Get()) {
    return nullptr;
  }
  return std::make_unique<PortalPageLoadMetricsObserver>();
}

PortalPageLoadMetricsObserver::PortalPageLoadMetricsObserver() = default;

PortalPageLoadMetricsObserver::~PortalPageLoadMetricsObserver() = default;

PortalPageLoadMetricsObserver::ObservePolicy
PortalPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  is_portal_ = web_contents->IsPortal();

  if (!started_in_foreground)
    was_hidden_ = true;

  navigation_start_ = navigation_handle->NavigationStart();

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PortalPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // TODO(https://crbug.com/1271055): Prerender doesn't support combined use
  // with Portals. So, there is no case to start with prerendering to monitor
  // Portals related metrics.
  DCHECK(!navigation_handle->GetWebContents()->IsPortal());
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PortalPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // FencedFrames can be created inside a Portal, but as this class is
  // interested only in Portal pages, stop observing for such FencedFrame inner
  // pages.
  return STOP_OBSERVING;
}

PortalPageLoadMetricsObserver::ObservePolicy
PortalPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (is_portal_)
    return STOP_OBSERVING;

  ReportPortalActivatedPaint(timing.paint_timing->portal_activated_paint);
  if (!was_hidden_)
    RecordTimingMetrics(timing);
  return STOP_OBSERVING;
}

PortalPageLoadMetricsObserver::ObservePolicy
PortalPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (is_portal_)
    return CONTINUE_OBSERVING;

  if (!was_hidden_) {
    RecordTimingMetrics(timing);
    was_hidden_ = true;
  }
  return CONTINUE_OBSERVING;
}

void PortalPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (is_portal_)
    return;

  ReportPortalActivatedPaint(timing.paint_timing->portal_activated_paint);
  if (!was_hidden_)
    RecordTimingMetrics(timing);
}

void PortalPageLoadMetricsObserver::RecordTimingMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  // Only record if timing is consistent across processes.
  if (!base::TimeTicks::IsConsistentAcrossProcesses())
    return;

  ukm::builders::Portal_Activate builder(GetDelegate().GetPageUkmSourceId());

  const page_load_metrics::ContentfulPaintTimingInfo&
      main_frame_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MainFrameLargestContentfulPaint();
  if (main_frame_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          main_frame_largest_contentful_paint.Time(), GetDelegate())) {
    if (portal_activation_time_) {
      base::TimeDelta portal_activation_time_delta =
          *portal_activation_time_ -
          main_frame_largest_contentful_paint.Time().value() -
          navigation_start_;
      bool portal_activation_before_lcp =
          portal_activation_time_delta.InMilliseconds() < 0;
      if (portal_activation_before_lcp)
        portal_activation_time_delta = -portal_activation_time_delta;
      builder.SetPortalActivation(ukm::GetExponentialBucketMinForUserTiming(
          portal_activation_time_delta.InMilliseconds()));

      // Store whether we are activating before the LCP has fired in the main
      // frame. This is necessary because we are only recording the activation
      // time in exponential buckets.
      builder.SetPortalActivationBeforeLCP(portal_activation_before_lcp);
    }
  }

  if (portal_paint_time_ && portal_activation_time_) {
    builder.SetPaintTiming_PortalActivationToFirstPaint(
        (*portal_paint_time_ - *portal_activation_time_).InMilliseconds());
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void PortalPageLoadMetricsObserver::ReportPortalActivatedPaint(
    const absl::optional<base::TimeTicks>& portal_activated_paint) {
  portal_paint_time_ = portal_activated_paint;
}

void PortalPageLoadMetricsObserver::DidActivatePortal(
    base::TimeTicks activation_time) {
  is_portal_ = false;
  portal_activation_time_ = activation_time;
}
