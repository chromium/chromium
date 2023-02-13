// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/diagnostics_metrics_message_handler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::diagnostics::metrics {
namespace {

const base::TimeDelta kDefaultTimeDelta = base::Minutes(1);

// Handler names:
const char kRecordNavigation[] = "recordNavigation";

// Metrics:
const char kConnectivityOpenDurationMetric[] =
    "ChromeOS.DiagnosticsUi.Connectivity.OpenDuration";
const char kInputOpenDurationMetric[] =
    "ChromeOS.DiagnosticsUi.Input.OpenDuration";
const char kSystemOpenDurationMetric[] =
    "ChromeOS.DiagnosticsUi.System.OpenDuration";

class DiagnosticsMetricsMessageHandlerTest : public testing::Test {
 public:
  DiagnosticsMetricsMessageHandlerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        web_ui_(),
        histograms() {}

  ~DiagnosticsMetricsMessageHandlerTest() override = default;

  void InitializeHandler(NavigationView view) {
    handler_ = std::make_unique<DiagnosticsMetricsMessageHandler>(view);
    handler_->SetWebUiForTesting(&web_ui_);
    handler_->RegisterMessages();
  }

  void SendRecordNavigation(NavigationView from, NavigationView to) {
    base::Value::List args;
    args.Append(static_cast<int>(from));
    args.Append(static_cast<int>(to));
    web_ui_.HandleReceivedMessage(kRecordNavigation, args);

    task_environment_.RunUntilIdle();
  }

 protected:
  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  base::test::TaskEnvironment task_environment_;
  content::TestWebUI web_ui_;
  std::unique_ptr<DiagnosticsMetricsMessageHandler> handler_;
  base::HistogramTester histograms;
};
}  // namespace

TEST_F(DiagnosticsMetricsMessageHandlerTest, AbleToRegisterMessages) {
  EXPECT_NO_FATAL_FAILURE(InitializeHandler(NavigationView::kInput));
}

TEST_F(DiagnosticsMetricsMessageHandlerTest, InitializedCorrectly) {
  NavigationView expected_view = NavigationView::kSystem;
  InitializeHandler(expected_view);
  AdvanceClock(kDefaultTimeDelta);

  EXPECT_EQ(expected_view, handler_->GetCurrentViewForTesting());
  EXPECT_EQ(kDefaultTimeDelta,
            handler_->GetElapsedNavigationTimeDeltaForTesting());
}

TEST_F(DiagnosticsMetricsMessageHandlerTest, HandleNoRecordNavigationMessage) {
  NavigationView expected_initial_view = NavigationView::kSystem;
  InitializeHandler(expected_initial_view);
  AdvanceClock(kDefaultTimeDelta);

  histograms.ExpectTotalCount(
      ash::diagnostics::metrics::kConnectivityOpenDurationMetric,
      /** count= */ 0);
  histograms.ExpectTotalCount(
      ash::diagnostics::metrics::kConnectivityOpenDurationMetric,
      /** count= */ 0);
  histograms.ExpectTotalCount(
      ash::diagnostics::metrics::kInputOpenDurationMetric, /** count= */ 0);
  EXPECT_EQ(expected_initial_view, handler_->GetCurrentViewForTesting());
  EXPECT_EQ(kDefaultTimeDelta,
            handler_->GetElapsedNavigationTimeDeltaForTesting());
  // Application shutdown should trigger metrics.
  handler_.reset();
  task_environment_.RunUntilIdle();
  histograms.ExpectTimeBucketCount(kSystemOpenDurationMetric,
                                   /** elapsed_time */ kDefaultTimeDelta,
                                   /** count= */ 1);

  histograms.ExpectTotalCount(
      ash::diagnostics::metrics::kSystemOpenDurationMetric, /** count= */ 1);
  histograms.ExpectTotalCount(
      ash::diagnostics::metrics::kConnectivityOpenDurationMetric,
      /** count= */ 0);
  histograms.ExpectTotalCount(
      ash::diagnostics::metrics::kInputOpenDurationMetric, /** count= */ 0);
}

TEST_F(DiagnosticsMetricsMessageHandlerTest, HandleRecordNavigationMessage) {
  NavigationView expected_initial_view = NavigationView::kSystem;
  InitializeHandler(expected_initial_view);
  EXPECT_EQ(expected_initial_view, handler_->GetCurrentViewForTesting());

  AdvanceClock(kDefaultTimeDelta);
  SendRecordNavigation(expected_initial_view, NavigationView::kConnectivity);

  EXPECT_EQ(NavigationView::kConnectivity,
            handler_->GetCurrentViewForTesting());
  EXPECT_EQ(base::Minutes(0),
            handler_->GetElapsedNavigationTimeDeltaForTesting());
}

TEST_F(DiagnosticsMetricsMessageHandlerTest,
       HandleRecordNavigationMultipleEvents) {
  NavigationView initial_view = NavigationView::kConnectivity;
  InitializeHandler(initial_view);
  AdvanceClock(kDefaultTimeDelta);

  histograms.ExpectTotalCount(kSystemOpenDurationMetric, /** count= */ 0);
  histograms.ExpectTotalCount(kConnectivityOpenDurationMetric, /** count= */ 0);
  histograms.ExpectTotalCount(kInputOpenDurationMetric, /** count= */ 0);
  EXPECT_EQ(NavigationView::kConnectivity,
            handler_->GetCurrentViewForTesting());
  EXPECT_EQ(kDefaultTimeDelta,
            handler_->GetElapsedNavigationTimeDeltaForTesting());

  // Subsequent record navigations will trigger metric recording.
  SendRecordNavigation(NavigationView::kConnectivity, NavigationView::kInput);
  AdvanceClock(kDefaultTimeDelta);

  histograms.ExpectTimeBucketCount(kConnectivityOpenDurationMetric,
                                   /** elapsed_time */ kDefaultTimeDelta,
                                   /** count= */ 1);
  EXPECT_EQ(NavigationView::kInput, handler_->GetCurrentViewForTesting());
  EXPECT_EQ(kDefaultTimeDelta,
            handler_->GetElapsedNavigationTimeDeltaForTesting());

  SendRecordNavigation(NavigationView::kInput, NavigationView::kSystem);
  AdvanceClock(kDefaultTimeDelta);

  histograms.ExpectTimeBucketCount(kInputOpenDurationMetric,
                                   /** elapsed_time */ kDefaultTimeDelta,
                                   /** count= */ 1);
  EXPECT_EQ(NavigationView::kSystem, handler_->GetCurrentViewForTesting());
  EXPECT_EQ(kDefaultTimeDelta,
            handler_->GetElapsedNavigationTimeDeltaForTesting());

  // Navigating to a screen you viewed before also updates metrics.
  SendRecordNavigation(NavigationView::kSystem, NavigationView::kConnectivity);
  AdvanceClock(kDefaultTimeDelta);

  histograms.ExpectTimeBucketCount(kSystemOpenDurationMetric,
                                   /** elapsed_time */ kDefaultTimeDelta,
                                   /** count= */ 1);
  EXPECT_EQ(NavigationView::kConnectivity,
            handler_->GetCurrentViewForTesting());
  EXPECT_EQ(kDefaultTimeDelta,
            handler_->GetElapsedNavigationTimeDeltaForTesting());

  // Application shutdown should trigger metrics.
  AdvanceClock(kDefaultTimeDelta);
  handler_.reset();
  histograms.ExpectTimeBucketCount(kConnectivityOpenDurationMetric,
                                   /** elapsed_time */ kDefaultTimeDelta,
                                   /** count= */ 1);

  histograms.ExpectTotalCount(kSystemOpenDurationMetric, /** count= */ 1);
  histograms.ExpectTotalCount(kConnectivityOpenDurationMetric, /** count= */ 2);
  histograms.ExpectTotalCount(kInputOpenDurationMetric, /** count= */ 1);
}

TEST_F(DiagnosticsMetricsMessageHandlerTest,
       HandleRecordNavigationWithoutArgs) {
  base::Value::List args;

  NavigationView expected_view = NavigationView::kSystem;
  InitializeHandler(expected_view);

  EXPECT_NO_FATAL_FAILURE(
      web_ui_.HandleReceivedMessage(kRecordNavigation, args));
  EXPECT_EQ(expected_view, handler_->GetCurrentViewForTesting());
}

TEST_F(DiagnosticsMetricsMessageHandlerTest, HandleRecordNavigationWithOneArg) {
  base::Value::List args;
  args.Append(0);

  NavigationView expected_view = NavigationView::kSystem;
  InitializeHandler(expected_view);

  EXPECT_NO_FATAL_FAILURE(
      web_ui_.HandleReceivedMessage(kRecordNavigation, args));
  EXPECT_EQ(expected_view, handler_->GetCurrentViewForTesting());
}

TEST_F(DiagnosticsMetricsMessageHandlerTest,
       HandleRecordNavigationWithInvalidArgs) {
  base::Value::List args;
  args.Append("0");
  args.Append(base::Value());

  NavigationView expected_view = NavigationView::kSystem;
  InitializeHandler(expected_view);

  EXPECT_NO_FATAL_FAILURE(
      web_ui_.HandleReceivedMessage(kRecordNavigation, args));
  EXPECT_EQ(expected_view, handler_->GetCurrentViewForTesting());
}

TEST_F(DiagnosticsMetricsMessageHandlerTest,
       HandleRecordNavigationWithMatchingArgs) {
  base::Value::List args;
  args.Append(1);
  args.Append(1);

  NavigationView expected_view = NavigationView::kSystem;
  InitializeHandler(expected_view);

  EXPECT_NO_FATAL_FAILURE(
      web_ui_.HandleReceivedMessage(kRecordNavigation, args));
  EXPECT_EQ(expected_view, handler_->GetCurrentViewForTesting());
}

TEST_F(DiagnosticsMetricsMessageHandlerTest,
       HandleRecordNavigationWithOutOfRangeArgs) {
  base::Value::List args;
  args.Append(-100);
  args.Append(100);

  NavigationView expected_view = NavigationView::kSystem;
  InitializeHandler(expected_view);

  EXPECT_NO_FATAL_FAILURE(
      web_ui_.HandleReceivedMessage(kRecordNavigation, args));
  EXPECT_EQ(expected_view, handler_->GetCurrentViewForTesting());
}
}  // namespace ash::diagnostics::metrics
