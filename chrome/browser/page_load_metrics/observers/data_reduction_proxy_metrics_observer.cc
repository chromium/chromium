// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "chrome/browser/loader/chrome_navigation_data.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_pingback_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_page_load_timing.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/proto/pageload_metrics.pb.h"
#include "components/previews/content/previews_user_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

// Appends |suffix| to |kHistogramDataReductionProxyPrefix| and returns it as a
// string.
std::string GetConstHistogramWithSuffix(const char* suffix) {
  return std::string(internal::kHistogramDataReductionProxyPrefix)
      .append(suffix);
}

PageloadMetrics_PageEndReason ConvertPLMPageEndReasonToProto(
    page_load_metrics::PageEndReason reason) {
  switch (reason) {
    case page_load_metrics::END_NONE:
      return PageloadMetrics_PageEndReason_END_NONE;
    case page_load_metrics::END_RELOAD:
      return PageloadMetrics_PageEndReason_END_RELOAD;
    case page_load_metrics::END_FORWARD_BACK:
      return PageloadMetrics_PageEndReason_END_FORWARD_BACK;
    case page_load_metrics::END_CLIENT_REDIRECT:
      return PageloadMetrics_PageEndReason_END_CLIENT_REDIRECT;
    case page_load_metrics::END_NEW_NAVIGATION:
      return PageloadMetrics_PageEndReason_END_NEW_NAVIGATION;
    case page_load_metrics::END_STOP:
      return PageloadMetrics_PageEndReason_END_STOP;
    case page_load_metrics::END_CLOSE:
      return PageloadMetrics_PageEndReason_END_CLOSE;
    case page_load_metrics::END_PROVISIONAL_LOAD_FAILED:
      return PageloadMetrics_PageEndReason_END_PROVISIONAL_LOAD_FAILED;
    case page_load_metrics::END_RENDER_PROCESS_GONE:
      return PageloadMetrics_PageEndReason_END_RENDER_PROCESS_GONE;
    default:
      return PageloadMetrics_PageEndReason_END_OTHER;
  }
}

// A macro is needed because PAGE_LOAD_HISTOGRAM creates a static instance of
// the histogram. A distinct histogram is needed for each place that calls
// RECORD_HISTOGRAMS_FOR_SUFFIX. |event| is the timing event representing when
// |value| became available.
#define RECORD_HISTOGRAMS_FOR_SUFFIX(data, value, histogram_suffix)            \
  do {                                                                         \
    PAGE_LOAD_HISTOGRAM(GetConstHistogramWithSuffix(histogram_suffix), value); \
    if (data->lite_page_received()) {                                          \
      PAGE_LOAD_HISTOGRAM(                                                     \
          std::string(internal::kHistogramDataReductionProxyLitePagePrefix)    \
              .append(histogram_suffix),                                       \
          value);                                                              \
    }                                                                          \
  } while (false)

// Like RECORD_HISTOGRAMS_FOR_SUFFIX, but only records histograms if the event
// occurred while the page was in the foreground.
#define RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(info, data, timing,         \
                                                histogram_suffix)           \
  do {                                                                      \
    if (WasStartedInForegroundOptionalEventInForeground(timing, info)) {    \
      RECORD_HISTOGRAMS_FOR_SUFFIX(data, timing.value(), histogram_suffix); \
    }                                                                       \
  } while (false)

}  // namespace

namespace internal {

const char kHistogramDataReductionProxyPrefix[] =
    "PageLoad.Clients.DataReductionProxy.";
const char kHistogramDataReductionProxyLitePagePrefix[] =
    "PageLoad.Clients.Previews.LitePages.";

const char kResourcesPercentProxied[] =
    "Experimental.CompletedResources.Network.PercentProxied";
const char kBytesPercentProxied[] = "Experimental.Bytes.Network.PercentProxied";
const char kBytesCompressionRatio[] =
    "Experimental.Bytes.Network.CompressionRatio";
const char kBytesInflationPercent[] =
    "Experimental.Bytes.Network.InflationPercent";
const char kNetworkResources[] = "Experimental.CompletedResources.Network";
const char kResourcesProxied[] =
    "Experimental.CompletedResources.Network.Proxied";
const char kResourcesNotProxied[] =
    "Experimental.CompletedResources.Network.NonProxied";
const char kNetworkBytes[] = "Experimental.Bytes.Network";
const char kBytesProxied[] = "Experimental.Bytes.Network.Proxied";
const char kBytesNotProxied[] = "Experimental.Bytes.Network.NonProxied";
const char kBytesOriginal[] = "Experimental.Bytes.Network.Original";
const char kBytesSavings[] = "Experimental.Bytes.Network.Savings";
const char kBytesInflation[] = "Experimental.Bytes.Network.Inflation";

}  // namespace internal

DataReductionProxyMetricsObserver::DataReductionProxyMetricsObserver()
    : browser_context_(nullptr),
      opted_out_(false),
      num_data_reduction_proxy_resources_(0),
      num_network_resources_(0),
      insecure_original_network_bytes_(0),
      secure_original_network_bytes_(0),
      network_bytes_proxied_(0),
      insecure_network_bytes_(0),
      secure_network_bytes_(0),
      insecure_cached_bytes_(0),
      secure_cached_bytes_(0),
      process_id_(base::kNullProcessId),
      renderer_memory_usage_kb_(0),
      render_process_host_id_(content::ChildProcessHost::kInvalidUniqueID),
      touch_count_(0),
      scroll_count_(0),
      weak_ptr_factory_(this) {}

DataReductionProxyMetricsObserver::~DataReductionProxyMetricsObserver() {}

// Check if the NavigationData indicates anything about the DataReductionProxy.
page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This BrowserContext is valid for the lifetime of
  // DataReductionProxyMetricsObserver. BrowserContext is always valid and
  // non-nullptr in NavigationControllerImpl, which is a member of WebContents.
  // A raw pointer to BrowserContext taken at this point will be valid until
  // after WebContent's destructor. The latest that PageLoadTracker's destructor
  // will be called is in MetricsWebContentsObserver's destrcutor, which is
  // called in WebContents destructor.
  browser_context_ = navigation_handle->GetWebContents()->GetBrowserContext();
  // As documented in content/public/browser/navigation_handle.h, this
  // NavigationData is a clone of the NavigationData instance returned from
  // ResourceDispatcherHostDelegate::GetNavigationData during commit.
  // Because ChromeResourceDispatcherHostDelegate always returns a
  // ChromeNavigationData, it is safe to static_cast here.
  std::unique_ptr<DataReductionProxyData> data;
  auto* settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context_);
  if (settings) {
    data = settings->CreateDataFromNavigationHandle(
        navigation_handle, navigation_handle->GetResponseHeaders());
  }
  if (!data || !(data->used_data_reduction_proxy() ||
                 data->was_cached_data_reduction_proxy_response())) {
    return STOP_OBSERVING;
  }
  data_ = std::move(data);

  PreviewsUITabHelper* ui_tab_helper =
      PreviewsUITabHelper::FromWebContents(navigation_handle->GetWebContents());
  previews::PreviewsUserData* previews_data = nullptr;

  if (ui_tab_helper)
    previews_data = ui_tab_helper->GetPreviewsUserData(navigation_handle);

  if (previews_data) {
    data_->set_black_listed(previews_data->black_listed_for_lite_page());
  }

  process_id_ = navigation_handle->GetWebContents()
                    ->GetMainFrame()
                    ->GetProcess()
                    ->GetProcess()
                    .Pid();
  render_process_host_id_ = navigation_handle->GetWebContents()
                                ->GetMainFrame()
                                ->GetProcess()
                                ->GetID();

  // DataReductionProxy page loads should only occur on HTTP navigations.
  DCHECK(!navigation_handle->GetURL().SchemeIsCryptographic());
  DCHECK_EQ(data_->request_url(), navigation_handle->GetURL());
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserver::OnStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url,
    bool started_in_foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!started_in_foreground)
    return STOP_OBSERVING;
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we send a pingback with data collected up to this point.
  if (info.did_commit) {
    RecordPageSizeUMA();
    SendPingback(timing, info, true /* app_background_occurred */);
  }
  return STOP_OBSERVING;
}

void DataReductionProxyMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordPageSizeUMA();
  SendPingback(timing, info, false /* app_background_occurred */);
}

void DataReductionProxyMetricsObserver::RecordPageSizeUMA() const {
  if (!data_)
    return;

  // If the first request didn't complete, don't record UMA.
  if (num_network_resources_ == 0)
    return;

  const int64_t network_bytes = insecure_network_bytes_ + secure_network_bytes_;
  const int64_t original_network_bytes =
      insecure_original_network_bytes_ + secure_original_network_bytes_;

  // TODO(ryansturm): Evaluate if any of the below histograms are unncessary
  // once data is available. crbug.com/682782

  // The percent of resources that went through the data reduction proxy.
  UMA_HISTOGRAM_PERCENTAGE(
      GetConstHistogramWithSuffix(internal::kResourcesPercentProxied),
      (100 * num_data_reduction_proxy_resources_) / num_network_resources_);

  // The percent of bytes that went through the data reduction proxy.
  if (network_bytes > 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        GetConstHistogramWithSuffix(internal::kBytesPercentProxied),
        static_cast<int>((100 * network_bytes_proxied_) / network_bytes));
  }

  // If the data reduction proxy caused savings, record the compression ratio;
  // otherwise, record the inflation ratio.
  if (original_network_bytes > 0 && original_network_bytes >= network_bytes) {
    UMA_HISTOGRAM_PERCENTAGE(
        GetConstHistogramWithSuffix(internal::kBytesCompressionRatio),
        static_cast<int>((100 * network_bytes) / original_network_bytes));
  } else if (original_network_bytes > 0) {
    // Inflation should never be above one hundred percent.
    UMA_HISTOGRAM_PERCENTAGE(
        GetConstHistogramWithSuffix(internal::kBytesInflationPercent),
        static_cast<int>((100 * network_bytes) / original_network_bytes - 100));
  }

  // Record the number of network resources seen.
  PAGE_RESOURCE_COUNT_HISTOGRAM(
      GetConstHistogramWithSuffix(internal::kNetworkResources),
      num_network_resources_);

  // Record the number of resources that used data reduction proxy.
  PAGE_RESOURCE_COUNT_HISTOGRAM(
      GetConstHistogramWithSuffix(internal::kResourcesProxied),
      num_data_reduction_proxy_resources_);

  // Record the number of resources that did not use data reduction proxy.
  PAGE_RESOURCE_COUNT_HISTOGRAM(
      GetConstHistogramWithSuffix(internal::kResourcesNotProxied),
      num_network_resources_ - num_data_reduction_proxy_resources_);

  // Record the total KB of network bytes.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kNetworkBytes),
                       network_bytes);

  // Record the total amount of bytes that went through the data reduction
  // proxy.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesProxied),
                       network_bytes_proxied_);

  // Record the total amount of bytes that did not go through the data reduction
  // proxy.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesNotProxied),
                       network_bytes - network_bytes_proxied_);

  // Record the total KB of network bytes that the user would have seen without
  // using data reduction proxy.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesOriginal),
                       original_network_bytes);

  // Record the savings the user saw by using data reduction proxy. If there was
  // inflation instead, record that.
  if (network_bytes <= original_network_bytes) {
    PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesSavings),
                         original_network_bytes - network_bytes);
  } else {
    PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesInflation),
                         network_bytes_proxied_ - original_network_bytes);
  }
}

// static
int64_t DataReductionProxyMetricsObserver::ExponentiallyBucketBytes(
    int64_t bytes) {
  const int64_t start_buckets = 10000;
  if (bytes < start_buckets) {
    return 0;
  }
  return ukm::GetExponentialBucketMin(bytes, 1.16);
}

void DataReductionProxyMetricsObserver::SendPingback(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info,
    bool app_background_occurred) {
  // TODO(ryansturm): Move to OnFirstBackgroundEvent to handle some fast
  // shutdown cases. crbug.com/618072
  if (!browser_context_ || !data_)
    return;
  // Only consider timing events that happened before the first background
  // event.
  base::Optional<base::TimeDelta> response_start;
  base::Optional<base::TimeDelta> load_event_start;
  base::Optional<base::TimeDelta> first_image_paint;
  base::Optional<base::TimeDelta> first_contentful_paint;
  base::Optional<base::TimeDelta> experimental_first_meaningful_paint;
  base::Optional<base::TimeDelta> first_input_delay;
  base::Optional<base::TimeDelta> parse_blocked_on_script_load_duration;
  base::Optional<base::TimeDelta> parse_stop;
  base::Optional<base::TimeDelta> page_end_time;
  if (WasStartedInForegroundOptionalEventInForeground(timing.response_start,
                                                      info)) {
    response_start = timing.response_start;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.document_timing->load_event_start, info)) {
    load_event_start = timing.document_timing->load_event_start;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_image_paint, info)) {
    first_image_paint = timing.paint_timing->first_image_paint;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, info)) {
    first_contentful_paint = timing.paint_timing->first_contentful_paint;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_meaningful_paint, info)) {
    experimental_first_meaningful_paint =
        timing.paint_timing->first_meaningful_paint;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_delay, info)) {
    first_input_delay = timing.interactive_timing->first_input_delay;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_blocked_on_script_load_duration, info)) {
    parse_blocked_on_script_load_duration =
        timing.parse_timing->parse_blocked_on_script_load_duration;
  }
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, info)) {
    parse_stop = timing.parse_timing->parse_stop;
  }
  if (info.started_in_foreground && info.page_end_time.has_value()) {
    // This should be reported even when the app goes into the background which
    // is excluded in |WasStartedInForegroundOptionalEventInForeground|.
    page_end_time = info.page_end_time;
  } else if (info.started_in_foreground) {
    page_end_time = base::TimeTicks::Now() - info.navigation_start;
  }

  // If a crash happens, report the host |render_process_host_id_| to the
  // pingback client. Otherwise report kInvalidUniqueID.
  int host_id = content::ChildProcessHost::kInvalidUniqueID;
  if (info.page_end_reason ==
      page_load_metrics::PageEndReason::END_RENDER_PROCESS_GONE) {
    host_id = render_process_host_id_;
  }

  const int64_t original_network_bytes =
      insecure_original_network_bytes_ +
      ExponentiallyBucketBytes(secure_original_network_bytes_);
  const int64_t network_bytes =
      insecure_network_bytes_ + ExponentiallyBucketBytes(secure_network_bytes_);
  const int64_t total_page_size_bytes =
      insecure_network_bytes_ + insecure_cached_bytes_ +
      ExponentiallyBucketBytes(secure_network_bytes_ + secure_cached_bytes_);

  // Recording cached bytes can be done with raw data, but the end result must
  // be bucketed in 50 linear buckets between 0% - 100%.
  const int64_t cached_bytes = insecure_cached_bytes_ + secure_cached_bytes_;
  const int64_t total_bytes =
      cached_bytes + insecure_network_bytes_ + secure_network_bytes_;
  int cached_percentage;
  if (total_bytes <= 0) {
    cached_percentage = 0;
  } else {
    cached_percentage =
        static_cast<int>(std::lround(static_cast<float>(cached_bytes) /
                                     static_cast<float>(total_bytes) * 100.0));
  }
  DCHECK_GE(cached_percentage, 0);
  DCHECK_LE(cached_percentage, 100);
  cached_percentage = cached_percentage - (cached_percentage % 2);
  const float cached_fraction = static_cast<float>(cached_percentage) / 100.0;

  DataReductionProxyPageLoadTiming data_reduction_proxy_timing(
      timing.navigation_start, response_start, load_event_start,
      first_image_paint, first_contentful_paint,
      experimental_first_meaningful_paint, first_input_delay,
      parse_blocked_on_script_load_duration, parse_stop, page_end_time,
      network_bytes, original_network_bytes, total_page_size_bytes,
      cached_fraction, app_background_occurred, opted_out_,
      renderer_memory_usage_kb_, host_id,
      ConvertPLMPageEndReasonToProto(info.page_end_reason), touch_count_,
      scroll_count_);
  GetPingbackClient()->SendPingback(*data_, data_reduction_proxy_timing);
}

void DataReductionProxyMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.document_timing->dom_content_loaded_event_start,
      ::internal::kHistogramDOMContentLoadedEventFiredSuffix);
}

void DataReductionProxyMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.document_timing->load_event_start,
      ::internal::kHistogramLoadEventFiredSuffix);
  if (process_id_ != base::kNullProcessId) {
    auto callback = base::BindRepeating(
        &DataReductionProxyMetricsObserver::ProcessMemoryDump,
        weak_ptr_factory_.GetWeakPtr());
    RequestProcessDump(process_id_, callback);
  }
}

void DataReductionProxyMetricsObserver::OnFirstLayout(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.document_timing->first_layout,
      ::internal::kHistogramFirstLayoutSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.paint_timing->first_paint,
      ::internal::kHistogramFirstPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstTextPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.paint_timing->first_text_paint,
      ::internal::kHistogramFirstTextPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstImagePaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.paint_timing->first_image_paint,
      ::internal::kHistogramFirstImagePaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.paint_timing->first_contentful_paint,
      ::internal::kHistogramFirstContentfulPaintSuffix);
}

void DataReductionProxyMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing,
        const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.paint_timing->first_meaningful_paint,
      ::internal::kHistogramFirstMeaningfulPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      info, data_, timing.parse_timing->parse_start,
      ::internal::kHistogramParseStartSuffix);
}

void DataReductionProxyMetricsObserver::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, info))
    return;

  base::TimeDelta parse_duration = timing.parse_timing->parse_stop.value() -
                                   timing.parse_timing->parse_start.value();
  RECORD_HISTOGRAMS_FOR_SUFFIX(data_, parse_duration,
                               ::internal::kHistogramParseDurationSuffix);
  RECORD_HISTOGRAMS_FOR_SUFFIX(
      data_, timing.parse_timing->parse_blocked_on_script_load_duration.value(),
      ::internal::kHistogramParseBlockedOnScriptLoadSuffix);
}

void DataReductionProxyMetricsObserver::OnLoadedResource(
    const page_load_metrics::ExtraRequestCompleteInfo&
        extra_request_complete_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (extra_request_complete_info.data_reduction_proxy_data &&
      extra_request_complete_info.data_reduction_proxy_data->lofi_received()) {
    data_->set_lofi_received(true);
  }

  const bool is_secure =
      extra_request_complete_info.url.SchemeIsCryptographic();

  if (extra_request_complete_info.was_cached) {
    if (is_secure) {
      secure_cached_bytes_ += extra_request_complete_info.raw_body_bytes;
    } else {
      insecure_cached_bytes_ += extra_request_complete_info.raw_body_bytes;
    }
    return;
  }

  if (is_secure) {
    secure_original_network_bytes_ +=
        extra_request_complete_info.original_network_content_length;
    secure_network_bytes_ += extra_request_complete_info.raw_body_bytes;
  } else {
    insecure_original_network_bytes_ +=
        extra_request_complete_info.original_network_content_length;
    insecure_network_bytes_ += extra_request_complete_info.raw_body_bytes;
  }
  num_network_resources_++;
  if (!extra_request_complete_info.data_reduction_proxy_data ||
      !extra_request_complete_info.data_reduction_proxy_data
           ->used_data_reduction_proxy()) {
    return;
  }
  num_data_reduction_proxy_resources_++;
  // Proxied bytes are always non-secure.
  network_bytes_proxied_ += extra_request_complete_info.raw_body_bytes;
}

DataReductionProxyPingbackClient*
DataReductionProxyMetricsObserver::GetPingbackClient() const {
  return DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
             browser_context_)
      ->data_reduction_proxy_service()
      ->pingback_client();
}

void DataReductionProxyMetricsObserver::OnEventOccurred(
    const void* const event_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (event_key == PreviewsUITabHelper::OptOutEventKey())
    opted_out_ = true;
}

void DataReductionProxyMetricsObserver::OnUserInput(
    const blink::WebInputEvent& event) {
  if (event.GetType() == blink::WebInputEvent::kMouseDown ||
      event.GetType() == blink::WebInputEvent::kGestureTap) {
    touch_count_++;
  }

  if (event.GetType() == blink::WebInputEvent::kMouseWheel ||
      event.GetType() == blink::WebInputEvent::kGestureScrollUpdate ||
      event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
    scroll_count_++;
  }
}

void DataReductionProxyMetricsObserver::ProcessMemoryDump(
    bool success,
    std::unique_ptr<memory_instrumentation::GlobalMemoryDump> memory_dump) {
  if (!success || !memory_dump)
    return;
  // There should only be one process in the dump.
  DCHECK_EQ(1, std::distance(memory_dump->process_dumps().begin(),
                             memory_dump->process_dumps().end()));

  auto process_dump_it = memory_dump->process_dumps().begin();
  if (process_dump_it == memory_dump->process_dumps().end())
    return;

  // We want to catch this in debug but not crash in release.
  DCHECK_EQ(process_id_, process_dump_it->pid());
  if (process_dump_it->pid() != process_id_)
    return;
  renderer_memory_usage_kb_ =
      static_cast<int64_t>(process_dump_it->os_dump().private_footprint_kb);
}

void DataReductionProxyMetricsObserver::RequestProcessDump(
    base::ProcessId pid,
    memory_instrumentation::MemoryInstrumentation::RequestGlobalDumpCallback
        callback) {
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(pid, std::move(callback));
}

}  // namespace data_reduction_proxy
