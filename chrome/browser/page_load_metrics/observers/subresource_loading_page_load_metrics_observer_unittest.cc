// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/subresource_loading_page_load_metrics_observer.h"

#include <memory>

#include "base/command_line.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace {

const GURL kTestUrl("https://chromium.org");

page_load_metrics::mojom::ResourceDataUpdatePtr CreateBaseResource(
    bool was_cached,
    bool is_complete) {
  return CreateResource(was_cached, 1234 /* delta_bytes */,
                        1234 /* encoded_body_length */, is_complete);
}

}  // namespace

class TestSubresourceLoadingPageLoadMetricsObserver
    : public SubresourceLoadingPageLoadMetricsObserver {
 public:
  void CallOnOriginLastVisitResult(
      history::HistoryLastVisitToHostResult result) {
    OnOriginLastVisitResult(base::Time::Now(), result);
  }
};

class SubresourceLoadingPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  SubresourceLoadingPageLoadMetricsObserverTest() = default;

  TestSubresourceLoadingPageLoadMetricsObserver* plm_observer() {
    return plm_observer_;
  }
  void set_navigation_url(const GURL& url) { navigation_url_ = url; }
  void set_in_main_frame(bool in_main_frame) { in_main_frame_ = in_main_frame; }

  void StartTest(bool data_saver_enabled) {
    ResetTest();

    if (data_saver_enabled) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          data_reduction_proxy::switches::kEnableDataReductionProxy);
    }

    NavigateAndCommit(navigation_url_);
    tester()->SimulateTimingUpdate(timing_);
  }

  void VerifyNoUKM() {
    auto entries = tester()->test_ukm_recorder().GetEntriesByName(
        ukm::builders::PrefetchProxy::kEntryName);
    EXPECT_TRUE(entries.empty());
  }

  void VerifyUKMEntry(const std::string& metric_name,
                      base::Optional<int64_t> expected_value) {
    auto entries = tester()->test_ukm_recorder().GetEntriesByName(
        ukm::builders::PrefetchProxy::kEntryName);
    ASSERT_EQ(1U, entries.size());

    const auto* entry = entries.front();
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          navigation_url_);

    const int64_t* value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
    EXPECT_EQ(value != nullptr, expected_value.has_value());

    if (!expected_value.has_value())
      return;

    EXPECT_EQ(*value, expected_value.value());
  }

  page_load_metrics::mojom::ResourceDataUpdatePtr CreateCSSResource(
      bool was_cached,
      bool is_complete,
      bool completed_before_fcp) {
    page_load_metrics::mojom::ResourceDataUpdatePtr update =
        CreateBaseResource(was_cached, is_complete);
    update->mime_type = "text/css";
    update->is_main_frame_resource = in_main_frame_;
    update->completed_before_fcp = completed_before_fcp;
    return update;
  }

  page_load_metrics::mojom::ResourceDataUpdatePtr CreateJSResource(
      bool was_cached,
      bool is_complete,
      bool completed_before_fcp) {
    page_load_metrics::mojom::ResourceDataUpdatePtr update =
        CreateBaseResource(was_cached, is_complete);
    update->mime_type = "text/javascript";
    update->is_main_frame_resource = in_main_frame_;
    update->completed_before_fcp = completed_before_fcp;
    return update;
  }

  page_load_metrics::mojom::ResourceDataUpdatePtr CreateOtherResource(
      bool was_cached,
      bool is_complete,
      bool completed_before_fcp) {
    page_load_metrics::mojom::ResourceDataUpdatePtr update =
        CreateBaseResource(was_cached, is_complete);
    update->mime_type = "other";
    update->is_main_frame_resource = in_main_frame_;
    update->completed_before_fcp = completed_before_fcp;
    return update;
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    std::unique_ptr<TestSubresourceLoadingPageLoadMetricsObserver> observer =
        std::make_unique<TestSubresourceLoadingPageLoadMetricsObserver>();
    plm_observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

 private:
  void ResetTest() {
    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    timing_.navigation_start = base::Time::FromDoubleT(2);
    timing_.response_start = base::TimeDelta::FromSeconds(3);
    timing_.parse_timing->parse_start = base::TimeDelta::FromSeconds(4);
    timing_.paint_timing->first_contentful_paint =
        base::TimeDelta::FromSeconds(5);
    timing_.paint_timing->first_image_paint = base::TimeDelta::FromSeconds(6);
    timing_.document_timing->load_event_start = base::TimeDelta::FromSeconds(7);
    PopulateRequiredTimingFields(&timing_);
  }

  TestSubresourceLoadingPageLoadMetricsObserver* plm_observer_ = nullptr;
  page_load_metrics::mojom::PageLoadTiming timing_;

  GURL navigation_url_ = kTestUrl;
  bool in_main_frame_ = true;

  DISALLOW_COPY_AND_ASSIGN(SubresourceLoadingPageLoadMetricsObserverTest);
};

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, BeforeFCP_CSS) {
  StartTest(true /* data_saver_enabled */);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));

  resources.push_back(CreateCSSResource(false /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(false /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));

  tester()->SimulateResourceDataUseUpdate(resources);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 2,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 3, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_network_before_fcpName, 2);
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_cache_before_fcpName, 3);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, BeforeFCP_JS) {
  StartTest(true /* data_saver_enabled */);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateJSResource(true /* was_cached */,
                                       true /* is_complete */,
                                       true /* completed_before_fcp */));
  resources.push_back(CreateJSResource(true /* was_cached */,
                                       true /* is_complete */,
                                       true /* completed_before_fcp */));
  resources.push_back(CreateJSResource(true /* was_cached */,
                                       true /* is_complete */,
                                       true /* completed_before_fcp */));

  resources.push_back(CreateJSResource(false /* was_cached */,
                                       true /* is_complete */,
                                       true /* completed_before_fcp */));
  resources.push_back(CreateJSResource(false /* was_cached */,
                                       true /* is_complete */,
                                       true /* completed_before_fcp */));

  tester()->SimulateResourceDataUseUpdate(resources);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 2,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 3, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_network_before_fcpName, 2);
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_cache_before_fcpName, 3);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, BeforeFCP_Other) {
  StartTest(true /* data_saver_enabled */);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateOtherResource(true /* was_cached */,
                                          true /* is_complete */,
                                          true /* completed_before_fcp */));
  resources.push_back(CreateOtherResource(true /* was_cached */,
                                          true /* is_complete */,
                                          true /* completed_before_fcp */));
  resources.push_back(CreateOtherResource(true /* was_cached */,
                                          true /* is_complete */,
                                          true /* completed_before_fcp */));

  resources.push_back(CreateOtherResource(false /* was_cached */,
                                          true /* is_complete */,
                                          true /* completed_before_fcp */));
  resources.push_back(CreateOtherResource(false /* was_cached */,
                                          true /* is_complete */,
                                          true /* completed_before_fcp */));

  tester()->SimulateResourceDataUseUpdate(resources);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 0,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 0, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_network_before_fcpName, 0);
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_cache_before_fcpName, 0);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, BeforeFCP_NotComplete) {
  StartTest(true /* data_saver_enabled */);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        false /* is_complete */,
                                        false /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        false /* is_complete */,
                                        false /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        false /* is_complete */,
                                        false /* completed_before_fcp */));

  resources.push_back(CreateCSSResource(false /* was_cached */,
                                        false /* is_complete */,
                                        false /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(false /* was_cached */,
                                        false /* is_complete */,
                                        false /* completed_before_fcp */));

  tester()->SimulateResourceDataUseUpdate(resources);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 0,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 0, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_network_before_fcpName, 0);
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_cache_before_fcpName, 0);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, BeforeFCP_Subframe) {
  StartTest(true /* data_saver_enabled */);
  set_in_main_frame(false);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));

  resources.push_back(CreateCSSResource(false /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(false /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));

  tester()->SimulateResourceDataUseUpdate(resources);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 0,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 0, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_network_before_fcpName, 0);
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_cache_before_fcpName, 0);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, AfterFCP) {
  StartTest(true /* data_saver_enabled */);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        false /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        false /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        false /* completed_before_fcp */));

  resources.push_back(CreateCSSResource(false /* was_cached */,
                                        true /* is_complete */,
                                        false /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(false /* was_cached */,
                                        true /* is_complete */,
                                        false /* completed_before_fcp */));

  tester()->SimulateResourceDataUseUpdate(resources);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 0,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 0, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_network_before_fcpName, 0);
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_cache_before_fcpName, 0);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, BeforeFCP_MaxUKM) {
  StartTest(true /* data_saver_enabled */);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));

  tester()->SimulateResourceDataUseUpdate(resources);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 0,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 11, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_network_before_fcpName, 0);
  VerifyUKMEntry(UkmEntry::kcount_css_js_loaded_cache_before_fcpName, 10);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, BeforeFCP_NoUKM) {
  StartTest(false /* data_saver_enabled */);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));

  tester()->SimulateResourceDataUseUpdate(resources);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 0,
      1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 2, 1);

  VerifyNoUKM();
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, DontRecordForNonHttp) {
  set_navigation_url(GURL("chrome://version"));

  StartTest(true /* data_saver_enabled */);

  std::vector<page_load_metrics::mojom::ResourceDataUpdatePtr> resources;
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(true /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));

  resources.push_back(CreateCSSResource(false /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));
  resources.push_back(CreateCSSResource(false /* was_cached */,
                                        true /* is_complete */,
                                        true /* completed_before_fcp */));

  tester()->SimulateResourceDataUseUpdate(resources);
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Noncached", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.LoadedCSSJSBeforeFCP.Cached", 0);

  VerifyNoUKM();
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, LastVisitToHost_None) {
  StartTest(true /* data_saver_enabled */);

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HistoryQueryTime", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName, base::nullopt);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, LastVisitToHost_Fail) {
  StartTest(true /* data_saver_enabled */);
  plm_observer()->CallOnOriginLastVisitResult(
      {false /* success */, base::Time()});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HistoryQueryTime", 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName, base::nullopt);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest,
       LastVisitToHost_NullTime) {
  StartTest(true /* data_saver_enabled */);
  plm_observer()->CallOnOriginLastVisitResult(
      {true /* success */, base::Time()});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HistoryQueryTime", 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName, -1);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, LastVisitToHost_Today) {
  StartTest(true /* data_saver_enabled */);
  plm_observer()->CallOnOriginLastVisitResult(
      {true /* success */, base::Time::Now()});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HistoryQueryTime", 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName, 0);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest,
       LastVisitToHost_Yesterday) {
  StartTest(true /* data_saver_enabled */);
  plm_observer()->CallOnOriginLastVisitResult(
      {true /* success */, base::Time::Now() - base::TimeDelta::FromDays(1)});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HistoryQueryTime", 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 1, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName, 1);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, LastVisitToHost_MaxUKM) {
  StartTest(true /* data_saver_enabled */);
  plm_observer()->CallOnOriginLastVisitResult(
      {true /* success */, base::Time::Now() - base::TimeDelta::FromDays(181)});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HistoryQueryTime", 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 181, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName,
                 /*ukm::GetExponentialBucketMin(180,1.70)=*/119);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, LastVisitToHost_NoUKM) {
  StartTest(false /* data_saver_enabled */);
  plm_observer()->CallOnOriginLastVisitResult(
      {true /* success */, base::Time::Now() - base::TimeDelta::FromDays(1)});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HistoryQueryTime", 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 1, 1);

  VerifyNoUKM();
}

// The rest of cookie testing is done in
// SubresourceLoadingPageLoadMetricsObserverBrowserTest.
TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, HadCookies_None) {
  StartTest(true /* data_saver_enabled */);

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.CookiesQueryTime", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", 0);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kmainpage_request_had_cookiesName, base::nullopt);
}

TEST_F(SubresourceLoadingPageLoadMetricsObserverTest, HadCookies_NoUKM) {
  StartTest(false /* data_saver_enabled */);

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.CookiesQueryTime", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.MainFrameHadCookies", 0);

  VerifyNoUKM();
}
