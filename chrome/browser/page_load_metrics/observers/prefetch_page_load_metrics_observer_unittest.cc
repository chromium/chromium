// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/prefetch_page_load_metrics_observer.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

class TestPrefetchPageLoadMetricsObserver
    : public PrefetchPageLoadMetricsObserver {
 public:
  void CallOnOriginLastVisitResult(history::HistoryLastVisitResult result) {
    OnOriginLastVisitResult(base::Time::Now(), result);
  }
};

class PrefetchPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  PrefetchPageLoadMetricsObserverTest() = default;

  PrefetchPageLoadMetricsObserverTest(
      const PrefetchPageLoadMetricsObserverTest&) = delete;
  PrefetchPageLoadMetricsObserverTest& operator=(
      const PrefetchPageLoadMetricsObserverTest&) = delete;

  TestPrefetchPageLoadMetricsObserver* plm_observer() { return plm_observer_; }
  void set_navigation_url(const GURL& url) { navigation_url_ = url; }
  void set_in_main_frame(bool in_main_frame) { in_main_frame_ = in_main_frame; }

  void StartTest() {
    ResetTest();

    NavigateAndCommit(navigation_url_);
    tester()->SimulateTimingUpdate(timing_);
  }

  void VerifyNoUKM() {
    auto entries = tester()->test_ukm_recorder().GetEntriesByName(
        ukm::builders::PrefetchProxy::kEntryName);
    EXPECT_TRUE(entries.empty());
  }

  void VerifyUKMEntry(const std::string& metric_name,
                      std::optional<int64_t> expected_value) {
    auto entries = tester()->test_ukm_recorder().GetEntriesByName(
        ukm::builders::PrefetchProxy::kEntryName);
    ASSERT_EQ(1U, entries.size());

    const auto* entry = entries.front().get();
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          navigation_url_);

    const int64_t* value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
    EXPECT_EQ(value != nullptr, expected_value.has_value());

    if (!expected_value.has_value()) {
      return;
    }

    EXPECT_EQ(*value, expected_value.value());
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    std::unique_ptr<TestPrefetchPageLoadMetricsObserver> observer =
        std::make_unique<TestPrefetchPageLoadMetricsObserver>();
    plm_observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

 private:
  void ResetTest() {
    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    timing_.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
    timing_.response_start = base::Seconds(3);
    timing_.parse_timing->parse_start = base::Seconds(4);
    timing_.paint_timing->first_contentful_paint = base::Seconds(5);
    timing_.paint_timing->first_image_paint = base::Seconds(6);
    timing_.document_timing->load_event_start = base::Seconds(7);
    PopulateRequiredTimingFields(&timing_);
  }

  raw_ptr<TestPrefetchPageLoadMetricsObserver, DanglingUntriaged>
      plm_observer_ = nullptr;
  page_load_metrics::mojom::PageLoadTiming timing_;

  GURL navigation_url_{"https://chromium.org"};
  bool in_main_frame_ = true;
};

// TODO(crbug.com/40899584): Fix and enable this test.
TEST_F(PrefetchPageLoadMetricsObserverTest, DISABLED_DontRecordForNonHttp) {
  set_navigation_url(GURL("chrome://version"));

  StartTest();

  tester()->NavigateToUntrackedUrl();

  VerifyNoUKM();
}

TEST_F(PrefetchPageLoadMetricsObserverTest, LastVisitToHost_None) {
  StartTest();

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName, std::nullopt);
}

TEST_F(PrefetchPageLoadMetricsObserverTest, LastVisitToHost_Fail) {
  StartTest();
  plm_observer()->CallOnOriginLastVisitResult(
      {false /* success */, base::Time()});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", 0);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName, std::nullopt);
}

TEST_F(PrefetchPageLoadMetricsObserverTest, LastVisitToHost_NullTime) {
  StartTest();
  plm_observer()->CallOnOriginLastVisitResult(
      {true /* success */, base::Time()});
  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", false, 1);
  tester()->histogram_tester().ExpectTotalCount(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName, -1);
}

TEST_F(PrefetchPageLoadMetricsObserverTest, LastVisitToHost_Today) {
  StartTest();
  plm_observer()->CallOnOriginLastVisitResult(
      {true /* success */, base::Time::Now()});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 0, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName, 0);
}

TEST_F(PrefetchPageLoadMetricsObserverTest, LastVisitToHost_Yesterday) {
  StartTest();
  plm_observer()->CallOnOriginLastVisitResult(
      {true /* success */, base::Time::Now() - base::Days(1)});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 1, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName, 1);
}

TEST_F(PrefetchPageLoadMetricsObserverTest, LastVisitToHost_MaxUKM) {
  StartTest();
  plm_observer()->CallOnOriginLastVisitResult(
      {true /* success */, base::Time::Now() - base::Days(181)});

  tester()->NavigateToUntrackedUrl();

  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.HasPreviousVisitToOrigin", true, 1);
  tester()->histogram_tester().ExpectUniqueSample(
      "PageLoad.Clients.SubresourceLoading.DaysSinceLastVisitToOrigin", 181, 1);

  using UkmEntry = ukm::builders::PrefetchProxy;
  VerifyUKMEntry(UkmEntry::kdays_since_last_visit_to_originName,
                 /*ukm::GetExponentialBucketMin(180,1.70)=*/119);
}
