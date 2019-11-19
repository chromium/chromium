// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_session.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_test_helper.h"
#include "chrome/browser/chromeos/arc/tracing/arc_app_performance_tracing_uma_session.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

constexpr char kFocusAppPackage[] = "focus.app.package";
constexpr char kFocusAppActivity[] = "focus.app.package.Activity";
constexpr char kFocusCategory[] = "OnlineGame";
constexpr char kNonFocusAppPackage[] = "nonfocus.app.package";
constexpr char kNonFocusAppActivity[] = "nonfocus.app.package.Activity";

// For 20 frames.
constexpr base::TimeDelta kTestPeriod =
    base::TimeDelta::FromSeconds(1) / (60 / 20);

// Creates name of histogram with required statistics.
std::string GetFocusStatisticName(const std::string& name) {
  return base::StringPrintf("Arc.Runtime.Performance.%s.%s", name.c_str(),
                            kFocusCategory);
}

// Reads statistics value from histogram.
int64_t ReadFocusStatistics(const std::string& name) {
  const base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(GetFocusStatisticName(name));
  DCHECK(histogram);

  std::unique_ptr<base::HistogramSamples> samples =
      histogram->SnapshotFinalDelta();
  DCHECK(samples.get());
  DCHECK_EQ(1, samples->TotalCount());
  return samples->sum();
}

}  // namespace

// BrowserWithTestWindowTest contains required ash/shell support that would not
// be possible to use directly.
class ArcAppPerformanceTracingTest : public BrowserWithTestWindowTest {
 public:
  ArcAppPerformanceTracingTest() = default;
  ~ArcAppPerformanceTracingTest() override = default;

  // testing::Test:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    arc_test_.SetUp(profile());
    tracing_helper_.SetUp(profile());

    ArcAppPerformanceTracing::SetFocusAppForTesting(
        kFocusAppPackage, kFocusAppActivity, kFocusCategory);

    ArcAppPerformanceTracingUmaSession::SetTracingPeriodForTesting(kTestPeriod);
  }

  void TearDown() override {
    tracing_helper_.TearDown();
    arc_test_.TearDown();
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  // Ensures that tracing is active.
  views::Widget* StartArcFocusAppTracing() {
    views::Widget* const arc_widget =
        ArcAppPerformanceTracingTestHelper::CreateArcWindow(
            "org.chromium.arc.1");
    DCHECK(arc_widget && arc_widget->GetNativeWindow());
    tracing_helper().GetTracing()->OnWindowActivated(
        wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
        arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
    tracing_helper().GetTracing()->OnTaskCreated(
        1 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
        std::string() /* intent */);
    DCHECK(tracing_helper().GetTracingSession());
    tracing_helper().GetTracingSession()->FireTimerForTesting();
    DCHECK(tracing_helper().GetTracingSession());
    DCHECK(tracing_helper().GetTracingSession()->tracing_active());
    return arc_widget;
  }

  ArcAppPerformanceTracingTestHelper& tracing_helper() {
    return tracing_helper_;
  }

 private:
  ArcAppPerformanceTracingTestHelper tracing_helper_;
  ArcAppTest arc_test_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppPerformanceTracingTest);
};

TEST_F(ArcAppPerformanceTracingTest, TracingScheduled) {
  // By default it is inactive.
  EXPECT_FALSE(tracing_helper().GetTracingSession());

  // Report task first.
  tracing_helper().GetTracing()->OnTaskCreated(
      1 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
      std::string() /* intent */);
  EXPECT_FALSE(tracing_helper().GetTracingSession());

  // Create window second.
  views::Widget* const arc_widget1 =
      ArcAppPerformanceTracingTestHelper::CreateArcWindow("org.chromium.arc.1");
  ASSERT_TRUE(arc_widget1);
  ASSERT_TRUE(arc_widget1->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget1->GetNativeWindow(), nullptr /* lost_active */);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  // Scheduled but not started.
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());

  // Test reverse order, create window first.
  views::Widget* const arc_widget2 =
      ArcAppPerformanceTracingTestHelper::CreateArcWindow("org.chromium.arc.2");
  ASSERT_TRUE(arc_widget2);
  ASSERT_TRUE(arc_widget2->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget2->GetNativeWindow(), arc_widget2->GetNativeWindow());
  // Task is not yet created, this also resets previous tracing.
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  // Report task second.
  tracing_helper().GetTracing()->OnTaskCreated(
      2 /* task_Id */, kFocusAppPackage, kFocusAppActivity,
      std::string() /* intent */);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  // Scheduled but not started.
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  arc_widget1->Close();
  arc_widget2->Close();
}

TEST_F(ArcAppPerformanceTracingTest, TracingNotScheduledForNonFocusApp) {
  views::Widget* const arc_widget =
      ArcAppPerformanceTracingTestHelper::CreateArcWindow("org.chromium.arc.1");
  ASSERT_TRUE(arc_widget);
  ASSERT_TRUE(arc_widget->GetNativeWindow());
  tracing_helper().GetTracing()->OnWindowActivated(
      wm::ActivationChangeObserver::ActivationReason::ACTIVATION_CLIENT,
      arc_widget->GetNativeWindow(), arc_widget->GetNativeWindow());
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  tracing_helper().GetTracing()->OnTaskCreated(
      1 /* task_Id */, kNonFocusAppPackage, kNonFocusAppActivity,
      std::string() /* intent */);
  EXPECT_FALSE(tracing_helper().GetTracingSession());
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, TracingStoppedOnIdle) {
  views::Widget* const arc_widget = StartArcFocusAppTracing();
  const base::TimeDelta normal_interval = base::TimeDelta::FromSeconds(1) / 60;
  base::Time timestamp = base::Time::Now();
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  // Expected updates;
  timestamp += normal_interval;
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());

  timestamp += normal_interval * 5;
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_TRUE(tracing_helper().GetTracingSession()->tracing_active());

  // Too long update.
  timestamp += normal_interval * 10;
  tracing_helper().GetTracingSession()->OnCommitForTesting(timestamp);
  // Tracing is rescheduled and no longer active.
  ASSERT_TRUE(tracing_helper().GetTracingSession());
  EXPECT_FALSE(tracing_helper().GetTracingSession()->tracing_active());
  arc_widget->Close();
}

TEST_F(ArcAppPerformanceTracingTest, StatisticsReported) {
  views::Widget* const arc_widget = StartArcFocusAppTracing();
  EXPECT_FALSE(tracing_helper().GetTracing()->WasReported(kFocusCategory));
  tracing_helper().PlayDefaultSequence();
  tracing_helper().FireTimerForTesting();
  EXPECT_TRUE(tracing_helper().GetTracing()->WasReported(kFocusCategory));
  EXPECT_EQ(45L, ReadFocusStatistics("FPS"));
  EXPECT_EQ(216L, ReadFocusStatistics("CommitDeviation"));
  EXPECT_EQ(48L, ReadFocusStatistics("RenderQuality"));
  arc_widget->Close();
}

}  // namespace arc
