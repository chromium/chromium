// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_anchors_metrics_observer.h"

#include "chrome/browser/navigation_predictor/navigation_predictor_metrics_document_data.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

class TestPageAnchorsMetricsObserver : public PageAnchorsMetricsObserver {
 public:
  explicit TestPageAnchorsMetricsObserver(content::WebContents* web_contents)
      : PageAnchorsMetricsObserver(web_contents) {}
  ukm::SourceId GetUkmSourceId() const {
    return GetDelegate().GetPageUkmSourceId();
  }
};

class PageAnchorsMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  PageAnchorsMetricsObserverTest() = default;

  PageAnchorsMetricsObserverTest(const PageAnchorsMetricsObserverTest&) =
      delete;
  PageAnchorsMetricsObserverTest& operator=(
      const PageAnchorsMetricsObserverTest&) = delete;

  TestPageAnchorsMetricsObserver* pam_observer() { return pam_observer_; }
  void set_navigation_url(const GURL& url) { navigation_url_ = url; }
  void set_in_main_frame(bool in_main_frame) { in_main_frame_ = in_main_frame; }

  void StartTest() {
    ResetTest();

    NavigateAndCommit(navigation_url_);
    tester()->SimulateTimingUpdate(timing_);
  }

  NavigationPredictorMetricsDocumentData&
  GetNavigationPredictorMetricsDocumentData() {
    // Create the `NavigationPredictorMetricsDocumentData` object for this
    // document if it doesn't already exist.
    NavigationPredictorMetricsDocumentData* data =
        NavigationPredictorMetricsDocumentData::GetOrCreateForCurrentDocument(
            main_rfh());
    EXPECT_TRUE(data);
    return *data;
  }

  void VerifyNoUKM(const std::string& entry_name) {
    auto entries = tester()->test_ukm_recorder().GetEntriesByName(entry_name);
    EXPECT_TRUE(entries.empty());
  }

  void VerifyUKMEntry(const std::string& entry_name,
                      const std::string& metric_name,
                      int64_t expected_value) {
    auto entries = tester()->test_ukm_recorder().GetEntriesByName(entry_name);
    ASSERT_EQ(1U, entries.size());

    const auto* entry = entries.front().get();
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(entry,
                                                          navigation_url_);

    const int64_t* value =
        ukm::TestUkmRecorder::GetEntryMetric(entry, metric_name);
    EXPECT_TRUE(value != nullptr);
    EXPECT_EQ(expected_value, *value);
  }

  ukm::SourceId GetUkmSourceId() const {
    return pam_observer_->GetUkmSourceId();
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    std::unique_ptr<TestPageAnchorsMetricsObserver> observer =
        std::make_unique<TestPageAnchorsMetricsObserver>(web_contents());
    pam_observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

 private:
  void ResetTest() {
    GetNavigationPredictorMetricsDocumentData().ClearUserInteractionsData();

    page_load_metrics::InitPageLoadTimingForTest(&timing_);
    timing_.navigation_start = base::Time::FromSecondsSinceUnixEpoch(2);
    timing_.response_start = base::Seconds(3);
    timing_.parse_timing->parse_start = base::Seconds(4);
    timing_.paint_timing->first_contentful_paint = base::Seconds(5);
    timing_.paint_timing->first_image_paint = base::Seconds(6);
    timing_.document_timing->load_event_start = base::Seconds(7);
    PopulateRequiredTimingFields(&timing_);
  }

  raw_ptr<TestPageAnchorsMetricsObserver, DanglingUntriaged> pam_observer_ =
      nullptr;
  page_load_metrics::mojom::PageLoadTiming timing_;

  GURL navigation_url_{"https://chromium.org"};
  bool in_main_frame_ = true;
};

TEST_F(PageAnchorsMetricsObserverTest,
       NavigateToUntrackedUrlShouldForceRecordingUkmData) {
  StartTest();

  NavigationPredictorMetricsDocumentData::UserInteractionsData
      user_interactions;
  GetNavigationPredictorMetricsDocumentData().AddUserInteractionsData(
      0, user_interactions);
  tester()->NavigateToUntrackedUrl();

  using UkmEntry = ukm::builders::NavigationPredictorUserInteractions;
  VerifyUKMEntry(UkmEntry::kEntryName, UkmEntry::kAnchorIndexName, 0);
}
