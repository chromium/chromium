// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/initial_webui_page_load_metrics_observer.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/common/chrome_features.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {
// Measurement marks.
constexpr char kInputMouseReleaseStartMark[] =
    "ReloadButton.Input.MouseRelease.Start";
constexpr char kInputKeyPressStartMark[] = "ReloadButton.Input.KeyPress.Start";

}  // namespace

InitialWebUIPageLoadMetricsObserver::InitialWebUIPageLoadMetricsObserver() =
    default;

InitialWebUIPageLoadMetricsObserver::~InitialWebUIPageLoadMetricsObserver() =
    default;

const char* InitialWebUIPageLoadMetricsObserver::GetObserverName() const {
  return "InitialWebUIPageLoadMetricsObserver";
}

void InitialWebUIPageLoadMetricsObserver::OnMonotonicFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!timing.monotonic_paint_timing ||
      !timing.monotonic_paint_timing->first_paint) {
    return;
  }

  if (auto* manager = GetMetricsManager()) {
    manager->OnReloadButtonFirstPaint(
        timing.monotonic_paint_timing->first_paint.value());
  }
}

void InitialWebUIPageLoadMetricsObserver::OnMonotonicFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!timing.monotonic_paint_timing ||
      !timing.monotonic_paint_timing->first_contentful_paint) {
    return;
  }

  if (auto* manager = GetMetricsManager()) {
    manager->OnReloadButtonFirstContentfulPaint(
        timing.monotonic_paint_timing->first_contentful_paint.value());
  }
}

void InitialWebUIPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!timing.paint_timing || !timing.paint_timing->first_contentful_paint) {
    return;
  }
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }

  ukm::builders::InitialWebUIPageLoad(GetDelegate().GetPageUkmSourceId())
      .SetPaintTiming_NavigationToFirstContentfulPaint(
          timing.paint_timing->first_contentful_paint.value().InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void InitialWebUIPageLoadMetricsObserver::OnUserInput(
    const blink::WebInputEvent& event,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  auto& metrics_reporter = GetMetricsReporter();
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kMouseUp:
      metrics_reporter.Mark(kInputMouseReleaseStartMark, event.TimeStamp());
      break;
    case blink::WebInputEvent::Type::kRawKeyDown:
      metrics_reporter.Mark(kInputKeyPressStartMark, event.TimeStamp());
      break;
    default:
      break;
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // The target renderer will never be a fenced frame.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // The target renderer will never be prerendered.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  if (started_in_foreground) {
    last_time_shown_ = navigation_handle->NavigationStart();
  }
  currently_in_foreground_ = started_in_foreground;
  if (!started_in_foreground) {
    was_hidden_ = true;
    return CONTINUE_OBSERVING;
  }

  page_transition_ = navigation_handle->GetPageTransition();
  navigation_start_time_ = base::Time::Now();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (auto* rfh = navigation_handle->GetRenderFrameHost()) {
    base::TimeTicks init_time = rfh->GetProcess()->GetLastInitTime();
    base::TimeTicks launched_time = rfh->GetProcess()->GetProcessLaunchedTime();

    if (auto* manager = GetMetricsManager()) {
      // Record the renderer process creation timing.
      manager->OnReloadButtonRendererProcessCreatedAndLaunched(init_time,
                                                               launched_time);
    }
  }

  const net::HttpResponseHeaders* response_headers =
      navigation_handle->GetResponseHeaders();
  if (response_headers) {
    main_frame_resource_has_no_store_ =
        response_headers->HasHeaderValue("cache-control", "no-store");
  }

  // The PageTransition for the navigation may be updated on commit.
  page_transition_ = navigation_handle->GetPageTransition();
  was_cached_ = navigation_handle->WasResponseCached();
  navigation_handle_timing_ = navigation_handle->GetNavigationHandleTiming();

  content::WebContents* web_contents = navigation_handle->GetWebContents();
  render_process_assignment_ = web_contents->GetPrimaryMainFrame()
                                   ->GetSiteInstance()
                                   ->GetLastProcessAssignmentOutcome();

  return CONTINUE_OBSERVING;
}

WaapUIMetricsService* InitialWebUIPageLoadMetricsObserver::service() const {
  CHECK(GetDelegate().GetWebContents()->GetBrowserContext());
  auto* profile = Profile::FromBrowserContext(
      GetDelegate().GetWebContents()->GetBrowserContext());
  // The service is null only if the profile is null or the feature is disabled.
  auto* service = WaapUIMetricsService::Get(profile);
  CHECK(service);
  return service;
}

MetricsReporter& InitialWebUIPageLoadMetricsObserver::GetMetricsReporter() {
  MetricsReporterService* service = MetricsReporterService::GetFromWebContents(
      GetDelegate().GetWebContents());
  // The service must exist for InitialWebUI web contents.
  CHECK(service);
  CHECK(service->metrics_reporter());
  return *service->metrics_reporter();
}

InitialWebUIWindowMetricsManager*
InitialWebUIPageLoadMetricsObserver::GetMetricsManager() const {
  content::WebContents* web_contents = GetDelegate().GetWebContents();
  gfx::NativeWindow window = web_contents
                                 ? web_contents->GetTopLevelNativeWindow()
                                 : gfx::NativeWindow();
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithWindow(window);
  return browser ? InitialWebUIWindowMetricsManager::From(browser) : nullptr;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::ShouldObserveScheme(
    const GURL& url) const {
  if (waap::IsForInitialWebUI(url)) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

void InitialWebUIPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::TimeTicks current_time = base::TimeTicks::Now();
  if (!was_hidden_) {
    RecordNavigationTimingMetrics();
    RecordPageLoadMetrics(current_time /* no app_background_time */);
    RecordRendererUsageMetrics();
  }
  if (GetDelegate().StartedInForeground()) {
    RecordTimingMetrics(timing);
  }
  RecordPageEndMetrics(&timing, current_time,
                       /* app_entered_background */ false);
}

void InitialWebUIPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info) {
  RecordPageEndMetrics(nullptr, base::TimeTicks(),
                       /* app_entered_background */ false);
  if (was_hidden_) {
    return;
  }

  RecordPageLoadMetrics(base::TimeTicks() /* no app_background_time */);
  RecordRendererUsageMetrics();

  // Error codes have negative values, however we log net error code enum values
  // for UMA histograms using the equivalent positive value. For consistency in
  // UKM, we convert to a positive value here.
  int64_t net_error_code = static_cast<int64_t>(failed_load_info.error) * -1;
  DCHECK_GE(net_error_code, 0);
  ukm::builders::InitialWebUIPageLoad(GetDelegate().GetPageUkmSourceId())
      .SetNet_ErrorCode_OnFailedProvisionalLoad(net_error_code)
      .SetPageTiming_NavigationToFailedProvisionalLoad(
          failed_load_info.time_to_failed_provisional_load.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void InitialWebUIPageLoadMetricsObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  if (GetDelegate().GetVisibilityTracker().currently_in_foreground() &&
      !was_hidden_) {
    total_foreground_cpu_time_ += timing.task_time;
  }
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (currently_in_foreground_ && !last_time_shown_.is_null()) {
    total_foreground_duration_ += base::TimeTicks::Now() - last_time_shown_;
  }
  currently_in_foreground_ = false;
  if (!was_hidden_) {
    RecordNavigationTimingMetrics();
    RecordPageLoadMetrics(base::TimeTicks() /* no app_background_time */);
    RecordRendererUsageMetrics();
    was_hidden_ = true;
  }

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::OnShown() {
  currently_in_foreground_ = true;
  last_time_shown_ = base::TimeTicks::Now();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::TimeTicks current_time = base::TimeTicks::Now();
  if (!was_hidden_) {
    RecordNavigationTimingMetrics();
    RecordPageLoadMetrics(current_time);
    RecordRendererUsageMetrics();
  }
  if (GetDelegate().StartedInForeground()) {
    RecordTimingMetrics(timing);
  }
  // Assume that page ends on this method, as the app could be evicted right
  // after.
  RecordPageEndMetrics(&timing, current_time,
                       /* app_entered_background */ true);
  return STOP_OBSERVING;
}

void InitialWebUIPageLoadMetricsObserver::RecordNavigationTimingMetrics() {
  const base::TimeTicks navigation_start_time =
      GetDelegate().GetNavigationStart();
  const content::NavigationHandleTiming& timing = navigation_handle_timing_;

  // Record metrics for navigation only when all relevant milestones are
  // recorded and in the expected order. It is allowed that they have the same
  // value for some cases (e.g., internal redirection for HSTS).
  if (navigation_start_time.is_null() ||
      timing.first_request_start_time.is_null() ||
      timing.first_response_start_time.is_null() ||
      timing.navigation_commit_sent_time.is_null() ||
      timing.navigation_commit_received_time.is_null() ||
      timing.navigation_commit_reply_sent_time.is_null() ||
      timing.navigation_did_commit_time.is_null()) {
    return;
  }
  if (navigation_start_time > timing.first_request_start_time ||
      timing.first_request_start_time > timing.first_response_start_time ||
      timing.first_response_start_time > timing.navigation_commit_sent_time ||
      timing.navigation_commit_sent_time >
          timing.navigation_commit_received_time ||
      timing.navigation_commit_received_time >
          timing.navigation_commit_reply_sent_time ||
      timing.navigation_commit_reply_sent_time >
          timing.navigation_did_commit_time) {
    return;
  }

  ukm::builders::InitialWebUINavigationTiming builder(
      GetDelegate().GetPageUkmSourceId());

  // Record the elapsed time from the navigation start milestone.
  builder
      .SetFirstRequestStart(
          (timing.first_request_start_time - navigation_start_time)
              .InMilliseconds())
      .SetFirstResponseStart(
          (timing.first_response_start_time - navigation_start_time)
              .InMilliseconds())
      .SetNavigationCommitSent(
          (timing.navigation_commit_sent_time - navigation_start_time)
              .InMilliseconds())
      .SetNavigationCommitReceived(
          (timing.navigation_commit_received_time - navigation_start_time)
              .InMilliseconds())
      .SetNavigationCommitReplySent(
          (timing.navigation_commit_reply_sent_time - navigation_start_time)
              .InMilliseconds())
      .SetNavigationDidCommit(
          (timing.navigation_did_commit_time - navigation_start_time)
              .InMilliseconds());

  builder.Record(ukm::UkmRecorder::Get());
}

void InitialWebUIPageLoadMetricsObserver::RecordTimingMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  ukm::builders::InitialWebUIPageLoad builder(
      GetDelegate().GetPageUkmSourceId());

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    builder.SetParseTiming_NavigationToParseStart(
        timing.parse_timing->parse_start.value().InMilliseconds());
  }

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          GetDelegate())) {
    builder.SetDocumentTiming_NavigationToDOMContentLoadedEventFired(
        timing.document_timing->dom_content_loaded_event_start.value()
            .InMilliseconds());
  }

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate())) {
    builder.SetDocumentTiming_NavigationToLoadEventFired(
        timing.document_timing->load_event_start.value().InMilliseconds());
  }

  if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
    builder.SetPaintTiming_NavigationToFirstPaint(
        timing.paint_timing->first_paint.value().InMilliseconds());
  }

  builder.SetCpuTimeMs(total_foreground_cpu_time_.InMilliseconds());

  builder.Record(ukm::UkmRecorder::Get());
}

void InitialWebUIPageLoadMetricsObserver::RecordPageLoadMetrics(
    base::TimeTicks app_background_time) {
  ukm::builders::InitialWebUIPageLoad builder(
      GetDelegate().GetPageUkmSourceId());

  std::optional<base::TimeDelta> foreground_duration =
      page_load_metrics::GetInitialForegroundDuration(GetDelegate(),
                                                      app_background_time);
  if (foreground_duration) {
    builder.SetPageTiming_ForegroundDurationMs(
        foreground_duration.value().InMilliseconds());
  }

  if (main_frame_resource_has_no_store_.has_value()) {
    builder.SetMainFrameResource_RequestHasNoStore(
        main_frame_resource_has_no_store_.value() ? 1 : 0);
  }

  if (GetDelegate().DidCommit() && was_cached_) {
    builder.SetWasCached(1);
  }

  if (GetDelegate().DidCommit()) {
    RecordPageLoadTimestampMetrics(builder);
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void InitialWebUIPageLoadMetricsObserver::RecordRendererUsageMetrics() {
  ukm::builders::InitialWebUIPageLoad builder(
      GetDelegate().GetPageUkmSourceId());

  if (render_process_assignment_) {
    builder.SetSiteInstanceRenderProcessAssignment(
        static_cast<int64_t>(render_process_assignment_.value()));
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void InitialWebUIPageLoadMetricsObserver::RecordPageLoadTimestampMetrics(
    ukm::builders::InitialWebUIPageLoad& builder) {
  DCHECK(!navigation_start_time_.is_null());

  base::Time::Exploded exploded;
  navigation_start_time_.LocalExplode(&exploded);
  builder.SetDayOfWeek(exploded.day_of_week);
  builder.SetHourOfDay(exploded.hour);
}

void InitialWebUIPageLoadMetricsObserver::RecordPageEndMetrics(
    const page_load_metrics::mojom::PageLoadTiming* timing,
    base::TimeTicks page_end_time,
    bool app_entered_background) {
  ukm::builders::InitialWebUIPageLoad builder(
      GetDelegate().GetPageUkmSourceId());
  // page_transition_ fits in a uint32_t, so we can safely cast to int64_t.
  builder.SetNavigation_PageTransition(static_cast<int64_t>(page_transition_));

  // GetDelegate().GetPageEndReason() fits in a uint32_t, so we can safely cast
  // to int64_t.
  auto page_end_reason = GetDelegate().GetPageEndReason();
  if (page_end_reason == page_load_metrics::PageEndReason::END_NONE &&
      app_entered_background) {
    page_end_reason =
        page_load_metrics::PageEndReason::END_APP_ENTER_BACKGROUND;
  }
  builder.SetNavigation_PageEndReason3(static_cast<int64_t>(page_end_reason));

  if (timing) {
    RecordAbortMetrics(*timing, page_end_time, &builder);
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void InitialWebUIPageLoadMetricsObserver::RecordAbortMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    base::TimeTicks page_end_time,
    ukm::builders::InitialWebUIPageLoad* builder) {
  if (currently_in_foreground_ && !last_time_shown_.is_null()) {
    total_foreground_duration_ += page_end_time - last_time_shown_;
  }
  builder->SetPageTiming_TotalForegroundDurationMs(
      ukm::GetSemanticBucketMinForDurationTiming(
          total_foreground_duration_.InMilliseconds()));
}
