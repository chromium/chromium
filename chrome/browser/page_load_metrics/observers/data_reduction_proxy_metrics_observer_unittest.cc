// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer.h"

#include <stdint.h>
#include <memory>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/page_load_metrics/observers/data_reduction_proxy_metrics_observer_test_utils.h"
#include "chrome/browser/page_load_metrics/observers/histogram_suffixes.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/previews/content/previews_user_data.h"
#include "content/public/browser/web_contents.h"

namespace data_reduction_proxy {

// DataReductionProxyMetricsObserver responsible for modifying data about the
// navigation in OnCommit.
class TestDataReductionProxyMetricsObserver
    : public DataReductionProxyMetricsObserver {
 public:
  TestDataReductionProxyMetricsObserver(content::WebContents* web_contents,
                                        bool data_reduction_proxy_used,
                                        bool cached_data_reduction_proxy_used,
                                        bool lite_page_used,
                                        bool black_listed)
      : web_contents_(web_contents),
        data_reduction_proxy_used_(data_reduction_proxy_used),
        cached_data_reduction_proxy_used_(cached_data_reduction_proxy_used),
        lite_page_used_(lite_page_used),
        black_listed_(black_listed) {}

  ~TestDataReductionProxyMetricsObserver() override {}

  // DataReductionProxyMetricsObserver:
  ObservePolicy OnCommitCalled(content::NavigationHandle* navigation_handle,
                               ukm::SourceId source_id) override {
    auto data =
        std::make_unique<data_reduction_proxy::DataReductionProxyData>();
    data->set_request_url(navigation_handle->GetURL());
    data->set_used_data_reduction_proxy(data_reduction_proxy_used_);
    data->set_was_cached_data_reduction_proxy_response(
        cached_data_reduction_proxy_used_);
    data->set_request_url(GURL(kDefaultTestUrl));
    data->set_lite_page_received(lite_page_used_);
    DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
        web_contents_->GetBrowserContext())
        ->SetDataForNextCommitForTesting(std::move(data));

    auto* previews_data = PreviewsDataForNavigationHandle(navigation_handle);
    previews_data->set_black_listed_for_lite_page(black_listed_);

    return DataReductionProxyMetricsObserver::OnCommitCalled(navigation_handle,
                                                             source_id);
  }

 private:
  content::WebContents* web_contents_;
  bool data_reduction_proxy_used_;
  bool cached_data_reduction_proxy_used_;
  bool lite_page_used_;
  bool black_listed_;

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyMetricsObserver);
};

class DataReductionProxyMetricsObserverTest
    : public DataReductionProxyMetricsObserverTestBase {
 public:
  DataReductionProxyMetricsObserverTest() {}
  ~DataReductionProxyMetricsObserverTest() override {}

  void ValidateHistograms() {
    ValidateHistogramsForSuffix(
        ::internal::kHistogramDOMContentLoadedEventFiredSuffix,
        timing_.document_timing->dom_content_loaded_event_start);
    ValidateHistogramsForSuffix(::internal::kHistogramFirstLayoutSuffix,
                                timing_.document_timing->first_layout);
    ValidateHistogramsForSuffix(::internal::kHistogramLoadEventFiredSuffix,
                                timing_.document_timing->load_event_start);
    ValidateHistogramsForSuffix(
        ::internal::kHistogramFirstContentfulPaintSuffix,
        timing_.paint_timing->first_contentful_paint);
    ValidateHistogramsForSuffix(
        ::internal::kHistogramFirstMeaningfulPaintSuffix,
        timing_.paint_timing->first_meaningful_paint);
    ValidateHistogramsForSuffix(::internal::kHistogramFirstImagePaintSuffix,
                                timing_.paint_timing->first_image_paint);
    ValidateHistogramsForSuffix(::internal::kHistogramFirstPaintSuffix,
                                timing_.paint_timing->first_paint);
    ValidateHistogramsForSuffix(::internal::kHistogramParseStartSuffix,
                                timing_.parse_timing->parse_start);
    ValidateHistogramsForSuffix(
        ::internal::kHistogramParseBlockedOnScriptLoadSuffix,
        timing_.parse_timing->parse_blocked_on_script_load_duration);
    ValidateHistogramsForSuffix(::internal::kHistogramParseDurationSuffix,
                                timing_.parse_timing->parse_stop.value() -
                                    timing_.parse_timing->parse_start.value());
  }

  void ValidateHistogramsForSuffix(
      const std::string& histogram_suffix,
      const base::Optional<base::TimeDelta>& event) {
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(histogram_suffix),
        data_reduction_proxy_used() || cached_data_reduction_proxy_used() ? 1
                                                                          : 0);
    tester()->histogram_tester().ExpectTotalCount(
        std::string(internal::kHistogramDataReductionProxyLitePagePrefix)
            .append(histogram_suffix),
        is_using_lite_page() ? 1 : 0);
    if (!(data_reduction_proxy_used() || cached_data_reduction_proxy_used()))
      return;
    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(histogram_suffix),
        static_cast<base::HistogramBase::Sample>(
            event.value().InMilliseconds()),
        1);
    if (!is_using_lite_page())
      return;
    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyLitePagePrefix)
            .append(histogram_suffix),
        event.value().InMilliseconds(), is_using_lite_page() ? 1 : 0);
  }

  void ValidateDataHistograms(int network_resources,
                              int drp_resources,
                              int64_t network_bytes,
                              int64_t drp_bytes,
                              int64_t ocl_bytes) {
    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(internal::kResourcesPercentProxied),
        100 * drp_resources / network_resources, 1);

    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(internal::kBytesPercentProxied),
        static_cast<int>(100 * drp_bytes / network_bytes), 1);

    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(internal::kNetworkResources),
        network_resources, 1);

    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(internal::kResourcesProxied),
        drp_resources, 1);

    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(internal::kResourcesNotProxied),
        network_resources - drp_resources, 1);

    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(internal::kNetworkBytes),
        static_cast<int>(network_bytes / 1024), 1);

    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(internal::kBytesProxied),
        static_cast<int>(drp_bytes / 1024), 1);

    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(internal::kBytesNotProxied),
        static_cast<int>((network_bytes - drp_bytes) / 1024), 1);

    tester()->histogram_tester().ExpectUniqueSample(
        std::string(internal::kHistogramDataReductionProxyPrefix)
            .append(internal::kBytesOriginal),
        static_cast<int>(ocl_bytes / 1024), 1);
    if (ocl_bytes < network_bytes) {
      tester()->histogram_tester().ExpectUniqueSample(
          std::string(internal::kHistogramDataReductionProxyPrefix)
              .append(internal::kBytesInflationPercent),
          static_cast<int>(100 * network_bytes / ocl_bytes - 100), 1);

      tester()->histogram_tester().ExpectUniqueSample(
          std::string(internal::kHistogramDataReductionProxyPrefix)
              .append(internal::kBytesInflation),
          static_cast<int>((network_bytes - ocl_bytes) / 1024), 1);
    } else {
      tester()->histogram_tester().ExpectUniqueSample(
          std::string(internal::kHistogramDataReductionProxyPrefix)
              .append(internal::kBytesCompressionRatio),
          static_cast<int>(100 * network_bytes / ocl_bytes), 1);

      tester()->histogram_tester().ExpectUniqueSample(
          std::string(internal::kHistogramDataReductionProxyPrefix)
              .append(internal::kBytesSavings),
          static_cast<int>((ocl_bytes - network_bytes) / 1024), 1);
    }
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestDataReductionProxyMetricsObserver>(
            web_contents(), data_reduction_proxy_used(),
            cached_data_reduction_proxy_used(), is_using_lite_page(),
            black_listed()));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyMetricsObserverTest);
};

TEST_F(DataReductionProxyMetricsObserverTest, DataReductionProxyOff) {
  ResetTest();
  // Verify that when the data reduction proxy was not used, no UMA is reported.
  RunTest(false, false, false, false);
  ValidateHistograms();
}

TEST_F(DataReductionProxyMetricsObserverTest, DataReductionProxyOn) {
  ResetTest();
  // Verify that when the data reduction proxy was used, but lite page was not
  // used, the corresponding UMA is reported.
  RunTest(true, false, false, false);
  ValidateHistograms();
}

TEST_F(DataReductionProxyMetricsObserverTest, LitePageEnabled) {
  ResetTest();
  // Verify that when the data reduction proxy was used and lite page was used,
  // both histograms are reported.
  RunTest(true, true, false, false);
  ValidateHistograms();
}

TEST_F(DataReductionProxyMetricsObserverTest, ByteInformationCompression) {
  ResetTest();

  RunTest(true, false, false, false);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  // Cached resource.
  resources.push_back(CreateDataReductionProxyResource(
      true /* was_cached */, 10 * 1024 /* delta_bytes */,
      true /* is_complete */, false /* proxy_used*/));
  // Non data saver resource.
  resources.push_back(CreateDataReductionProxyResource(
      false /* was_cached */, 20 * 1024 /* delta_bytes */,
      true /* is_complete */, false /* proxy_used*/));
  // Data saver resource.
  resources.push_back(CreateDataReductionProxyResource(
      false /* was_cached */, 2 * 1024 /* delta_bytes */,
      true /* is_complete */, true /* proxy_used*/,
      0.5 /* compression_ratio */));
  // Data saver incomplete resource.
  resources.push_back(CreateDataReductionProxyResource(
      false /* was_cached */, 3 * 1024 /* delta_bytes */,
      false /* is_complete */, true /* proxy_used*/,
      0.5 /* compression_ratio */));

  tester()->SimulateResourceDataUseUpdate(resources);

  int network_resources = 0;
  int drp_resources = 0;
  int64_t insecure_network_bytes = 0;
  int64_t secure_network_bytes = 0;
  int64_t drp_bytes = 0;
  int64_t insecure_ocl_bytes = 0;
  int64_t secure_ocl_bytes = 0;
  for (const auto& request : resources) {
    if (request->cache_type ==
        page_load_metrics::mojom::CacheType::kNotCached) {
      if (request->is_secure_scheme) {
        secure_network_bytes += request->delta_bytes;
        secure_ocl_bytes +=
            request->delta_bytes *
            request->data_reduction_proxy_compression_ratio_estimate;
      } else {
        insecure_network_bytes += request->delta_bytes;
        insecure_ocl_bytes +=
            request->delta_bytes *
            request->data_reduction_proxy_compression_ratio_estimate;
      }
      if (request->is_complete)
        ++network_resources;
    }
    if (request->proxy_used) {
      drp_bytes += request->delta_bytes;
      if (request->cache_type ==
              page_load_metrics::mojom::CacheType::kNotCached &&
          request->is_complete)
        ++drp_resources;
    }
  }
  tester()->NavigateToUntrackedUrl();

  ValidateDataHistograms(network_resources, drp_resources,
                         insecure_network_bytes + secure_network_bytes,
                         drp_bytes, insecure_ocl_bytes + secure_ocl_bytes);
}

TEST_F(DataReductionProxyMetricsObserverTest, ByteInformationInflation) {
  ResetTest();

  RunTest(true, false, false, false);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  // Cached resource.
  resources.push_back(CreateDataReductionProxyResource(
      true /* was_cached */, 10 * 1024 /* delta_bytes */,
      true /* is_complete */, false /* proxy_used*/));
  // Non data saver resource.
  resources.push_back(CreateDataReductionProxyResource(
      false /* was_cached */, 20 * 1024 /* delta_bytes */,
      true /* is_complete */, false /* proxy_used*/));
  // Data saver inflated resource.
  resources.push_back(CreateDataReductionProxyResource(
      false /* was_cached */, 2 * 1024 /* delta_bytes */,
      true /* is_complete */, true /* proxy_used*/, 5 /* compression_ratio */));
  // Data saver incomplete inflated resource.
  resources.push_back(CreateDataReductionProxyResource(
      false /* was_cached */, 3 * 1024 /* delta_bytes */,
      false /* is_complete */, true /* proxy_used*/,
      10 /* compression_ratio */));

  tester()->SimulateResourceDataUseUpdate(resources);

  int network_resources = 0;
  int drp_resources = 0;
  int64_t insecure_network_bytes = 0;
  int64_t secure_network_bytes = 0;
  int64_t drp_bytes = 0;
  int64_t secure_drp_bytes = 0;
  int64_t insecure_ocl_bytes = 0;
  int64_t secure_ocl_bytes = 0;
  for (const auto& request : resources) {
    if (request->cache_type ==
        page_load_metrics::mojom::CacheType::kNotCached) {
      if (request->is_secure_scheme) {
        secure_network_bytes += request->delta_bytes;
        secure_ocl_bytes +=
            request->delta_bytes *
            request->data_reduction_proxy_compression_ratio_estimate;
      } else {
        insecure_network_bytes += request->delta_bytes;
        insecure_ocl_bytes +=
            request->delta_bytes *
            request->data_reduction_proxy_compression_ratio_estimate;
      }
      if (request->is_complete)
        ++network_resources;
    }
    if (request->proxy_used) {
      if (request->is_secure_scheme)
        secure_drp_bytes += request->delta_bytes;
      else
        drp_bytes += request->delta_bytes;
      if (request->cache_type ==
              page_load_metrics::mojom::CacheType::kNotCached &&
          request->is_complete)
        ++drp_resources;
    }
  }
  tester()->NavigateToUntrackedUrl();

  ValidateDataHistograms(network_resources, drp_resources,
                         insecure_network_bytes + secure_network_bytes,
                         drp_bytes + secure_drp_bytes,
                         insecure_ocl_bytes + secure_ocl_bytes);
}

}  //  namespace data_reduction_proxy
