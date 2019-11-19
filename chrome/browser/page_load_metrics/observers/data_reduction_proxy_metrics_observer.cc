// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

namespace {

// Appends |suffix| to |kHistogramDataReductionProxyPrefix| and returns it as a
// string.
std::string GetConstHistogramWithSuffix(const char* suffix) {
  return std::string(internal::kHistogramDataReductionProxyPrefix)
      .append(suffix);
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
#define RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(delegate, data, timing,     \
                                                histogram_suffix)           \
  do {                                                                      \
    if (page_load_metrics::WasStartedInForegroundOptionalEventInForeground( \
            timing, delegate)) {                                            \
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
    "Experimental.CompletedResources.Network2.PercentProxied";
const char kBytesPercentProxied[] =
    "Experimental.Bytes.Network.PercentProxied2";
const char kBytesCompressionRatio[] =
    "Experimental.Bytes.Network.CompressionRatio2";
const char kBytesInflationPercent[] =
    "Experimental.Bytes.Network.InflationPercent2";
const char kNetworkResources[] = "Experimental.CompletedResources.Network2";
const char kResourcesProxied[] =
    "Experimental.CompletedResources.Network2.Proxied";
const char kResourcesNotProxied[] =
    "Experimental.CompletedResources.Network2.NonProxied";
const char kNetworkBytes[] = "Experimental.Bytes.Network2";
const char kBytesProxied[] = "Experimental.Bytes.Network.Proxied2";
const char kBytesNotProxied[] = "Experimental.Bytes.Network.NonProxied2";
const char kBytesOriginal[] = "Experimental.Bytes.Network.Original2";
const char kBytesSavings[] = "Experimental.Bytes.Network.Savings2";
const char kBytesInflation[] = "Experimental.Bytes.Network.Inflation2";

}  // namespace internal

DataReductionProxyMetricsObserver::DataReductionProxyMetricsObserver()
    : DataReductionProxyMetricsObserverBase() {}

DataReductionProxyMetricsObserver::~DataReductionProxyMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
DataReductionProxyMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // FlushMetricsOnAppEnterBackground is invoked on Android in cases where the
  // app is about to be backgrounded, as part of the Activity.onPause()
  // flow. After this method is invoked, Chrome may be killed without further
  // notification, so we send a pingback with data collected up to this point.
  if (GetDelegate().DidCommit()) {
    RecordPageSizeUMA();
  }
  return DataReductionProxyMetricsObserverBase::
      FlushMetricsOnAppEnterBackground(timing);
}

void DataReductionProxyMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DataReductionProxyMetricsObserverBase::OnComplete(timing);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordPageSizeUMA();
}

void DataReductionProxyMetricsObserver::RecordPageSizeUMA() const {
  if (!data())
    return;

  // If the first request didn't complete, don't record UMA.
  if (num_network_resources() == 0)
    return;

  const int64_t network_bytes =
      insecure_network_bytes() + secure_network_bytes();
  const int64_t original_network_bytes =
      insecure_original_network_bytes() + secure_original_network_bytes();

  // TODO(ryansturm): Evaluate if any of the below histograms are unnecessary
  // once data is available. crbug.com/682782

  // The percent of resources that went through the data reduction proxy.
  UMA_HISTOGRAM_PERCENTAGE(
      GetConstHistogramWithSuffix(internal::kResourcesPercentProxied),
      (100 * num_data_reduction_proxy_resources()) / num_network_resources());

  // The percent of bytes that went through the data reduction proxy.
  if (network_bytes > 0) {
    UMA_HISTOGRAM_PERCENTAGE(
        GetConstHistogramWithSuffix(internal::kBytesPercentProxied),
        static_cast<int>((100 * network_bytes_proxied()) / network_bytes));
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
      num_network_resources());

  // Record the number of resources that used data reduction proxy.
  PAGE_RESOURCE_COUNT_HISTOGRAM(
      GetConstHistogramWithSuffix(internal::kResourcesProxied),
      num_data_reduction_proxy_resources());

  // Record the number of resources that did not use data reduction proxy.
  PAGE_RESOURCE_COUNT_HISTOGRAM(
      GetConstHistogramWithSuffix(internal::kResourcesNotProxied),
      num_network_resources() - num_data_reduction_proxy_resources());

  // Record the total KB of network bytes.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kNetworkBytes),
                       network_bytes);

  // Record the total amount of bytes that went through the data reduction
  // proxy.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesProxied),
                       network_bytes_proxied());

  // Record the total amount of bytes that did not go through the data reduction
  // proxy.
  PAGE_BYTES_HISTOGRAM(GetConstHistogramWithSuffix(internal::kBytesNotProxied),
                       network_bytes - network_bytes_proxied());

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
                         network_bytes_proxied() - original_network_bytes);
  }
}

void DataReductionProxyMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      GetDelegate(), data(),
      timing.document_timing->dom_content_loaded_event_start,
      ::internal::kHistogramDOMContentLoadedEventFiredSuffix);
}

void DataReductionProxyMetricsObserver::OnLoadEventStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DataReductionProxyMetricsObserverBase::OnLoadEventStart(timing);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      GetDelegate(), data(), timing.document_timing->load_event_start,
      ::internal::kHistogramLoadEventFiredSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstLayout(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      GetDelegate(), data(), timing.document_timing->first_layout,
      ::internal::kHistogramFirstLayoutSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      GetDelegate(), data(), timing.paint_timing->first_paint,
      ::internal::kHistogramFirstPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstImagePaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      GetDelegate(), data(), timing.paint_timing->first_image_paint,
      ::internal::kHistogramFirstImagePaintSuffix);
}

void DataReductionProxyMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      GetDelegate(), data(), timing.paint_timing->first_contentful_paint,
      ::internal::kHistogramFirstContentfulPaintSuffix);
}

void DataReductionProxyMetricsObserver::
    OnFirstMeaningfulPaintInMainFrameDocument(
        const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      GetDelegate(), data(), timing.paint_timing->first_meaningful_paint,
      ::internal::kHistogramFirstMeaningfulPaintSuffix);
}

void DataReductionProxyMetricsObserver::OnParseStart(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RECORD_FOREGROUND_HISTOGRAMS_FOR_SUFFIX(
      GetDelegate(), data(), timing.parse_timing->parse_start,
      ::internal::kHistogramParseStartSuffix);
}

void DataReductionProxyMetricsObserver::OnParseStop(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.parse_timing->parse_stop, GetDelegate()))
    return;

  base::TimeDelta parse_duration = timing.parse_timing->parse_stop.value() -
                                   timing.parse_timing->parse_start.value();
  RECORD_HISTOGRAMS_FOR_SUFFIX(data(), parse_duration,
                               ::internal::kHistogramParseDurationSuffix);
  RECORD_HISTOGRAMS_FOR_SUFFIX(
      data(),
      timing.parse_timing->parse_blocked_on_script_load_duration.value(),
      ::internal::kHistogramParseBlockedOnScriptLoadSuffix);
}

}  // namespace data_reduction_proxy
