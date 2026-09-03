// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/initial_webui_page_load_metrics_observer.h"

#include "base/barrier_closure.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/browser/ui/waap/waap_utils.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
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
#include "ui/base/page_transition_types.h"

namespace {

// LINT.IfChange(InitialWebUIRendererMilestones)
constexpr char kMarkJsResourcesLoaded[] = "JsResourcesLoaded";
constexpr char kMarkLoadTimeDataRead[] = "LoadTimeDataRead";
constexpr char kMarkJsCompositionComplete[] = "JsCompositionComplete";
// LINT.ThenChange(//chrome/browser/resources/webui_toolbar/app.ts:InitialWebUIRendererMilestones)

}  // namespace

InitialWebUIPageLoadMetricsObserver::InitialWebUIPageLoadMetricsObserver() =
    default;

InitialWebUIPageLoadMetricsObserver::~InitialWebUIPageLoadMetricsObserver() {
  base::UmaHistogramEnumeration(
      "InitialWebUI.Toolbar.WindowMetricsManagerBindingStatus",
      manager_binding_status_);
  if (manager_binding_status_ ==
      WindowMetricsManagerBindingStatus::kNeverBound) {
    base::UmaHistogramMediumTimes(
        "InitialWebUI.Toolbar.TimeWithoutWindowMetricsManagerOnDestruction",
        base::TimeTicks::Now() - creation_time_);
  }
}

const char* InitialWebUIPageLoadMetricsObserver::GetObserverName() const {
  return "InitialWebUIPageLoadMetricsObserver";
}

void InitialWebUIPageLoadMetricsObserver::OnMonotonicFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!timing.monotonic_paint_timing ||
      !timing.monotonic_paint_timing->first_paint) {
    return;
  }

  first_paint_time_ = timing.monotonic_paint_timing->first_paint.value();
  FlushMetricsToManager();
}

void InitialWebUIPageLoadMetricsObserver::OnMonotonicFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!timing.monotonic_paint_timing ||
      !timing.monotonic_paint_timing->first_contentful_paint) {
    return;
  }

  first_contentful_paint_time_ =
      timing.monotonic_paint_timing->first_contentful_paint.value();
  FlushMetricsToManager();
}

void InitialWebUIPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!timing.paint_timing || !timing.paint_timing->first_contentful_paint) {
    return;
  }
  // TODO(crbug.com/527276743): consider moving this to the
  // `OnMonotonicFirstContentfulPaintInPage()` so that the paint time is more
  // accurate, see the comments from
  // https://chromium-review.git.corp.google.com/c/chromium/src/+/7991315/comment/5d8434b4_4546404c/
  ukm::builders::InitialWebUIPageLoad builder(
      GetDelegate().GetPageUkmSourceId());
  builder.SetPaintTiming_NavigationToFirstContentfulPaint(
      timing.paint_timing->first_contentful_paint.value().InMilliseconds());
  if (timing.monotonic_paint_timing &&
      timing.monotonic_paint_timing->first_contentful_paint_submitted) {
    base::TimeTicks fcp_submitted =
        timing.monotonic_paint_timing->first_contentful_paint_submitted.value();
    base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
    builder.SetPaintTiming_FirstContentfulPaintSubmittedMs(
        (fcp_submitted - navigation_start).InMilliseconds());
  }
  builder.Record(ukm::UkmRecorder::Get());

  // FCP cannot occur before WebUI JS composition completes, because the root
  // WebUI component returns `nothing` from render() until it is initialized.
  // Therefore, renderer timing marks are guaranteed to be present by FCP time.
  RecordRendererMilestones();
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
  if (GetMetricsManager()) {
    manager_binding_status_ = WindowMetricsManagerBindingStatus::kBoundAtStart;
  }
  if (started_in_foreground) {
    last_time_shown_ = navigation_handle->NavigationStart();
  }
  currently_in_foreground_ = started_in_foreground;
  navigation_start_time_ = base::Time::Now();
  if (!started_in_foreground) {
    was_hidden_ = true;
    return CONTINUE_OBSERVING;
  }

  page_transition_ = navigation_handle->GetPageTransition();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
InitialWebUIPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  if (auto* rfh = navigation_handle->GetRenderFrameHost()) {
    base::TimeTicks init_time = rfh->GetProcess()->GetLastInitTime();
    base::TimeTicks launched_time = rfh->GetProcess()->GetProcessLaunchedTime();

    renderer_process_times_ = RendererProcessTimes{
        .init_time = init_time, .launched_time = launched_time};
    FlushMetricsToManager();
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

  GetMetricsReporter().Mark("NavigationStart",
                            GetDelegate().GetNavigationStart());

  return CONTINUE_OBSERVING;
}

WaapUIMetricsService* InitialWebUIPageLoadMetricsObserver::service() const {
  CHECK(GetDelegate().GetWebContents()->GetBrowserContext());
  auto* profile = Profile::FromBrowserContext(
      GetDelegate().GetWebContents()->GetBrowserContext());
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
  if (!web_contents) {
    return nullptr;
  }
  if (BrowserWindowInterface* browser =
          webui::GetBrowserWindowInterface(web_contents)) {
    return InitialWebUIWindowMetricsManager::From(browser);
  }
  gfx::NativeWindow window = web_contents->GetTopLevelNativeWindow();
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithWindow(window);
  return browser ? InitialWebUIWindowMetricsManager::From(browser) : nullptr;
}

void InitialWebUIPageLoadMetricsObserver::FlushMetricsToManager() {
  auto* manager = GetMetricsManager();
  if (!manager) {
    // When the manager is not ready, subscribe to BrowserWindowInterface
    // changes so that all held metrics can be flushed once it becomes
    // available.
    if (!bwi_subscription_ && GetDelegate().GetWebContents()) {
      bwi_subscription_ = webui::RegisterBrowserWindowInterfaceChanged(
          GetDelegate().GetWebContents(),
          base::BindRepeating(
              &InitialWebUIPageLoadMetricsObserver::FlushMetricsToManager,
              weak_factory_.GetWeakPtr()));
    }
    return;
  }

  if (manager_binding_status_ ==
      WindowMetricsManagerBindingStatus::kNeverBound) {
    manager_binding_status_ = WindowMetricsManagerBindingStatus::kBoundDeferred;
    base::UmaHistogramMediumTimes(
        "InitialWebUI.Toolbar.TimeToWindowMetricsManagerBound",
        base::TimeTicks::Now() - creation_time_);
  }

  if (renderer_process_times_.has_value()) {
    manager->OnReloadButtonRendererProcessCreatedAndLaunched(
        renderer_process_times_->init_time,
        renderer_process_times_->launched_time);
    renderer_process_times_.reset();
  }
  if (first_paint_time_.has_value()) {
    manager->OnReloadButtonFirstPaint(*first_paint_time_);
    first_paint_time_.reset();
  }
  if (first_contentful_paint_time_.has_value()) {
    manager->OnReloadButtonFirstContentfulPaint(*first_contentful_paint_time_);
    first_contentful_paint_time_.reset();
  }
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

  if (!metrics_recorded_ && GetDelegate().DidCommit()) {
    RecordNavigationTimingMetrics();
    RecordPageLoadMetrics(current_time /* no app_background_time */);
    RecordRendererUsageMetrics();
    metrics_recorded_ = true;
  }
  RecordTimingMetrics(timing);
  RecordPageEndMetrics(&timing, current_time,
                       /* app_entered_background */ false);
}

void InitialWebUIPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info) {
  RecordPageEndMetrics(nullptr, base::TimeTicks(),
                       /* app_entered_background */ false);
  if (!metrics_recorded_) {
    RecordPageLoadMetrics(base::TimeTicks() /* no app_background_time */);
    RecordRendererUsageMetrics();
    metrics_recorded_ = true;
  }

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
  if (!metrics_recorded_ && GetDelegate().DidCommit()) {
    RecordNavigationTimingMetrics();
    RecordPageLoadMetrics(base::TimeTicks() /* no app_background_time */);
    RecordRendererUsageMetrics();
    metrics_recorded_ = true;
  }
  was_hidden_ = true;

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

  if (!metrics_recorded_ && GetDelegate().DidCommit()) {
    RecordNavigationTimingMetrics();
    RecordPageLoadMetrics(current_time);
    RecordRendererUsageMetrics();
    metrics_recorded_ = true;
  }
  RecordTimingMetrics(timing);
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

  if (timing.parse_timing && timing.parse_timing->parse_start) {
    builder.SetParseTiming_NavigationToParseStart(
        timing.parse_timing->parse_start.value().InMilliseconds());
  }

  if (timing.document_timing &&
      timing.document_timing->dom_content_loaded_event_start) {
    builder.SetDocumentTiming_NavigationToDOMContentLoadedEventFired(
        timing.document_timing->dom_content_loaded_event_start.value()
            .InMilliseconds());
  }

  if (timing.document_timing && timing.document_timing->load_event_start) {
    builder.SetDocumentTiming_NavigationToLoadEventFired(
        timing.document_timing->load_event_start.value().InMilliseconds());
  }

  if (timing.paint_timing && timing.paint_timing->first_paint) {
    builder.SetPaintTiming_NavigationToFirstPaint(
        timing.paint_timing->first_paint.value().InMilliseconds());
  }

  builder.SetCPUTimeMs(total_foreground_cpu_time_.InMilliseconds());

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

struct InitialWebUIPageLoadMetricsObserver::TimingData {
  // Time from navigation start until WebUI JS resources finish loading.
  std::optional<base::TimeDelta> js_resources_loaded;
  // Time from navigation start until loadTimeData is read by the frontend.
  std::optional<base::TimeDelta> load_time_data_read;
  // Time from navigation start until initial JS/Lit UI composition is complete.
  std::optional<base::TimeDelta> js_composition_complete;
};

void InitialWebUIPageLoadMetricsObserver::RecordRendererMilestones() {
  auto data = std::make_unique<TimingData>();
  base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
  std::vector<std::string> mark_names = {kMarkJsResourcesLoaded,
                                         kMarkLoadTimeDataRead,
                                         kMarkJsCompositionComplete};

  // TODO(crbug.com/542030476): Replace individual Mojo queries with a single
  // batch Mojo IPC (e.g., `OnGetMarks()`) to retrieve all timestamps at once.
  // The current implementation queries marks in parallel via a BarrierClosure.
  FetchMarks(std::move(data), mark_names, navigation_start);
}

void InitialWebUIPageLoadMetricsObserver::FetchMarks(
    std::unique_ptr<TimingData> data,
    const std::vector<std::string>& mark_names,
    base::TimeTicks navigation_start) {
  if (mark_names.empty()) {
    OnRendererMilestonesRecorded(std::move(data));
    return;
  }

  TimingData* data_ptr = data.get();
  auto barrier = base::BarrierClosure(
      mark_names.size(),
      base::BindOnce(
          &InitialWebUIPageLoadMetricsObserver::OnRendererMilestonesRecorded,
          weak_factory_.GetWeakPtr(), std::move(data)));

  for (const std::string& mark_name : mark_names) {
    FetchMark(mark_name, data_ptr, navigation_start, barrier);
  }
}

void InitialWebUIPageLoadMetricsObserver::FetchMark(
    const std::string& mark_name,
    TimingData* data_ptr,
    base::TimeTicks navigation_start,
    base::OnceClosure barrier) {
  GetMetricsReporter().HasMark(
      mark_name,
      base::BindOnce(&InitialWebUIPageLoadMetricsObserver::OnMarkChecked,
                     weak_factory_.GetWeakPtr(), mark_name, data_ptr,
                     navigation_start, std::move(barrier)));
}

void InitialWebUIPageLoadMetricsObserver::OnMarkChecked(
    const std::string& mark_name,
    TimingData* data_ptr,
    base::TimeTicks navigation_start,
    base::OnceClosure barrier,
    bool has_mark) {
  if (!has_mark) {
    std::move(barrier).Run();
    return;
  }

  GetMetricsReporter().Measure(
      mark_name, navigation_start,
      base::BindOnce(&InitialWebUIPageLoadMetricsObserver::OnMarkMeasured,
                     weak_factory_.GetWeakPtr(), mark_name, data_ptr,
                     std::move(barrier)));
}

void InitialWebUIPageLoadMetricsObserver::OnMarkMeasured(
    const std::string& mark_name,
    TimingData* data_ptr,
    base::OnceClosure barrier,
    base::TimeDelta delta) {
  std::optional<base::TimeDelta> value = std::make_optional(-delta);

  if (mark_name == kMarkJsResourcesLoaded) {
    data_ptr->js_resources_loaded = value;
  } else if (mark_name == kMarkLoadTimeDataRead) {
    data_ptr->load_time_data_read = value;
  } else if (mark_name == kMarkJsCompositionComplete) {
    data_ptr->js_composition_complete = value;
  }

  std::move(barrier).Run();
}

void InitialWebUIPageLoadMetricsObserver::OnRendererMilestonesRecorded(
    std::unique_ptr<TimingData> data) {
  ukm::builders::InitialWebUIPageLoad builder(
      GetDelegate().GetPageUkmSourceId());

  if (data->js_resources_loaded) {
    builder.SetPaintTiming_JsResourcesLoadedMs(
        data->js_resources_loaded->InMilliseconds());
  }
  if (data->load_time_data_read) {
    builder.SetPaintTiming_LoadTimeDataReadMs(
        data->load_time_data_read->InMilliseconds());
  }
  if (data->js_composition_complete) {
    builder.SetPaintTiming_JsCompositionCompleteMs(
        data->js_composition_complete->InMilliseconds());
  }
  builder.Record(ukm::UkmRecorder::Get());
}
