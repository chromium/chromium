// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/previews_ukm_observer.h"

#include <memory>
#include <unordered_map>

#include "base/base64.h"
#include "base/macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_event.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/ip_endpoint.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace content {
class NavigationHandle;
}

namespace previews {

namespace {

const char kDefaultTestUrl[] = "https://www.google.com/";

class TestPreviewsUKMObserver : public PreviewsUKMObserver {
 public:
  explicit TestPreviewsUKMObserver(bool save_data_enabled)
      : save_data_enabled_(save_data_enabled) {}

  ~TestPreviewsUKMObserver() override {}

  // page_load_metrics::PageLoadMetricsObserver implementation:
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override {
    return PreviewsUKMObserver::OnCommit(navigation_handle, source_id);
  }

 private:
  bool IsDataSaverEnabled(
      content::NavigationHandle* navigation_handle) const override {
    return save_data_enabled_;
  }

  const bool save_data_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestPreviewsUKMObserver);
};

class PreviewsUKMObserverTest
    : public page_load_metrics::PageLoadMetricsObserverTestHarness {
 public:
  PreviewsUKMObserverTest() {}
  ~PreviewsUKMObserverTest() override {}

  void RunTest(bool save_data_enabled) {
    save_data_enabled_ = save_data_enabled;
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL(kDefaultTestUrl), web_contents());

    navigation->Commit();
  }

  void ValidatePreviewsUKM(bool save_data_enabled_expected) {
    using UkmEntry = ukm::builders::Previews;
    auto entries =
        tester()->test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);
    if (!save_data_enabled_expected) {
      EXPECT_EQ(0u, entries.size());
      return;
    }
    EXPECT_EQ(1u, entries.size());

    const auto* const entry = entries.front();
    tester()->test_ukm_recorder().ExpectEntrySourceHasUrl(
        entry, GURL(kDefaultTestUrl));

    EXPECT_EQ(save_data_enabled_expected,
              tester()->test_ukm_recorder().EntryHasMetric(
                  entry, UkmEntry::ksave_data_enabledName));
  }

  void SetUp() override {
    page_load_metrics::PageLoadMetricsObserverTestHarness::SetUp();
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(
        std::make_unique<TestPreviewsUKMObserver>(save_data_enabled_));
  }

 private:
  bool save_data_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(PreviewsUKMObserverTest);
};


TEST_F(PreviewsUKMObserverTest, DataSaverEnabled) {
  RunTest(true);

  tester()->NavigateToUntrackedUrl();

  ValidatePreviewsUKM(true);
}




TEST_F(PreviewsUKMObserverTest, CheckReportingForHidden) {
  RunTest(true);

  web_contents()->WasHidden();

  ValidatePreviewsUKM(true);
}

TEST_F(PreviewsUKMObserverTest, CheckReportingForFlushMetrics) {
  RunTest(true);

  tester()->SimulateAppEnterBackground();

  ValidatePreviewsUKM(true);
}


}  // namespace

}  // namespace previews
