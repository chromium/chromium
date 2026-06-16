// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/initial_webui_page_load_metrics_observer.h"

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_tester.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/gmock_matchers.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "services/metrics/public/cpp/delegating_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::NavigationSimulator;
using page_load_metrics::PageLoadMetricsObserverTester;

namespace {

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::ReturnRef;

const char kTestWebUIUrl[] = "chrome://webui-toolbar.top-chrome/";

using ukm::testing::HasMetric;
using ukm::testing::HasMetricWithValue;

MATCHER_P2(HasUkmSourceUrl, recorder, source_url, "") {
  const ukm::UkmSource* source = recorder->GetSourceForSourceId(arg->source_id);
  return source && source->url() == source_url;
}

class InitialWebUIPageLoadMetricsObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  InitialWebUIPageLoadMetricsObserverTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kInitialWebUI, features::kInitialWebUIMetrics,
         features::kWebUIReloadButton},
        {});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
    tester_ = std::make_unique<PageLoadMetricsObserverTester>(
        web_contents(), this,
        base::BindRepeating(
            &InitialWebUIPageLoadMetricsObserverTest::RegisterObservers,
            base::Unretained(this)),
        /* is_non_tab_webui */ true);
    web_contents()->WasShown();
  }

  PageLoadMetricsObserverTester* tester() { return tester_.get(); }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) {
    auto observer = std::make_unique<InitialWebUIPageLoadMetricsObserver>();
    observer_ = observer.get();
    tracker->AddObserver(std::move(observer));
  }

  raw_ptr<InitialWebUIPageLoadMetricsObserver, DanglingUntriaged> observer_ =
      nullptr;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PageLoadMetricsObserverTester> tester_;
};

TEST_F(InitialWebUIPageLoadMetricsObserverTest,
       RecordMetricsOnCommitAndComplete) {
  page_load_metrics::mojom::PageLoadTiming timing;
  page_load_metrics::InitPageLoadTimingForTest(&timing);
  timing.navigation_start = base::Time::FromSecondsSinceUnixEpoch(1);
  timing.paint_timing->first_paint = base::Milliseconds(10);
  timing.paint_timing->first_contentful_paint = base::Milliseconds(20);
  timing.document_timing->dom_content_loaded_event_start =
      base::Milliseconds(30);
  timing.document_timing->load_event_start = base::Milliseconds(40);
  timing.parse_timing->parse_start = base::Milliseconds(5);
  PopulateRequiredTimingFields(&timing);

  page_load_metrics::mojom::CpuTiming cpu_timing;
  cpu_timing.task_time = base::Milliseconds(50);

  NavigateAndCommit(GURL(kTestWebUIUrl));

  tester()->SimulateTimingUpdate(timing);
  tester()->SimulateCpuTimingUpdate(cpu_timing);

  // Trigger observer destruction by deleting WebContents
  DeleteContents();

  auto entries = test_ukm_recorder_.GetEntriesByName("InitialWebUIPageLoad");
  EXPECT_THAT(entries,
              Each(HasUkmSourceUrl(&test_ukm_recorder_, GURL(kTestWebUIUrl))));
  EXPECT_THAT(entries,
              Contains(HasMetricWithValue(
                  "PaintTiming.NavigationToFirstContentfulPaint", 20)));
  EXPECT_THAT(
      entries,
      Contains(AllOf(
          HasMetricWithValue("PaintTiming.NavigationToFirstPaint", 10),
          HasMetricWithValue(
              "DocumentTiming.NavigationToDOMContentLoadedEventFired", 30),
          HasMetricWithValue("DocumentTiming.NavigationToLoadEventFired", 40),
          HasMetricWithValue("ParseTiming.NavigationToParseStart", 5))));
  EXPECT_THAT(entries, Contains(HasMetricWithValue("CpuTimeMs", 50)));
}

TEST_F(InitialWebUIPageLoadMetricsObserverTest, NavigationTiming) {
  NavigateAndCommit(GURL(kTestWebUIUrl));

  base::TimeTicks navigation_start =
      tester()->GetDelegateForCommittedLoad().GetNavigationStart();

  content::NavigationHandleTiming timing;
  timing.first_request_start_time = navigation_start + base::Milliseconds(10);
  timing.first_response_start_time = navigation_start + base::Milliseconds(20);
  timing.navigation_commit_sent_time =
      navigation_start + base::Milliseconds(25);
  timing.navigation_commit_received_time =
      navigation_start + base::Milliseconds(30);
  timing.navigation_commit_reply_sent_time =
      navigation_start + base::Milliseconds(40);
  timing.navigation_did_commit_time = navigation_start + base::Milliseconds(50);

  content::MockNavigationHandle handle(GURL(kTestWebUIUrl), main_rfh());
  EXPECT_CALL(handle, GetNavigationHandleTiming())
      .WillRepeatedly(ReturnRef(timing));
  EXPECT_CALL(handle, IsSameProcess()).WillRepeatedly(::testing::Return(false));

  observer_->OnCommit(&handle);

  // Trigger observer destruction by deleting WebContents
  DeleteContents();

  auto entries =
      test_ukm_recorder_.GetEntriesByName("InitialWebUINavigationTiming");
  EXPECT_THAT(entries, ElementsAre(AllOf(HasUkmSourceUrl(&test_ukm_recorder_,
                                                         GURL(kTestWebUIUrl)),
                                         HasMetric("FirstRequestStart"),
                                         HasMetric("FirstResponseStart"),
                                         HasMetric("NavigationCommitSent"),
                                         HasMetric("NavigationCommitReceived"),
                                         HasMetric("NavigationCommitReplySent"),
                                         HasMetric("NavigationDidCommit"))));
}

TEST_F(InitialWebUIPageLoadMetricsObserverTest, NavigationTiming_NullField) {
  NavigateAndCommit(GURL(kTestWebUIUrl));

  base::TimeTicks navigation_start =
      tester()->GetDelegateForCommittedLoad().GetNavigationStart();

  content::NavigationHandleTiming timing;
  timing.first_request_start_time = navigation_start + base::Milliseconds(10);
  timing.first_response_start_time = navigation_start + base::Milliseconds(20);
  // navigation_commit_received_time is left null!
  timing.navigation_commit_reply_sent_time =
      navigation_start + base::Milliseconds(40);
  timing.navigation_did_commit_time = navigation_start + base::Milliseconds(50);

  content::MockNavigationHandle handle(GURL(kTestWebUIUrl), main_rfh());
  EXPECT_CALL(handle, GetNavigationHandleTiming())
      .WillRepeatedly(ReturnRef(timing));
  EXPECT_CALL(handle, IsSameProcess()).WillRepeatedly(::testing::Return(false));

  observer_->OnCommit(&handle);

  DeleteContents();

  auto entries =
      test_ukm_recorder_.GetEntriesByName("InitialWebUINavigationTiming");
  EXPECT_THAT(entries, IsEmpty());
}

TEST_F(InitialWebUIPageLoadMetricsObserverTest, NavigationTiming_OutOfOrder) {
  NavigateAndCommit(GURL(kTestWebUIUrl));

  base::TimeTicks navigation_start =
      tester()->GetDelegateForCommittedLoad().GetNavigationStart();

  content::NavigationHandleTiming timing;
  timing.first_request_start_time = navigation_start + base::Milliseconds(10);
  timing.first_response_start_time = navigation_start + base::Milliseconds(20);
  timing.navigation_commit_sent_time =
      navigation_start + base::Milliseconds(25);
  // Out of order: reply_sent_time (30) is earlier than commit_received_time
  // (40)
  timing.navigation_commit_received_time =
      navigation_start + base::Milliseconds(40);
  timing.navigation_commit_reply_sent_time =
      navigation_start + base::Milliseconds(30);
  timing.navigation_did_commit_time = navigation_start + base::Milliseconds(50);

  content::MockNavigationHandle handle(GURL(kTestWebUIUrl), main_rfh());
  EXPECT_CALL(handle, GetNavigationHandleTiming())
      .WillRepeatedly(ReturnRef(timing));
  EXPECT_CALL(handle, IsSameProcess()).WillRepeatedly(::testing::Return(false));

  observer_->OnCommit(&handle);

  DeleteContents();

  auto entries =
      test_ukm_recorder_.GetEntriesByName("InitialWebUINavigationTiming");
  EXPECT_THAT(entries, IsEmpty());
}

TEST_F(InitialWebUIPageLoadMetricsObserverTest, WasCached) {
  // Simulate cached response
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateBrowserInitiated(GURL(kTestWebUIUrl),
                                                  web_contents());
  simulator->Start();
  simulator->SetWasFetchedViaCache(true);
  simulator->Commit();

  DeleteContents();

  auto entries = test_ukm_recorder_.GetEntriesByName("InitialWebUIPageLoad");
  EXPECT_THAT(entries, Contains(HasMetricWithValue("WasCached", 1)));
}

TEST_F(InitialWebUIPageLoadMetricsObserverTest, OnFailedProvisionalLoad) {
  std::unique_ptr<NavigationSimulator> simulator =
      NavigationSimulator::CreateBrowserInitiated(GURL(kTestWebUIUrl),
                                                  web_contents());
  simulator->Start();
  simulator->Fail(net::ERR_CONNECTION_RESET);
  DeleteContents();

  auto entries = test_ukm_recorder_.GetEntriesByName("InitialWebUIPageLoad");
  EXPECT_THAT(entries,
              Contains(AllOf(
                  HasMetricWithValue("Net.ErrorCode.OnFailedProvisionalLoad",
                                     net::ERR_CONNECTION_RESET * -1),
                  HasMetric("PageTiming.NavigationToFailedProvisionalLoad"))));
}

TEST_F(InitialWebUIPageLoadMetricsObserverTest,
       NavigationTiming_StartEarlierThanRequest) {
  NavigateAndCommit(GURL(kTestWebUIUrl));

  base::TimeTicks navigation_start =
      tester()->GetDelegateForCommittedLoad().GetNavigationStart();

  content::NavigationHandleTiming timing;
  // Out of order: first_request_start_time (navigation_start - 10ms) is earlier
  // than navigation_start
  timing.first_request_start_time = navigation_start - base::Milliseconds(10);
  timing.first_response_start_time = navigation_start + base::Milliseconds(20);
  timing.navigation_commit_sent_time =
      navigation_start + base::Milliseconds(25);
  timing.navigation_commit_received_time =
      navigation_start + base::Milliseconds(30);
  timing.navigation_commit_reply_sent_time =
      navigation_start + base::Milliseconds(40);
  timing.navigation_did_commit_time = navigation_start + base::Milliseconds(50);

  content::MockNavigationHandle handle(GURL(kTestWebUIUrl), main_rfh());
  EXPECT_CALL(handle, GetNavigationHandleTiming())
      .WillRepeatedly(ReturnRef(timing));
  EXPECT_CALL(handle, IsSameProcess()).WillRepeatedly(::testing::Return(false));

  observer_->OnCommit(&handle);

  DeleteContents();

  auto entries =
      test_ukm_recorder_.GetEntriesByName("InitialWebUINavigationTiming");
  EXPECT_THAT(entries, IsEmpty());
}

TEST_F(InitialWebUIPageLoadMetricsObserverTest,
       NavigationTiming_FirstRequestNull) {
  NavigateAndCommit(GURL(kTestWebUIUrl));

  base::TimeTicks navigation_start =
      tester()->GetDelegateForCommittedLoad().GetNavigationStart();

  content::NavigationHandleTiming timing;
  // `first_request_start_time` is null.
  timing.first_response_start_time = navigation_start + base::Milliseconds(20);
  timing.navigation_commit_sent_time =
      navigation_start + base::Milliseconds(25);
  timing.navigation_commit_received_time =
      navigation_start + base::Milliseconds(30);
  timing.navigation_commit_reply_sent_time =
      navigation_start + base::Milliseconds(40);
  timing.navigation_did_commit_time = navigation_start + base::Milliseconds(50);

  content::MockNavigationHandle handle(GURL(kTestWebUIUrl), main_rfh());
  EXPECT_CALL(handle, GetNavigationHandleTiming())
      .WillRepeatedly(ReturnRef(timing));
  EXPECT_CALL(handle, IsSameProcess()).WillRepeatedly(::testing::Return(false));

  observer_->OnCommit(&handle);

  DeleteContents();

  auto entries =
      test_ukm_recorder_.GetEntriesByName("InitialWebUINavigationTiming");
  EXPECT_THAT(entries, IsEmpty());
}

TEST_F(InitialWebUIPageLoadMetricsObserverTest,
       NavigationTiming_DidCommitNull) {
  NavigateAndCommit(GURL(kTestWebUIUrl));

  base::TimeTicks navigation_start =
      tester()->GetDelegateForCommittedLoad().GetNavigationStart();

  content::NavigationHandleTiming timing;
  timing.first_request_start_time = navigation_start + base::Milliseconds(10);
  timing.first_response_start_time = navigation_start + base::Milliseconds(20);
  timing.navigation_commit_sent_time =
      navigation_start + base::Milliseconds(25);
  timing.navigation_commit_received_time =
      navigation_start + base::Milliseconds(30);
  timing.navigation_commit_reply_sent_time =
      navigation_start + base::Milliseconds(40);
  // `navigation_did_commit_time` is null.

  content::MockNavigationHandle handle(GURL(kTestWebUIUrl), main_rfh());
  EXPECT_CALL(handle, GetNavigationHandleTiming())
      .WillRepeatedly(ReturnRef(timing));
  EXPECT_CALL(handle, IsSameProcess()).WillRepeatedly(::testing::Return(false));

  observer_->OnCommit(&handle);

  DeleteContents();

  auto entries =
      test_ukm_recorder_.GetEntriesByName("InitialWebUINavigationTiming");
  EXPECT_THAT(entries, IsEmpty());
}

}  // namespace
