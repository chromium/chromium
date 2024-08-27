// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/page_load_metrics/observers/core/ukm_page_load_metrics_observer.h"

#include <cmath>
#include <memory>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/hash/sha1.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_id_helper.h"
#include "base/trace_event/typed_macros.h"
#include "cc/metrics/ukm_smoothness_data.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/performance_manager/public/user_tuning/user_tuning_utils.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/metrics/metrics_data_validation.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_utils.h"
#include "components/no_state_prefetch/common/no_state_prefetch_final_status.h"
#include "components/no_state_prefetch/common/no_state_prefetch_origin.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/browser/protocol_util.h"
#include "components/page_load_metrics/common/page_visit_final_status.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "media/base/mime_util.h"
#include "net/base/load_timing_info.h"
#include "net/cookies/cookie_options.h"
#include "net/http/http_response_headers.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/performance/largest_contentful_paint_type.h"
#include "third_party/metrics_proto/system_profile.pb.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/events/blink/blink_features.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_tab_helper.h"
#endif

using page_load_metrics::PageVisitFinalStatus;

namespace {

const char kOfflinePreviewsMimeType[] = "multipart/related";

static constexpr uint64_t kInstantPageLoadEventsTraceTrackId = 14878427190820;

const char kHistogramSoftNavigationCount[] =
    "PageLoad.Expermental.SoftNavigations.Count";

template <size_t N>
uint64_t PackBytes(base::span<const uint8_t, N> bytes) {
  static_assert(N <= 8u,
                "Error: Can't pack more than 8 bytes into a uint64_t.");
  uint64_t result = 0;
  for (auto byte : bytes) {
    result = (result << 8) | byte;
  }
  return result;
}

uint64_t StrToHash64Bit(std::string_view str) {
  auto bytes = base::as_bytes(base::make_span(str));
  const base::SHA1Digest digest = base::SHA1Hash(bytes);
  return PackBytes(base::make_span(digest).subspan<0, 8>());
}

bool IsSupportedProtocol(page_load_metrics::NetworkProtocol protocol) {
  switch (protocol) {
    case page_load_metrics::NetworkProtocol::kHttp11:
      return true;
    case page_load_metrics::NetworkProtocol::kHttp2:
      return true;
    case page_load_metrics::NetworkProtocol::kQuic:
      return true;
    case page_load_metrics::NetworkProtocol::kOther:
      return false;
  }
}

bool IsDefaultSearchEngine(content::BrowserContext* browser_context,
                           const GURL& url) {
  if (!browser_context)
    return false;

  auto* template_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));

  if (!template_service)
    return false;

  return template_service->IsSearchResultsPageFromDefaultSearchProvider(url);
}

bool IsValidSearchURL(content::BrowserContext* browser_context,
                      const GURL& url) {
  if (!browser_context)
    return false;

  auto* template_service = TemplateURLServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (!template_service)
    return false;

  const TemplateURL* template_url =
      template_service->GetTemplateURLForHost(url.host());
  const SearchTermsData& search_terms_data =
      template_service->search_terms_data();

  std::u16string search_terms;
  // Is eligible if it is a valid template URL and contains search terms.
  return template_url &&
         template_url->ExtractSearchTermsFromURL(url, search_terms_data,
                                                 &search_terms) &&
         !search_terms.empty();
}

bool IsUserHomePage(content::BrowserContext* browser_context, const GURL& url) {
  if (!browser_context)
    return false;

  return url.spec() == Profile::FromBrowserContext(browser_context)
                           ->GetPrefs()
                           ->GetString(prefs::kHomePage);
}

std::unique_ptr<base::trace_event::TracedValue> CumulativeShiftScoreTraceData(
    float layout_shift_score,
    float layout_shift_score_before_input_or_scroll) {
  std::unique_ptr<base::trace_event::TracedValue> data =
      std::make_unique<base::trace_event::TracedValue>();
  data->SetDouble("layoutShiftScore", layout_shift_score);
  data->SetDouble("layoutShiftScoreBeforeInputOrScroll",
                  layout_shift_score_before_input_or_scroll);
  return data;
}

int SiteInstanceRenderProcessAssignmentToInt(
    content::SiteInstanceProcessAssignment assignment) {
  // These values are logged in UKM and should not be reordered or changed. Add
  // new values to the end and be sure to update the enum
  // |SiteInstanceProcessAssignment| in
  // //tools/metrics/histograms/enums.xml.
  switch (assignment) {
    case content::SiteInstanceProcessAssignment::UNKNOWN:
      return 0;
    case content::SiteInstanceProcessAssignment::REUSED_EXISTING_PROCESS:
      return 1;
    case content::SiteInstanceProcessAssignment::USED_SPARE_PROCESS:
      return 2;
    case content::SiteInstanceProcessAssignment::CREATED_NEW_PROCESS:
      return 3;
  }
  return 0;
}

// These are the high bounds of each bucket, in enum order. The index into this
// array is cast to an enum value when recording UKM. These should correspond to
// the upper bounds of the BitsPerPixelExponential enum in
// //tools/metrics/histograms/enums.xml.
static const double kLCPEntropyBucketThresholds[] = {
    0.0,  0.00001, 0.0001, 0.001, 0.01, 0.02, 0.03, 0.04,  0.05,   0.06,   0.07,
    0.08, 0.09,    0.1,    0.2,   0.3,  0.4,  0.5,  0.6,   0.7,    0.8,    0.9,
    1.0,  2.0,     3.0,    4.0,   5.0,  6.0,  7.0,  8.0,   9.0,    10.0,   20.0,
    30.0, 40.0,    50.0,   60.0,  70.0, 80.0, 90.0, 100.0, 1000.0, 10000.0};

int64_t CalculateLCPEntropyBucket(double bpp) {
  return std::lower_bound(std::begin(kLCPEntropyBucketThresholds),
                          std::end(kLCPEntropyBucketThresholds), bpp) -
         std::begin(kLCPEntropyBucketThresholds);
}

}  // namespace

// static
std::unique_ptr<page_load_metrics::PageLoadMetricsObserver>
UkmPageLoadMetricsObserver::CreateIfNeeded() {
  if (!ukm::UkmRecorder::Get()) {
    return nullptr;
  }
  return std::make_unique<UkmPageLoadMetricsObserver>(
      g_browser_process->network_quality_tracker());
}

UkmPageLoadMetricsObserver::UkmPageLoadMetricsObserver(
    network::NetworkQualityTracker* network_quality_tracker)
    : network_quality_tracker_(network_quality_tracker) {
  DCHECK(network_quality_tracker_);
}

UkmPageLoadMetricsObserver::~UkmPageLoadMetricsObserver() = default;

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  browser_context_ = web_contents->GetBrowserContext();
  navigation_id_ = navigation_handle->GetNavigationId();
  if (auto* clusters_helper =
          HistoryClustersTabHelper::FromWebContents(web_contents)) {
    clusters_helper->TagNavigationAsExpectingUkmNavigationComplete(
        navigation_id_);
  }

  start_url_is_default_search_ =
      IsDefaultSearchEngine(browser_context_, navigation_handle->GetURL());
  start_url_is_home_page_ =
      IsUserHomePage(browser_context_, navigation_handle->GetURL());

  was_scoped_search_like_navigation_ =
      IsValidSearchURL(browser_context_, navigation_handle->GetURL());

  if (started_in_foreground) {
    last_time_shown_ = navigation_handle->NavigationStart();
  }
  currently_in_foreground_ = started_in_foreground;

  if (!started_in_foreground) {
    was_hidden_ = true;
    return CONTINUE_OBSERVING;
  }

  // When OnStart is invoked, we don't yet know whether we're observing a web
  // page load, vs another kind of load (e.g. a download or a PDF). Thus,
  // metrics and source information should not be recorded here. Instead, we
  // store data we might want to persist in member variables below, and later
  // record UKM metrics for that data once we've confirmed that we're observing
  // a web page load.

  effective_connection_type_ =
      network_quality_tracker_->GetEffectiveConnectionType();
  http_rtt_estimate_ = network_quality_tracker_->GetHttpRTT();
  transport_rtt_estimate_ = network_quality_tracker_->GetTransportRTT();
  downstream_kbps_estimate_ =
      network_quality_tracker_->GetDownstreamThroughputKbps();
  page_transition_ = navigation_handle->GetPageTransition();
  navigation_start_time_ = base::Time::Now();
  UpdateMainFrameRequestHadCookie(
      navigation_handle->GetWebContents()->GetBrowserContext(),
      navigation_handle->GetURL());
  was_discarded_ = navigation_handle->ExistingDocumentWasDiscarded();
  refresh_rate_throttled_ =
      performance_manager::user_tuning::IsRefreshRateThrottled();

  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // Only OnLoadedResource needs observer level forwarding, but the method
  // handles only the outermost main frame case.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // PrerenderPageLoadMetricsObserver records prerendering version of metrics
  // and this PLMO can stop on prerendering.
  return STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::OnRedirect(
    content::NavigationHandle* navigation_handle) {
  main_frame_request_redirect_count_++;
  UpdateMainFrameRequestHadCookie(
      navigation_handle->GetWebContents()->GetBrowserContext(),
      navigation_handle->GetURL());

  return CONTINUE_OBSERVING;
}

void UkmPageLoadMetricsObserver::UpdateMainFrameRequestHadCookie(
    content::BrowserContext* browser_context,
    const GURL& url) {
  content::StoragePartition* partition =
      browser_context->GetStoragePartitionForUrl(url);

  partition->GetCookieManagerForBrowserProcess()->GetCookieList(
      url, net::CookieOptions::MakeAllInclusive(),
      net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(
          &UkmPageLoadMetricsObserver::OnMainFrameRequestHadCookieResult,
          weak_factory_.GetWeakPtr(), base::Time::Now()));
}

void UkmPageLoadMetricsObserver::OnMainFrameRequestHadCookieResult(
    base::Time query_start_time,
    const net::CookieAccessResultList& cookies,
    const net::CookieAccessResultList& excluded_cookies) {
  main_frame_request_had_cookies_ =
      main_frame_request_had_cookies_.value_or(false) || !cookies.empty();
}

UkmPageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  if (PageLoadMetricsObserver::ShouldObserveMimeType(mime_type) ==
          CONTINUE_OBSERVING ||
      mime_type == kOfflinePreviewsMimeType) {
    return CONTINUE_OBSERVING;
  }
  return STOP_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  if (web_contents->GetContentsMimeType() == kOfflinePreviewsMimeType) {
    if (!IsOfflinePreview(web_contents))
      return STOP_OBSERVING;
  }
  connection_info_ = navigation_handle->GetConnectionInfo();
  const net::HttpResponseHeaders* response_headers =
      navigation_handle->GetResponseHeaders();
  if (response_headers) {
    http_response_code_ = response_headers->response_code();
    main_frame_resource_has_no_store_ =
        response_headers->HasHeaderValue("cache-control", "no-store");
  }

  navigation_trigger_type_ =
      page_load_metrics::NavigationHandleUserData::InitiatorLocation::kOther;
  auto* navigation_userdata =
      page_load_metrics::NavigationHandleUserData::GetForNavigationHandle(
          *navigation_handle);
  if (navigation_userdata) {
    navigation_trigger_type_ = navigation_userdata->navigation_type();
  }

  // The PageTransition for the navigation may be updated on commit.
  page_transition_ = navigation_handle->GetPageTransition();
  was_cached_ = navigation_handle->WasResponseCached();
  navigation_handle_timing_ = navigation_handle->GetNavigationHandleTiming();
  prerender::NoStatePrefetchManager* const no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  ukm::SourceId source_id = GetDelegate().GetPageUkmSourceId();
  if (no_state_prefetch_manager) {
    prerender::RecordNoStatePrefetchMetrics(navigation_handle, source_id,
                                            no_state_prefetch_manager);
  }
  RecordGeneratedNavigationUKM(source_id, navigation_handle->GetURL());
  navigation_is_cross_process_ = !navigation_handle->IsSameProcess();
  navigation_entry_offset_ = navigation_handle->GetNavigationEntryOffset();
  main_document_sequence_number_ = web_contents->GetController()
                                       .GetLastCommittedEntry()
                                       ->GetMainFrameDocumentSequenceNumber();

  render_process_assignment_ = web_contents->GetPrimaryMainFrame()
                                   ->GetSiteInstance()
                                   ->GetLastProcessAssignmentOutcome();

  return CONTINUE_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::TimeTicks current_time = base::TimeTicks::Now();
  if (!was_hidden_) {
    RecordNavigationTimingMetrics();
    RecordPageLoadMetrics(current_time);
    RecordRendererUsageMetrics();
    RecordSiteEngagement();
  }
  if (GetDelegate().StartedInForeground())
    RecordTimingMetrics(timing);
  ReportLayoutStability();
  RecordSmoothnessMetrics();
  RecordResponsivenessMetrics();
  // Assume that page ends on this method, as the app could be evicted right
  // after.
  RecordPageEndMetrics(&timing, current_time,
                       /* app_entered_background */ true);
  return STOP_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy UkmPageLoadMetricsObserver::OnHidden(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (currently_in_foreground_ && !last_time_shown_.is_null()) {
    total_foreground_duration_ += base::TimeTicks::Now() - last_time_shown_;
  }
  currently_in_foreground_ = false;
  if (!was_hidden_) {
    RecordNavigationTimingMetrics();
    RecordPageLoadMetrics(base::TimeTicks() /* no app_background_time */);
    RecordRendererUsageMetrics();
    RecordSiteEngagement();
    was_hidden_ = true;
  }

  // Record the CLS, LCP and INP metrics when the tab is first hidden after it
  // is first shown in foreground, in case that OnComplete is not called.
  // last_time_shown_ is set when the page starts in the foreground or the page
  // becomes foregrounded.
  if (!was_hidden_after_first_show_in_foreground &&
      !last_time_shown_.is_null()) {
    ReportLayoutInstabilityAfterFirstForeground();
    ReportLargestContentfulPaintAfterFirstForeground();
    ReportResponsivenessAfterFirstForeground();
    was_hidden_after_first_show_in_foreground = true;
  }
  return CONTINUE_OBSERVING;
}

UkmPageLoadMetricsObserver::ObservePolicy
UkmPageLoadMetricsObserver::OnShown() {
  currently_in_foreground_ = true;
  last_time_shown_ = base::TimeTicks::Now();
  return CONTINUE_OBSERVING;
}

void UkmPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info) {
  RecordPageEndMetrics(nullptr, base::TimeTicks(),
                       /* app_entered_background */ false);
  if (was_hidden_)
    return;

  RecordPageLoadMetrics(base::TimeTicks() /* no app_background_time */);

  RecordRendererUsageMetrics();

  // Error codes have negative values, however we log net error code enum values
  // for UMA histograms using the equivalent positive value. For consistency in
  // UKM, we convert to a positive value here.
  int64_t net_error_code = static_cast<int64_t>(failed_load_info.error) * -1;
  DCHECK_GE(net_error_code, 0);
  ukm::builders::PageLoad(GetDelegate().GetPageUkmSourceId())
      .SetNet_ErrorCode_OnFailedProvisionalLoad(net_error_code)
      .SetPageTiming_NavigationToFailedProvisionalLoad(
          failed_load_info.time_to_failed_provisional_load.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  base::TimeTicks current_time = base::TimeTicks::Now();
  if (!was_hidden_) {
    RecordNavigationTimingMetrics();
    RecordPageLoadMetrics(current_time /* no app_background_time */);
    RecordRendererUsageMetrics();
    RecordSiteEngagement();
  }
  if (GetDelegate().StartedInForeground())
    RecordTimingMetrics(timing);
  ReportLayoutStability();
  RecordSmoothnessMetrics();
  RecordResponsivenessMetrics();
  RecordPageEndMetrics(&timing, current_time,
                       /* app_entered_background */ false);
}

void UkmPageLoadMetricsObserver::OnResourceDataUseObserved(
    content::RenderFrameHost* content,
    const std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr>&
        resources) {
  if (was_hidden_)
    return;
  for (auto const& resource : resources) {
    network_bytes_ += resource->delta_bytes;

    if (blink::IsSupportedImageMimeType(resource->mime_type)) {
      image_total_bytes_ += resource->delta_bytes;
      if (!resource->is_main_frame_resource)
        image_subframe_bytes_ += resource->delta_bytes;
    } else if (media::IsSupportedMediaMimeType(resource->mime_type) ||
               base::StartsWith(resource->mime_type, "audio/",
                                base::CompareCase::SENSITIVE) ||
               base::StartsWith(resource->mime_type, "video/",
                                base::CompareCase::SENSITIVE)) {
      media_bytes_ += resource->delta_bytes;
    }

    // Only sum body lengths for completed resources.
    if (!resource->is_complete)
      continue;
    if (blink::IsSupportedJavascriptMimeType(resource->mime_type)) {
      js_decoded_bytes_ += resource->decoded_body_length;
      if (resource->decoded_body_length > js_max_decoded_bytes_)
        js_max_decoded_bytes_ = resource->decoded_body_length;
    }
    if (resource->cache_type !=
        page_load_metrics::mojom::CacheType::kNotCached) {
      cache_bytes_ += resource->encoded_body_length;
    }
  }
}

void UkmPageLoadMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  if (was_hidden_)
    return;
  // This condition check accepts only the outermost main frame as FencedFrame
  // has RequestDestination::kFencedFrame.
  if (extra_request_complete_info.request_destination ==
      network::mojom::RequestDestination::kDocument) {
    DCHECK(!main_frame_timing_.has_value());
    main_frame_timing_ = *extra_request_complete_info.load_timing_info;
  }
}

void UkmPageLoadMetricsObserver::RecordNavigationTimingMetrics() {
  const base::TimeTicks navigation_start_time =
      GetDelegate().GetNavigationStart();
  const content::NavigationHandleTiming& timing = navigation_handle_timing_;

  // Record metrics for navigation only when all relevant milestones are
  // recorded and in the expected order. It is allowed that they have the same
  // value for some cases (e.g., internal redirection for HSTS).
  if (navigation_start_time.is_null() ||
      timing.first_request_start_time.is_null() ||
      timing.first_response_start_time.is_null() ||
      timing.first_loader_callback_time.is_null() ||
      timing.final_request_start_time.is_null() ||
      timing.final_response_start_time.is_null() ||
      timing.final_loader_callback_time.is_null() ||
      timing.navigation_commit_sent_time.is_null()) {
    return;
  }
  // TODO(crbug.com/40688345): Change these early-returns to DCHECKs
  // after the issue 1076710 is fixed.
  if (navigation_start_time > timing.first_request_start_time ||
      timing.first_request_start_time > timing.first_response_start_time ||
      timing.first_response_start_time > timing.first_loader_callback_time ||
      timing.first_loader_callback_time > timing.navigation_commit_sent_time) {
    return;
  }
  if (navigation_start_time > timing.final_request_start_time ||
      timing.final_request_start_time > timing.final_response_start_time ||
      timing.final_response_start_time > timing.final_loader_callback_time ||
      timing.final_loader_callback_time > timing.navigation_commit_sent_time) {
    return;
  }
  DCHECK_LE(timing.first_request_start_time, timing.final_request_start_time);
  DCHECK_LE(timing.first_response_start_time, timing.final_response_start_time);
  DCHECK_LE(timing.first_loader_callback_time,
            timing.final_loader_callback_time);

  ukm::builders::NavigationTiming builder(GetDelegate().GetPageUkmSourceId());

  // Record the elapsed time from the navigation start milestone.
  builder
      .SetFirstRequestStart(
          (timing.first_request_start_time - navigation_start_time)
              .InMilliseconds())
      .SetFirstResponseStart(
          (timing.first_response_start_time - navigation_start_time)
              .InMilliseconds())
      .SetFirstLoaderCallback(
          (timing.first_loader_callback_time - navigation_start_time)
              .InMilliseconds())
      .SetFinalRequestStart(
          (timing.final_request_start_time - navigation_start_time)
              .InMilliseconds())
      .SetFinalResponseStart(
          (timing.final_response_start_time - navigation_start_time)
              .InMilliseconds())
      .SetFinalLoaderCallback(
          (timing.final_loader_callback_time - navigation_start_time)
              .InMilliseconds())
      .SetNavigationCommitSent(
          (timing.navigation_commit_sent_time - navigation_start_time)
              .InMilliseconds());

  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate()))
    return;

  DCHECK(timing.paint_timing->first_contentful_paint.has_value());
  first_contentful_paint_ = timing.paint_timing->first_contentful_paint.value();

  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());
  builder.SetPaintTiming_NavigationToFirstContentfulPaint(
      timing.paint_timing->first_contentful_paint.value().InMilliseconds());
  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::RecordSiteEngagement() const {
  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());

  std::optional<int64_t> rounded_site_engagement_score =
      GetRoundedSiteEngagementScore();
  if (rounded_site_engagement_score) {
    builder.SetSiteEngagementScore(rounded_site_engagement_score.value());
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::RecordSoftNavigationMetrics(
    ukm::SourceId ukm_source_id,
    page_load_metrics::mojom::SoftNavigationMetrics& soft_navigation_metrics) {
  ukm::builders::SoftNavigation builder(ukm_source_id);
  builder.SetNavigationId(
      StrToHash64Bit(soft_navigation_metrics.navigation_id));

  builder.SetStartTime(soft_navigation_metrics.start_time.InMillisecondsF());

  auto largest_contentful_paint = GetSoftNavigationLargestContentfulPaint();

  if (largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    builder.SetPaintTiming_LargestContentfulPaint(
        largest_contentful_paint.Time().value().InMilliseconds());
    PAGE_LOAD_HISTOGRAM("PageLoad.SoftNavigation.LargestContentfulPaint",
                        largest_contentful_paint.Time().value());

    builder.SetPaintTiming_LargestContentfulPaintType(
        LargestContentfulPaintTypeToUKMFlags(largest_contentful_paint.Type()));

    if (largest_contentful_paint.TextOrImage() ==
        page_load_metrics::ContentfulPaintTimingInfo::
            LargestContentTextOrImage::kImage) {
      builder.SetPaintTiming_LargestContentfulPaintBPP(
          CalculateLCPEntropyBucket(largest_contentful_paint.ImageBPP()));

      auto priority = largest_contentful_paint.ImageRequestPriority();

      if (priority) {
        builder.SetPaintTiming_LargestContentfulPaintRequestPriority(*priority);
      }

      if (largest_contentful_paint.ImageDiscoveryTime().has_value()) {
        builder.SetPaintTiming_LargestContentfulPaintImageDiscoveryTime(
            largest_contentful_paint.ImageDiscoveryTime()
                .value()
                .InMilliseconds());
      }

      if (largest_contentful_paint.ImageLoadStart().has_value()) {
        builder.SetPaintTiming_LargestContentfulPaintImageLoadStart(
            largest_contentful_paint.ImageLoadStart().value().InMilliseconds());
      }

      if (largest_contentful_paint.ImageLoadEnd().has_value()) {
        builder.SetPaintTiming_LargestContentfulPaintImageLoadEnd(
            largest_contentful_paint.ImageLoadEnd().value().InMilliseconds());
      }
    }
  }

  const page_load_metrics::ResponsivenessMetricsNormalization&
      soft_nav_responsiveness_metrics_normalization =
          GetDelegate()
              .GetSoftNavigationIntervalResponsivenessMetricsNormalization();

  std::optional<page_load_metrics::mojom::UserInteractionLatency> inp =
      soft_nav_responsiveness_metrics_normalization.ApproximateHighPercentile();
  if (inp.has_value()) {
    builder
        .SetInteractiveTiming_UserInteractionLatency_HighPercentile2_MaxEventDuration(
            inp->interaction_latency.InMilliseconds());

    UmaHistogramCustomTimes("PageLoad.SoftNavigation.InteractionToNextPaint",
                            inp->interaction_latency, base::Milliseconds(1),
                            base::Seconds(60), 50);

    // For soft navigations, the interaction offset is the offset _after_ the
    // soft navigation occurred. So we want to start the offset at the number
    // of interactions which had occurred before this soft navigation.
    const page_load_metrics::ResponsivenessMetricsNormalization&
        responsiveness_metrics_normalization =
            GetDelegate().GetResponsivenessMetricsNormalization();
    uint64_t previous_interaction_count =
        (responsiveness_metrics_normalization.num_user_interactions() -
         soft_nav_responsiveness_metrics_normalization.num_user_interactions());
    builder.SetInteractiveTiming_INPOffset(inp->interaction_offset -
                                           previous_interaction_count);
    // For soft navigations, the interaction time should be reported as the
    // TimeDelta between the interaction and the soft navigation start. Since
    // the interaction time is a TimeTicks and the soft navigation start_time is
    // a TimeDelta from navigation_start, we need to add the navigation start
    // TimeTicks to the soft_navigation start_time TimeDeltat and then subtract
    // that from the interaction_time TimeTicks.
    base::TimeDelta interaction_time =
        inp->interaction_time - (GetDelegate().GetNavigationStart() +
                                 soft_navigation_metrics.start_time);
    builder.SetInteractiveTiming_INPTime(interaction_time.InMilliseconds());
    builder.SetInteractiveTiming_NumInteractions(
        ukm::GetExponentialBucketMinForCounts1000(
            soft_nav_responsiveness_metrics_normalization
                .num_user_interactions()));
  }

  // Don't report CLS if we were never in the foreground.
  if (!last_time_shown_.is_null()) {
    const std::optional<float> cwv_cls_value =
        GetCoreWebVitalsSoftNavigationIntervalCLS();
    if (cwv_cls_value.has_value()) {
      builder
          .SetLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000ms(
              page_load_metrics::LayoutShiftUkmValue(*cwv_cls_value));
      // Report UMA using same binning as all WebVitals.CumulativeLayoutShift
      // histograms; the binning ensures changes close to zero can accurately
      // be measured.
      base::UmaHistogramCustomCounts(
          "PageLoad.SoftNavigation.CumulativeLayoutShift",
          page_load_metrics::LayoutShiftUmaValue10000(*cwv_cls_value), 1, 24000,
          50);
    }
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::
    RecordResponsivenessMetricsBeforeSoftNavigationForMainFrame() {
  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());
  const page_load_metrics::ResponsivenessMetricsNormalization&
      responsiveness_metrics_normalization_before_soft_nav =
          GetDelegate()
              .GetSoftNavigationIntervalResponsivenessMetricsNormalization();
  std::optional<page_load_metrics::mojom::UserInteractionLatency> inp =
      responsiveness_metrics_normalization_before_soft_nav
          .ApproximateHighPercentile();
  if (inp.has_value()) {
    builder
        .SetInteractiveTimingBeforeSoftNavigation_UserInteractionLatency_HighPercentile2_MaxEventDuration(
            inp->interaction_latency.InMilliseconds());
    UmaHistogramCustomTimes(
        "PageLoad.BeforeSoftNavigation.InteractionToNextPaint",
        inp->interaction_latency, base::Milliseconds(1), base::Seconds(60), 50);
    builder.SetInteractiveTimingBeforeSoftNavigation_INPOffset(
        inp->interaction_offset);
    base::TimeDelta interaction_time =
        inp->interaction_time - GetDelegate().GetNavigationStart();
    builder.SetInteractiveTimingBeforeSoftNavigation_INPTime(
        interaction_time.InMilliseconds());
    builder.SetInteractiveTimingBeforeSoftNavigation_NumInteractions(
        ukm::GetExponentialBucketMinForCounts1000(
            responsiveness_metrics_normalization_before_soft_nav
                .num_user_interactions()));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::
    RecordLayoutShiftBeforeSoftNavigationForMainFrame() {
  // Don't report CLS if we were never in the foreground.
  if (last_time_shown_.is_null()) {
    return;
  }

  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());

  const std::optional<float> cwv_cls_value =
      GetCoreWebVitalsSoftNavigationIntervalCLS();

  if (cwv_cls_value.has_value()) {
    builder
        .SetLayoutInstabilityBeforeSoftNavigation_MaxCumulativeShiftScore_MainFrame_SessionWindow_Gap1000ms_Max5000ms(
            page_load_metrics::LayoutShiftUkmValue(*cwv_cls_value));
    // Report UMA using same binning as all WebVitals.CumulativeLayoutShift
    // histograms; the binning ensures changes close to zero can accurately
    // be measured.
    base::UmaHistogramCustomCounts(
        "PageLoad.BeforeSoftNavigation.CumulativeLayoutShift",
        page_load_metrics::LayoutShiftUmaValue10000(*cwv_cls_value), 1, 24000,
        50);
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::OnSoftNavigationUpdated(
    const page_load_metrics::mojom::SoftNavigationMetrics&
        new_soft_navigation_metrics) {
  auto current_soft_navigation_metrics =
      GetDelegate().GetSoftNavigationMetrics().Clone();

  // When the 1st soft navigation comes in, we record the
  // soft_navigation_interval_responsiveness_metrics_normalization_ as INP
  // before soft nav.
  if (current_soft_navigation_metrics->count == 0 &&
      new_soft_navigation_metrics.count == 1) {
    RecordResponsivenessMetricsBeforeSoftNavigationForMainFrame();
    RecordLayoutShiftBeforeSoftNavigationForMainFrame();
  }

  // Record current soft navigation metrics into Ukm when a new soft navigation
  // comes in. For example, when 2nd soft navigation with a larger count comes
  // in, the 1st(current) soft metrics are recorded. The initial soft
  // navigation metrics that have default values should not reported.
  if (current_soft_navigation_metrics->count == 0 ||
      current_soft_navigation_metrics->count >=
          new_soft_navigation_metrics.count) {
    return;
  }

  RecordSoftNavigationMetrics(
      GetDelegate().GetPreviousUkmSourceIdForSoftNavigation(),
      *current_soft_navigation_metrics);
}

const page_load_metrics::ContentfulPaintTimingInfo&
UkmPageLoadMetricsObserver::GetSoftNavigationLargestContentfulPaint() const {
  return GetDelegate()
      .GetLargestContentfulPaintHandler()
      .GetSoftNavigationLargestContentfulPaint();
}

const page_load_metrics::ContentfulPaintTimingInfo&
UkmPageLoadMetricsObserver::GetCoreWebVitalsLcpTimingInfo() {
  return GetDelegate()
      .GetLargestContentfulPaintHandler()
      .MergeMainFrameAndSubframes();
}

void UkmPageLoadMetricsObserver::RecordTimingMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());

  if (timing.input_to_navigation_start) {
    builder.SetExperimental_InputToNavigationStart(
        timing.input_to_navigation_start.value().InMilliseconds());
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_start, GetDelegate())) {
    builder.SetParseTiming_NavigationToParseStart(
        timing.parse_timing->parse_start.value().InMilliseconds());
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->dom_content_loaded_event_start,
          GetDelegate())) {
    builder.SetDocumentTiming_NavigationToDOMContentLoadedEventFired(
        timing.document_timing->dom_content_loaded_event_start.value()
            .InMilliseconds());
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, GetDelegate())) {
    builder.SetDocumentTiming_NavigationToLoadEventFired(
        timing.document_timing->load_event_start.value().InMilliseconds());
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_paint, GetDelegate())) {
    builder.SetPaintTiming_NavigationToFirstPaint(
        timing.paint_timing->first_paint.value().InMilliseconds());
  }

  // FCP is reported in OnFirstContentfulPaintInPage.

  const page_load_metrics::ContentfulPaintTimingInfo&
      main_frame_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MainFrameLargestContentfulPaint();
  if (main_frame_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          main_frame_largest_contentful_paint.Time(), GetDelegate())) {
    builder.SetPaintTiming_NavigationToLargestContentfulPaint2_MainFrame(
        main_frame_largest_contentful_paint.Time().value().InMilliseconds());
  }

  const page_load_metrics::ContentfulPaintTimingInfo& cwv_lcp_timing_info =
      GetCoreWebVitalsLcpTimingInfo();
  if (cwv_lcp_timing_info.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          cwv_lcp_timing_info.Time(), GetDelegate())) {
    builder.SetPaintTiming_NavigationToLargestContentfulPaint2(
        cwv_lcp_timing_info.Time().value().InMilliseconds());
    builder.SetPaintTiming_LargestContentfulPaintType(
        LargestContentfulPaintTypeToUKMFlags(cwv_lcp_timing_info.Type()));
    if (cwv_lcp_timing_info.TextOrImage() ==
        page_load_metrics::ContentfulPaintTimingInfo::
            LargestContentTextOrImage::kImage) {
      builder.SetPaintTiming_LargestContentfulPaintBPP(
          CalculateLCPEntropyBucket(cwv_lcp_timing_info.ImageBPP()));
      auto priority = cwv_lcp_timing_info.ImageRequestPriority();
      if (priority)
        builder.SetPaintTiming_LargestContentfulPaintRequestPriority(*priority);
      bool is_cross_origin = cwv_lcp_timing_info.Type() ==
                             (cwv_lcp_timing_info.Type() |
                              blink::LargestContentfulPaintType::kCrossOrigin);
      builder.SetPaintTiming_LargestContentfulPaintImageIsCrossOrigin(
          is_cross_origin);
    }
    if (cwv_lcp_timing_info.ImageDiscoveryTime().has_value()) {
      builder.SetPaintTiming_LargestContentfulPaintImageDiscoveryTime(
          cwv_lcp_timing_info.ImageDiscoveryTime().value().InMilliseconds());
    }
    if (cwv_lcp_timing_info.ImageLoadStart().has_value()) {
      builder.SetPaintTiming_LargestContentfulPaintImageLoadStart(
          cwv_lcp_timing_info.ImageLoadStart().value().InMilliseconds());
    }
    if (cwv_lcp_timing_info.ImageLoadEnd().has_value()) {
      builder.SetPaintTiming_LargestContentfulPaintImageLoadEnd(
          cwv_lcp_timing_info.ImageLoadEnd().value().InMilliseconds());
    }
  }
  RecordInternalTimingMetrics(cwv_lcp_timing_info);

  const page_load_metrics::ContentfulPaintTimingInfo&
      cross_site_sub_frame_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .CrossSiteSubframesLargestContentfulPaint();
  if (cross_site_sub_frame_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          cross_site_sub_frame_largest_contentful_paint.Time(),
          GetDelegate())) {
    builder
        .SetPaintTiming_NavigationToLargestContentfulPaint2_CrossSiteSubFrame(
            cross_site_sub_frame_largest_contentful_paint.Time()
                .value()
                .InMilliseconds());
  }
  if (timing.interactive_timing->first_input_delay &&
      WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    base::TimeDelta first_input_delay =
        timing.interactive_timing->first_input_delay.value();
    builder.SetInteractiveTiming_FirstInputDelay4(
        first_input_delay.InMilliseconds());
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    base::TimeDelta first_input_timestamp =
        timing.interactive_timing->first_input_timestamp.value();
    builder.SetInteractiveTiming_FirstInputTimestamp4(
        first_input_timestamp.InMilliseconds());
  }

  if (timing.interactive_timing->first_scroll_delay &&
      WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_scroll_timestamp, GetDelegate())) {
    base::TimeDelta first_scroll_delay =
        timing.interactive_timing->first_scroll_delay.value();
    builder.SetInteractiveTiming_FirstScrollDelay(
        first_scroll_delay.InMilliseconds());
  }
  if (timing.interactive_timing->first_scroll_timestamp &&
      WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_scroll_timestamp, GetDelegate())) {
    base::TimeDelta first_scroll_timestamp =
        timing.interactive_timing->first_scroll_timestamp.value();
    builder.SetInteractiveTiming_FirstScrollTimestamp(
        ukm::GetExponentialBucketMinForUserTiming(
            first_scroll_timestamp.InMilliseconds()));
  }

  if (timing.user_timing_mark_fully_loaded) {
    builder.SetPageTiming_UserTimingMarkFullyLoaded(
        timing.user_timing_mark_fully_loaded.value().InMilliseconds());
    EmitUserTimingEvent(timing.user_timing_mark_fully_loaded.value(),
                        "PageLoadMetrics.UserTimingMarkFullyLoaded");
  }
  if (timing.user_timing_mark_fully_visible) {
    builder.SetPageTiming_UserTimingMarkFullyVisible(
        timing.user_timing_mark_fully_visible.value().InMilliseconds());
    EmitUserTimingEvent(timing.user_timing_mark_fully_visible.value(),
                        "PageLoadMetrics.UserTimingMarkFullyVisible");
  }
  if (timing.user_timing_mark_interactive) {
    builder.SetPageTiming_UserTimingMarkInteractive(
        timing.user_timing_mark_interactive.value().InMilliseconds());
    EmitUserTimingEvent(timing.user_timing_mark_interactive.value(),
                        "PageLoadMetrics.UserTimingMarkInteractive");
  }
  builder.SetCpuTime(total_foreground_cpu_time_.InMilliseconds());

  builder.SetNet_CacheBytes2(
      ukm::GetExponentialBucketMinForBytes(cache_bytes_));
  builder.SetNet_NetworkBytes2(
      ukm::GetExponentialBucketMinForBytes(network_bytes_));

  builder.SetNet_JavaScriptBytes2(
      ukm::GetExponentialBucketMinForBytes(js_decoded_bytes_));
  builder.SetNet_JavaScriptMaxBytes2(
      ukm::GetExponentialBucketMinForBytes(js_max_decoded_bytes_));

  builder.SetNet_ImageBytes2(
      ukm::GetExponentialBucketMinForBytes(image_total_bytes_));
  builder.SetNet_ImageSubframeBytes2(
      ukm::GetExponentialBucketMinForBytes(image_subframe_bytes_));
  builder.SetNet_MediaBytes2(
      ukm::GetExponentialBucketMinForBytes(media_bytes_));

  builder.SetSoftNavigationCount(
      GetDelegate().GetSoftNavigationMetrics().count);

  if (main_frame_timing_)
    ReportMainResourceTimingMetrics(builder);

  builder.Record(ukm::UkmRecorder::Get());

  // Record last soft navigation metrics.
  if (GetDelegate().GetSoftNavigationMetrics().count >= 1 &&
      !GetDelegate().GetSoftNavigationMetrics().navigation_id.empty()) {
    RecordSoftNavigationMetrics(GetDelegate().GetUkmSourceIdForSoftNavigation(),
                                GetDelegate().GetSoftNavigationMetrics());
  }

  // Record soft navigation count histogram to UMA.
  base::UmaHistogramCounts100(kHistogramSoftNavigationCount,
                              GetDelegate().GetSoftNavigationMetrics().count);
}

void UkmPageLoadMetricsObserver::RecordInternalTimingMetrics(
    const page_load_metrics::ContentfulPaintTimingInfo&
        all_frames_largest_contentful_paint) {
  ukm::builders::PageLoad_Internal debug_builder(
      GetDelegate().GetPageUkmSourceId());
  LargestContentState lcp_state = LargestContentState::kNotFound;
  if (all_frames_largest_contentful_paint.ContainsValidTime()) {
    if (WasStartedInForegroundOptionalEventInForeground(
            all_frames_largest_contentful_paint.Time(), GetDelegate())) {
      debug_builder.SetPaintTiming_LargestContentfulPaint_ContentType(
          static_cast<int>(all_frames_largest_contentful_paint.TextOrImage()));
      lcp_state = LargestContentState::kReported;
    } else {
      // This can be reached if LCP occurs after tab hide.
      lcp_state = LargestContentState::kFoundButNotReported;
    }
  } else if (all_frames_largest_contentful_paint.Time().has_value()) {
    DCHECK(all_frames_largest_contentful_paint.Size());
    lcp_state = LargestContentState::kLargestImageLoading;
  } else {
    DCHECK(all_frames_largest_contentful_paint.Empty());
    lcp_state = LargestContentState::kNotFound;
  }
  debug_builder.SetPaintTiming_LargestContentfulPaint_TerminationState(
      static_cast<int>(lcp_state));
  debug_builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::RecordPageLoadMetrics(
    base::TimeTicks app_background_time) {
  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());

  std::optional<bool> third_party_cookie_blocking_enabled =
      GetThirdPartyCookieBlockingEnabled();
  if (third_party_cookie_blocking_enabled) {
    builder.SetThirdPartyCookieBlockingEnabledForSite(
        third_party_cookie_blocking_enabled.value());
  }

  std::optional<base::TimeDelta> foreground_duration =
      page_load_metrics::GetInitialForegroundDuration(GetDelegate(),
                                                      app_background_time);
  if (foreground_duration) {
    builder.SetPageTiming_ForegroundDuration(
        foreground_duration.value().InMilliseconds());
  }

  // Convert to the EffectiveConnectionType as used in SystemProfileProto
  // before persisting the metric.
  metrics::SystemProfileProto::Network::EffectiveConnectionType
      proto_effective_connection_type =
          metrics::ConvertEffectiveConnectionType(effective_connection_type_);
  if (proto_effective_connection_type !=
      metrics::SystemProfileProto::Network::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
    builder.SetNet_EffectiveConnectionType2_OnNavigationStart(
        static_cast<int64_t>(proto_effective_connection_type));
  }

  if (http_response_code_) {
    builder.SetNet_HttpResponseCode(
        static_cast<int64_t>(http_response_code_.value()));
  }
  if (http_rtt_estimate_) {
    builder.SetNet_HttpRttEstimate_OnNavigationStart(
        static_cast<int64_t>(http_rtt_estimate_.value().InMilliseconds()));
  }
  if (transport_rtt_estimate_) {
    builder.SetNet_TransportRttEstimate_OnNavigationStart(
        static_cast<int64_t>(transport_rtt_estimate_.value().InMilliseconds()));
  }
  if (downstream_kbps_estimate_) {
    builder.SetNet_DownstreamKbpsEstimate_OnNavigationStart(
        static_cast<int64_t>(downstream_kbps_estimate_.value()));
  }

  if (main_frame_resource_has_no_store_.has_value()) {
    builder.SetMainFrameResource_RequestHasNoStore(
        main_frame_resource_has_no_store_.value() ? 1 : 0);
  }

  if (GetDelegate().DidCommit() && was_cached_) {
    builder.SetWasCached(1);
  }
  if (GetDelegate().DidCommit() && was_discarded_) {
    builder.SetWasDiscarded(true);
  }
  if (GetDelegate().DidCommit() && refresh_rate_throttled_) {
    builder.SetRefreshRateThrottled(true);
  }
  if (GetDelegate().DidCommit() && navigation_is_cross_process_) {
    builder.SetIsCrossProcessNavigation(navigation_is_cross_process_);
  }
  if (GetDelegate().DidCommit()) {
    builder.SetNavigationEntryOffset(navigation_entry_offset_);
    builder.SetMainDocumentSequenceNumber(main_document_sequence_number_);
    RecordPageLoadTimestampMetrics(builder);
  }

  if (GetDelegate().DidCommit()) {
    builder.SetIsScopedSearchLikeNavigation(was_scoped_search_like_navigation_);
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::RecordRendererUsageMetrics() {
  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());

  if (render_process_assignment_) {
    builder.SetSiteInstanceRenderProcessAssignment(
        SiteInstanceRenderProcessAssignmentToInt(
            render_process_assignment_.value()));
  }

  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::ReportMainResourceTimingMetrics(
    ukm::builders::PageLoad& builder) {
  DCHECK(main_frame_timing_.has_value());

  builder.SetMainFrameResource_SocketReused(main_frame_timing_->socket_reused);

  int64_t dns_start_ms =
      main_frame_timing_->connect_timing.domain_lookup_start.since_origin()
          .InMilliseconds();
  int64_t dns_end_ms =
      main_frame_timing_->connect_timing.domain_lookup_end.since_origin()
          .InMilliseconds();
  int64_t connect_start_ms =
      main_frame_timing_->connect_timing.connect_start.since_origin()
          .InMilliseconds();
  int64_t connect_end_ms =
      main_frame_timing_->connect_timing.connect_end.since_origin()
          .InMilliseconds();
  int64_t request_start_ms =
      main_frame_timing_->request_start.since_origin().InMilliseconds();
  int64_t send_start_ms =
      main_frame_timing_->send_start.since_origin().InMilliseconds();
  int64_t receive_headers_end_ms =
      main_frame_timing_->receive_headers_end.since_origin().InMilliseconds();

  DCHECK_LE(dns_start_ms, dns_end_ms);
  DCHECK_LE(dns_end_ms, connect_start_ms);
  DCHECK_LE(dns_start_ms, connect_start_ms);
  DCHECK_LE(connect_start_ms, connect_end_ms);

  int64_t dns_duration_ms = dns_end_ms - dns_start_ms;
  int64_t connect_duration_ms = connect_end_ms - connect_start_ms;
  int64_t request_start_to_send_start_ms = send_start_ms - request_start_ms;
  int64_t send_start_to_receive_headers_end_ms =
      receive_headers_end_ms - send_start_ms;
  int64_t request_start_to_receive_headers_end_ms =
      receive_headers_end_ms - request_start_ms;

  builder.SetMainFrameResource_DNSDelay(dns_duration_ms);
  builder.SetMainFrameResource_ConnectDelay(connect_duration_ms);
  if (request_start_to_send_start_ms >= 0) {
    builder.SetMainFrameResource_RequestStartToSendStart(
        request_start_to_send_start_ms);
  }
  if (send_start_to_receive_headers_end_ms >= 0) {
    builder.SetMainFrameResource_SendStartToReceiveHeadersEnd(
        send_start_to_receive_headers_end_ms);
  }
  builder.SetMainFrameResource_RequestStartToReceiveHeadersEnd(
      request_start_to_receive_headers_end_ms);

  if (!main_frame_timing_->connect_timing.connect_start.is_null() &&
      !GetDelegate().GetNavigationStart().is_null()) {
    base::TimeDelta navigation_start_to_connect_start =
        main_frame_timing_->connect_timing.connect_start -
        GetDelegate().GetNavigationStart();

    builder.SetMainFrameResource_NavigationStartToConnectStart(
        navigation_start_to_connect_start.InMilliseconds());
  }

  if (!main_frame_timing_->request_start.is_null() &&
      !GetDelegate().GetNavigationStart().is_null()) {
    base::TimeDelta navigation_start_to_request_start =
        main_frame_timing_->request_start - GetDelegate().GetNavigationStart();

    builder.SetMainFrameResource_NavigationStartToRequestStart(
        navigation_start_to_request_start.InMilliseconds());
  }

  if (!main_frame_timing_->receive_headers_start.is_null() &&
      !GetDelegate().GetNavigationStart().is_null()) {
    base::TimeDelta navigation_start_to_receive_headers_start =
        main_frame_timing_->receive_headers_start -
        GetDelegate().GetNavigationStart();
    builder.SetMainFrameResource_NavigationStartToReceiveHeadersStart(
        navigation_start_to_receive_headers_start.InMilliseconds());
  }

  if (connection_info_.has_value()) {
    page_load_metrics::NetworkProtocol protocol =
        page_load_metrics::GetNetworkProtocol(*connection_info_);
    if (IsSupportedProtocol(protocol)) {
      builder.SetMainFrameResource_HttpProtocolScheme(
          static_cast<int>(protocol));
    }
  }

  if (main_frame_request_redirect_count_ > 0) {
    builder.SetMainFrameResource_RedirectCount(
        main_frame_request_redirect_count_);
  }
  if (main_frame_request_had_cookies_.has_value()) {
    builder.SetMainFrameResource_RequestHadCookies(
        main_frame_request_had_cookies_.value() ? 1 : 0);
  }
}

std::optional<float> UkmPageLoadMetricsObserver::GetCoreWebVitalsCLS() {
  const page_load_metrics::NormalizedCLSData& normalized_cls_data =
      GetDelegate().GetNormalizedCLSData(
          page_load_metrics::PageLoadMetricsObserverDelegate::BfcacheStrategy::
              ACCUMULATE);
  if (!normalized_cls_data.data_tainted) {
    return normalized_cls_data.session_windows_gap1000ms_max5000ms_max_cls;
  }
  return std::nullopt;
}

std::optional<float>
UkmPageLoadMetricsObserver::GetCoreWebVitalsSoftNavigationIntervalCLS() {
  const page_load_metrics::NormalizedCLSData& normalized_cls_data =
      GetDelegate().GetSoftNavigationIntervalNormalizedCLSData();
  if (!normalized_cls_data.data_tainted) {
    return normalized_cls_data.session_windows_gap1000ms_max5000ms_max_cls;
  }
  return std::nullopt;
}

void UkmPageLoadMetricsObserver::ReportLayoutStability() {
  // Don't report CLS if we were never in the foreground.
  if (last_time_shown_.is_null())
    return;

  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());
  builder
      .SetLayoutInstability_CumulativeShiftScore(
          page_load_metrics::LayoutShiftUkmValue(
              GetDelegate().GetPageRenderData().layout_shift_score))
      .SetLayoutInstability_CumulativeShiftScore_BeforeInputOrScroll(
          page_load_metrics::LayoutShiftUkmValue(
              GetDelegate()
                  .GetPageRenderData()
                  .layout_shift_score_before_input_or_scroll))
      .SetLayoutInstability_CumulativeShiftScore_MainFrame(
          page_load_metrics::LayoutShiftUkmValue(
              GetDelegate().GetMainFrameRenderData().layout_shift_score))
      .SetLayoutInstability_CumulativeShiftScore_MainFrame_BeforeInputOrScroll(
          page_load_metrics::LayoutShiftUkmValue(
              GetDelegate()
                  .GetMainFrameRenderData()
                  .layout_shift_score_before_input_or_scroll));

  const std::optional<float> cwv_cls_value = GetCoreWebVitalsCLS();
  if (cwv_cls_value.has_value()) {
    builder
        .SetLayoutInstability_MaxCumulativeShiftScore_SessionWindow_Gap1000ms_Max5000ms(
            page_load_metrics::LayoutShiftUkmValue(*cwv_cls_value));
    base::UmaHistogramCustomCounts(
        "PageLoad.LayoutInstability.MaxCumulativeShiftScore.SessionWindow."
        "Gap1000ms.Max5000ms2",
        page_load_metrics::LayoutShiftUmaValue10000(*cwv_cls_value), 1, 24000,
        50);
    // The pseudo metric of PageLoad.LayoutInstability.MaxCumulativeShiftScore.
    // SessionWindow.Gap1000ms.Max5000ms2.
    // Only used to assess field trial data quality.
    base::UmaHistogramCustomCounts(
        "UMA.Pseudo.PageLoad.LayoutInstability.MaxCumulativeShiftScore."
        "SessionWindow.Gap1000ms.Max5000ms2",
        page_load_metrics::LayoutShiftUmaValue10000(
            metrics::GetPseudoMetricsSample(*cwv_cls_value)),
        1, 24000, 50);
  }
  builder.Record(ukm::UkmRecorder::Get());

  // TODO(crbug.com/40681312): We should move UMA recording to components/

  const float layout_shift_score =
      GetDelegate().GetPageRenderData().layout_shift_score;
  base::UmaHistogramCounts100(
      "PageLoad.LayoutInstability.CumulativeShiftScore",
      page_load_metrics::LayoutShiftUmaValue(layout_shift_score));
  // The pseudo metric of PageLoad.LayoutInstability.CumulativeShiftScore. Only
  // used to assess field trial data quality.
  base::UmaHistogramCounts100(
      "UMA.Pseudo.PageLoad.LayoutInstability.CumulativeShiftScore",
      page_load_metrics::LayoutShiftUmaValue(
          metrics::GetPseudoMetricsSample(layout_shift_score)));

  TRACE_EVENT_INSTANT1("loading", "CumulativeShiftScore::AllFrames::UMA",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       CumulativeShiftScoreTraceData(
                           GetDelegate().GetPageRenderData().layout_shift_score,
                           GetDelegate()
                               .GetPageRenderData()
                               .layout_shift_score_before_input_or_scroll));

  base::UmaHistogramCounts100(
      "PageLoad.LayoutInstability.CumulativeShiftScore.MainFrame",
      page_load_metrics::LayoutShiftUmaValue(
          GetDelegate().GetMainFrameRenderData().layout_shift_score));
}

void UkmPageLoadMetricsObserver::ReportLayoutInstabilityAfterFirstForeground() {
  DCHECK(!last_time_shown_.is_null());

  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());
  const float layout_shift_score =
      GetDelegate().GetPageRenderData().layout_shift_score;
  builder.SetExperimental_LayoutInstability_CumulativeShiftScoreAtFirstOnHidden(
      page_load_metrics::LayoutShiftUkmValue(layout_shift_score));
  // Record CLS normalization UKM.
  const std::optional<float> cwv_cls_value = GetCoreWebVitalsCLS();
  if (cwv_cls_value.has_value()) {
    builder
        .SetExperimental_LayoutInstability_MaxCumulativeShiftScoreAtFirstOnHidden_SessionWindow_Gap1000ms_Max5000ms(
            page_load_metrics::LayoutShiftUkmValue(*cwv_cls_value));
    base::UmaHistogramCustomCounts(
        "PageLoad.LayoutInstability."
        "MaxCumulativeShiftScoreAtFirstOnHidden.SessionWindow."
        "Gap1000ms.Max5000ms",
        page_load_metrics::LayoutShiftUmaValue10000(*cwv_cls_value), 1, 24000,
        50);
  }
  builder.Record(ukm::UkmRecorder::Get());

  base::UmaHistogramCounts100(
      "PageLoad.LayoutInstability."
      "CumulativeShiftScoreAtFirstOnHidden",
      page_load_metrics::LayoutShiftUmaValue(layout_shift_score));
}

void UkmPageLoadMetricsObserver::
    ReportLargestContentfulPaintAfterFirstForeground() {
  DCHECK(!last_time_shown_.is_null());

  const page_load_metrics::ContentfulPaintTimingInfo&
      all_frames_largest_contentful_paint =
          GetDelegate()
              .GetLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();

  if (all_frames_largest_contentful_paint.ContainsValidTime() &&
      WasStartedInForegroundOptionalEventInForeground(
          all_frames_largest_contentful_paint.Time(), GetDelegate())) {
    ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());
    builder.SetPaintTiming_NavigationToLargestContentfulPaint2AtFirstOnHidden(
        all_frames_largest_contentful_paint.Time().value().InMilliseconds());

    PAGE_LOAD_HISTOGRAM(
        "PageLoad.PaintTiming."
        "NavigationToLargestContentfulPaint2AtFirstOnHidden",
        all_frames_largest_contentful_paint.Time().value());
    builder.Record(ukm::UkmRecorder::Get());
  }
}

void UkmPageLoadMetricsObserver::ReportResponsivenessAfterFirstForeground() {
  DCHECK(!last_time_shown_.is_null());

  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());
  const page_load_metrics::ResponsivenessMetricsNormalization&
      responsiveness_metrics_normalization =
          GetDelegate().GetResponsivenessMetricsNormalization();

  std::optional<page_load_metrics::mojom::UserInteractionLatency> inp =
      responsiveness_metrics_normalization.ApproximateHighPercentile();
  if (inp.has_value()) {
    builder
        .SetInteractiveTiming_UserInteractionLatencyAtFirstOnHidden_HighPercentile2_MaxEventDuration(
            inp->interaction_latency.InMilliseconds());

    builder.SetInteractiveTiming_INPOffset(inp->interaction_offset);
    base::TimeDelta interaction_time =
        inp->interaction_time - GetDelegate().GetNavigationStart();
    builder.SetInteractiveTiming_INPTime(interaction_time.InMilliseconds());

    UmaHistogramCustomTimes(
        "PageLoad.InteractiveTiming.UserInteractionLatencyAtFirstOnHidden."
        "HighPercentile2."
        "MaxEventDuration",
        inp->interaction_latency, base::Milliseconds(1), base::Seconds(60), 50);
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::RecordAbortMetrics(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    base::TimeTicks page_end_time,
    ukm::builders::PageLoad* builder) {
  PageVisitFinalStatus page_visit_status =
      page_load_metrics::RecordPageVisitFinalStatusForTiming(
          timing, GetDelegate(), GetDelegate().GetPageUkmSourceId());
  if (currently_in_foreground_ && !last_time_shown_.is_null()) {
    total_foreground_duration_ += page_end_time - last_time_shown_;
  }
  UMA_HISTOGRAM_ENUMERATION("PageLoad.Experimental.PageVisitFinalStatus",
                            page_visit_status);
  PAGE_LOAD_LONG_HISTOGRAM("PageLoad.Experimental.TotalForegroundDuration",
                           total_foreground_duration_);

  builder->SetPageVisitFinalStatus(static_cast<int>(page_visit_status))
      .SetPageTiming_TotalForegroundDuration(
          ukm::GetSemanticBucketMinForDurationTiming(
              total_foreground_duration_.InMilliseconds()));
}

void UkmPageLoadMetricsObserver::RecordMemoriesMetrics(
    ukm::builders::PageLoad& builder,
    const page_load_metrics::PageEndReason page_end_reason) {
  content::WebContents* web_contents = GetDelegate().GetWebContents();
  DCHECK(web_contents);
  HistoryClustersTabHelper* clusters_helper =
      HistoryClustersTabHelper::FromWebContents(web_contents);
  if (!clusters_helper)
    return;
  const history::VisitContextAnnotations context_annotations =
      clusters_helper->OnUkmNavigationComplete(
          navigation_id_, total_foreground_duration_, page_end_reason);
  // Send ALL Memories signals to UKM at page end. This is to harmonize with
  // the fact that they may only be recorded into History at page end, when
  // we can be sure that the visit row already exists.
  //
  // Please note: We don't record everything in |context_annotations| into UKM,
  // because some of these signals are already recorded elsewhere.
  builder.SetOmniboxUrlCopied(context_annotations.omnibox_url_copied);
  builder.SetIsExistingPartOfTabGroup(
      context_annotations.is_existing_part_of_tab_group);
  builder.SetIsPlacedInTabGroup(context_annotations.is_placed_in_tab_group);
  builder.SetIsExistingBookmark(context_annotations.is_existing_bookmark);
  builder.SetIsNewBookmark(context_annotations.is_new_bookmark);
  builder.SetIsNTPCustomLink(context_annotations.is_ntp_custom_link);
  builder.SetDurationSinceLastVisitSeconds(
      context_annotations.duration_since_last_visit.InSeconds());
}

void UkmPageLoadMetricsObserver::RecordPageLoadTimestampMetrics(
    ukm::builders::PageLoad& builder) {
  DCHECK(!navigation_start_time_.is_null());

  base::Time::Exploded exploded;
  navigation_start_time_.LocalExplode(&exploded);
  builder.SetDayOfWeek(exploded.day_of_week);
  builder.SetHourOfDay(exploded.hour);
}

void UkmPageLoadMetricsObserver::RecordSmoothnessMetrics() {
  auto* smoothness =
      ukm_smoothness_data_.GetMemoryAs<cc::UkmSmoothnessDataShared>();
  if (!smoothness) {
    return;
  }

  cc::UkmSmoothnessData smoothness_data;
  bool success = smoothness->Read(smoothness_data);

  if (!success)
    return;

  ukm::builders::Graphics_Smoothness_NormalizedPercentDroppedFrames builder(
      GetDelegate().GetPageUkmSourceId());
  builder.SetAverage(smoothness_data.avg_smoothness)
      .SetMedian(smoothness_data.median_smoothness)
      .SetPercentile95(smoothness_data.percentile_95)
      .SetAboveThreshold(smoothness_data.above_threshold)
      .SetWorstCase(smoothness_data.worst_smoothness)
      .SetVariance(smoothness_data.variance)
      .SetSmoothnessVeryGood(smoothness_data.buckets[0])
      .SetSmoothnessGood(smoothness_data.buckets[1])
      .SetSmoothnessOkay(smoothness_data.buckets[2])
      .SetSmoothnessBad(smoothness_data.buckets[3])
      .SetSmoothnessVeryBad25to50(smoothness_data.buckets[4])
      .SetSmoothnessVeryBad50to75(smoothness_data.buckets[5])
      .SetSmoothnessVeryBad75to100(smoothness_data.buckets[6])
      .SetMainFocusedMedian(smoothness_data.main_focused_median)
      .SetMainFocusedPercentile95(smoothness_data.main_focused_percentile_95)
      .SetMainFocusedVariance(smoothness_data.main_focused_variance)
      .SetCompositorFocusedMedian(smoothness_data.compositor_focused_median)
      .SetCompositorFocusedPercentile95(
          smoothness_data.compositor_focused_percentile_95)
      .SetCompositorFocusedVariance(smoothness_data.compositor_focused_variance)
      .SetScrollFocusedMedian(smoothness_data.scroll_focused_median)
      .SetScrollFocusedPercentile95(
          smoothness_data.scroll_focused_percentile_95)
      .SetScrollFocusedVariance(smoothness_data.scroll_focused_variance);
  if (smoothness_data.worst_smoothness_after1sec >= 0)
    builder.SetWorstCaseAfter1Sec(smoothness_data.worst_smoothness_after1sec);
  if (smoothness_data.worst_smoothness_after2sec >= 0)
    builder.SetWorstCaseAfter2Sec(smoothness_data.worst_smoothness_after2sec);
  if (smoothness_data.worst_smoothness_after5sec >= 0)
    builder.SetWorstCaseAfter5Sec(smoothness_data.worst_smoothness_after5sec);
  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::RecordPageEndMetrics(
    const page_load_metrics::mojom::PageLoadTiming* timing,
    base::TimeTicks page_end_time,
    bool app_entered_background) {
  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());
  // page_transition_ fits in a uint32_t, so we can safely cast to int64_t.
  builder.SetNavigation_PageTransition(static_cast<int64_t>(page_transition_));

  builder.SetNavigation_InitiatorLocation(
      static_cast<int64_t>(navigation_trigger_type_));

  // GetDelegate().GetPageEndReason() fits in a uint32_t, so we can safely cast
  // to int64_t.
  auto page_end_reason = GetDelegate().GetPageEndReason();
  if (page_end_reason == page_load_metrics::PageEndReason::END_NONE &&
      app_entered_background) {
    page_end_reason =
        page_load_metrics::PageEndReason::END_APP_ENTER_BACKGROUND;
  }
  builder.SetNavigation_PageEndReason3(page_end_reason);
  bool is_user_initiated_navigation =
      // All browser initiated page loads are user-initiated.
      GetDelegate().GetUserInitiatedInfo().browser_initiated ||
      // Renderer-initiated navigations are user-initiated if there is an
      // associated input event.
      GetDelegate().GetUserInitiatedInfo().user_input_event;
  builder.SetExperimental_Navigation_UserInitiated(
      is_user_initiated_navigation);
  if (timing)
    RecordAbortMetrics(*timing, page_end_time, &builder);

  RecordMemoriesMetrics(builder, page_end_reason);

  builder.Record(ukm::UkmRecorder::Get());

  // Also log UserInitiated in UserPerceivedPageVisit.
  ukm::builders::UserPerceivedPageVisit(GetDelegate().GetPageUkmSourceId())
      .SetUserInitiated(is_user_initiated_navigation)
      .Record(ukm::UkmRecorder::Get());
}

std::optional<int64_t>
UkmPageLoadMetricsObserver::GetRoundedSiteEngagementScore() const {
  if (!browser_context_)
    return std::nullopt;

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  site_engagement::SiteEngagementService* engagement_service =
      site_engagement::SiteEngagementService::Get(profile);

  // UKM privacy requires the engagement score be rounded to nearest
  // value of 10.
  int64_t rounded_document_engagement_score =
      static_cast<int>(std::roundf(
          engagement_service->GetScore(GetDelegate().GetUrl()) / 10.0)) *
      10;

  DCHECK(rounded_document_engagement_score >= 0 &&
         rounded_document_engagement_score <=
             engagement_service->GetMaxPoints());

  return rounded_document_engagement_score;
}

std::optional<bool>
UkmPageLoadMetricsObserver::GetThirdPartyCookieBlockingEnabled() const {
  if (!browser_context_)
    return std::nullopt;

  Profile* profile = Profile::FromBrowserContext(browser_context_);
  auto cookie_settings = CookieSettingsFactory::GetForProfile(profile);
  if (!cookie_settings->ShouldBlockThirdPartyCookies())
    return std::nullopt;

  return !cookie_settings->IsThirdPartyAccessAllowed(GetDelegate().GetUrl());
}

void UkmPageLoadMetricsObserver::RecordResponsivenessMetrics() {
  ukm::builders::PageLoad builder(GetDelegate().GetPageUkmSourceId());
  const page_load_metrics::ResponsivenessMetricsNormalization&
      responsiveness_metrics_normalization =
          GetDelegate().GetResponsivenessMetricsNormalization();
  std::optional<page_load_metrics::mojom::UserInteractionLatency> inp =
      responsiveness_metrics_normalization.ApproximateHighPercentile();
  if (inp.has_value()) {
    builder.SetInteractiveTiming_WorstUserInteractionLatency_MaxEventDuration(
        responsiveness_metrics_normalization.worst_latency()
            .value()
            .interaction_latency.InMilliseconds());
    builder
        .SetInteractiveTiming_UserInteractionLatency_HighPercentile2_MaxEventDuration(
            inp->interaction_latency.InMilliseconds());

    builder.SetInteractiveTiming_INPOffset(inp->interaction_offset);
    base::TimeDelta interaction_time =
        inp->interaction_time - GetDelegate().GetNavigationStart();
    builder.SetInteractiveTiming_INPTime(interaction_time.InMilliseconds());

    builder.SetInteractiveTiming_NumInteractions(
        ukm::GetExponentialBucketMinForCounts1000(
            responsiveness_metrics_normalization.num_user_interactions()));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::OnTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  bool loading_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("loading", &loading_enabled);
  if (!loading_enabled)
    return;

  TRACE_EVENT_INSTANT(
      "loading", "UkmPageLoadTimingUpdate", [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_ukm_page_load_timing_update();
        if (first_contentful_paint_.has_value()) {
          data->set_first_contentful_paint_ms(
              first_contentful_paint_.value().InMillisecondsF());
        }
        data->set_ukm_source_id(
            static_cast<int64_t>(GetDelegate().GetPageUkmSourceId()));
        data->set_latest_url(GetDelegate().GetUrl().spec());

        const std::optional<float> cwv_cls_value = GetCoreWebVitalsCLS();
        if (cwv_cls_value.has_value()) {
          data->set_latest_cumulative_layout_shift(*cwv_cls_value);
        }
        const page_load_metrics::ContentfulPaintTimingInfo&
            cwv_lcp_timing_info = GetCoreWebVitalsLcpTimingInfo();
        if (cwv_lcp_timing_info.ContainsValidTime() &&
            WasStartedInForegroundOptionalEventInForeground(
                cwv_lcp_timing_info.Time(), GetDelegate())) {
          data->set_latest_largest_contentful_paint_ms(
              cwv_lcp_timing_info.Time().value().InMillisecondsF());
        }
      });

  // The ones below are old trace events which should not be necessary given the
  // UkmPageLoadTimingUpdate event above, but they may be used in
  // devtools/lighthouse in addition to the old catapult loadingMetric. They can
  // be removed once we verify that the consumers of these trace events have
  // been updated.

  const page_load_metrics::ContentfulPaintTimingInfo& paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();

  if (paint.ContainsValidTime()) {
    TRACE_EVENT_INSTANT2(
        "loading",
        "NavStartToLargestContentfulPaint::Candidate::AllFrames::UKM",
        TRACE_EVENT_SCOPE_THREAD, "data", paint.DataAsTraceValue(),
        "main_frame_tree_node_id",
        GetDelegate().GetLargestContentfulPaintHandler().MainFrameTreeNodeId());
  } else {
    TRACE_EVENT_INSTANT1(
        "loading",
        "NavStartToLargestContentfulPaint::"
        "Invalidate::AllFrames::UKM",
        TRACE_EVENT_SCOPE_THREAD, "main_frame_tree_node_id",
        GetDelegate().GetLargestContentfulPaintHandler().MainFrameTreeNodeId());
  }

  const page_load_metrics::ContentfulPaintTimingInfo&
      experimental_largest_contentful_paint =
          GetDelegate()
              .GetExperimentalLargestContentfulPaintHandler()
              .MergeMainFrameAndSubframes();
  if (experimental_largest_contentful_paint.ContainsValidTime()) {
    TRACE_EVENT_INSTANT2(
        "loading",
        "NavStartToExperimentalLargestContentfulPaint::Candidate::AllFrames::"
        "UKM",
        TRACE_EVENT_SCOPE_THREAD, "data",
        experimental_largest_contentful_paint.DataAsTraceValue(),
        "main_frame_tree_node_id",
        GetDelegate()
            .GetExperimentalLargestContentfulPaintHandler()
            .MainFrameTreeNodeId());
  } else {
    TRACE_EVENT_INSTANT1("loading",
                         "NavStartToExperimentalLargestContentfulPaint::"
                         "Invalidate::AllFrames::UKM",
                         TRACE_EVENT_SCOPE_THREAD, "main_frame_tree_node_id",
                         GetDelegate()
                             .GetExperimentalLargestContentfulPaintHandler()
                             .MainFrameTreeNodeId());
  }
}

void UkmPageLoadMetricsObserver::SetUpSharedMemoryForSmoothness(
    const base::ReadOnlySharedMemoryRegion& shared_memory) {
  ukm_smoothness_data_ = shared_memory.Map();
}

void UkmPageLoadMetricsObserver::OnCpuTimingUpdate(
    content::RenderFrameHost* subframe_rfh,
    const page_load_metrics::mojom::CpuTiming& timing) {
  if (GetDelegate().GetVisibilityTracker().currently_in_foreground() &&
      !was_hidden_)
    total_foreground_cpu_time_ += timing.task_time;
}

void UkmPageLoadMetricsObserver::RecordNoStatePrefetchMetrics(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  prerender::NoStatePrefetchManager* const no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
  if (!no_state_prefetch_manager)
    return;

  const std::vector<GURL>& redirects = navigation_handle->GetRedirectChain();

  base::TimeDelta prefetch_age;
  prerender::FinalStatus final_status;
  prerender::Origin prefetch_origin;

  bool no_state_prefetch_entry_found =
      no_state_prefetch_manager->GetPrefetchInformation(
          navigation_handle->GetURL(), &prefetch_age, &final_status,
          &prefetch_origin);

  // Try the URLs from the redirect chain.
  if (!no_state_prefetch_entry_found) {
    for (const auto& url : redirects) {
      no_state_prefetch_entry_found =
          no_state_prefetch_manager->GetPrefetchInformation(
              url, &prefetch_age, &final_status, &prefetch_origin);
      if (no_state_prefetch_entry_found)
        break;
    }
  }

  if (!no_state_prefetch_entry_found)
    return;

  ukm::builders::NoStatePrefetch builder(source_id);
  builder.SetPrefetchedRecently_PrefetchAge(
      ukm::GetExponentialBucketMinForUserTiming(prefetch_age.InMilliseconds()));
  builder.SetPrefetchedRecently_FinalStatus(final_status);
  builder.SetPrefetchedRecently_Origin(prefetch_origin);
  builder.Record(ukm::UkmRecorder::Get());
}

bool UkmPageLoadMetricsObserver::IsOfflinePreview(
    content::WebContents* web_contents) const {
#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_pages::OfflinePageTabHelper* tab_helper =
      offline_pages::OfflinePageTabHelper::FromWebContents(web_contents);
  return tab_helper && tab_helper->GetOfflinePreviewItem();
#else
  return false;
#endif
}

void UkmPageLoadMetricsObserver::RecordGeneratedNavigationUKM(
    ukm::SourceId source_id,
    const GURL& committed_url) {
  bool final_url_is_home_page = IsUserHomePage(browser_context_, committed_url);
  bool final_url_is_default_search =
      IsDefaultSearchEngine(browser_context_, committed_url);

  if (!final_url_is_home_page && !final_url_is_default_search &&
      !start_url_is_home_page_ && !start_url_is_default_search_) {
    return;
  }

  ukm::builders::GeneratedNavigation builder(source_id);
  builder.SetFinalURLIsHomePage(final_url_is_home_page);
  builder.SetFinalURLIsDefaultSearchEngine(final_url_is_default_search);
  builder.SetFirstURLIsHomePage(start_url_is_home_page_);
  builder.SetFirstURLIsDefaultSearchEngine(start_url_is_default_search_);
  builder.Record(ukm::UkmRecorder::Get());
}

void UkmPageLoadMetricsObserver::EmitUserTimingEvent(base::TimeDelta duration,
                                                     const char event_name[]) {
  const base::TimeTicks navigation_start = GetDelegate().GetNavigationStart();
  const perfetto::Track track(kInstantPageLoadEventsTraceTrackId,
                              perfetto::ProcessTrack::Current());
  TRACE_EVENT_INSTANT(
      "loading,interactions", perfetto::StaticString{event_name}, track,
      navigation_start + duration, [&](perfetto::EventContext ctx) {
        auto* page_load_proto =
            ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>()
                ->set_page_load();
        page_load_proto->set_navigation_id(GetDelegate().GetNavigationId());
      });
}
