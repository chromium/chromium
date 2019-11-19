// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_memory_metrics_reporter.h"

#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using WebContents = content::WebContents;

namespace resource_coordinator {

using testing::_;
using testing::StrictMock;
using LoadingState = resource_coordinator::TabLoadTracker::LoadingState;

class TestTabMemoryMetricsReporter : public TabMemoryMetricsReporter {
 public:
  using TabMemoryMetricsReporter::OnLoadingStateChange;
  using TabMemoryMetricsReporter::OnStartTracking;
  using TabMemoryMetricsReporter::OnStopTracking;

  TestTabMemoryMetricsReporter() = delete;

  explicit TestTabMemoryMetricsReporter(const base::TickClock* tick_clock)
      : TabMemoryMetricsReporter(tick_clock), emit_count_(0) {}

  const content::WebContents* TopMonitoredContent() const {
    if (monitored_contents_.empty())
      return nullptr;
    return monitored_contents_.cbegin()->web_contents;
  }

  base::TimeDelta NextEmitTimeFromNow() const {
    if (monitored_contents_.empty())
      return base::TimeDelta();
    return monitored_contents_.cbegin()->next_emit_time - NowTicks();
  }

  void InstallTaskRunner(scoped_refptr<base::SequencedTaskRunner> task_runner) {
    update_timer_.SetTaskRunner(task_runner);
  }

  void DiscardContent(content::WebContents* content) {
    discarded_contents_.insert(content);
  }

  bool EmitMemoryMetricsAfterPageLoaded(
      const TabMemoryMetricsReporter::WebContentsData& content) override {
    if (discarded_contents_.find(content.web_contents) !=
        discarded_contents_.cend())
      return false;
    ++emit_count_;
    return true;
  }

  base::OneShotTimer& update_timer_for_testing() { return update_timer_; }
  unsigned emit_count() const { return emit_count_; }

 private:
  unsigned emit_count_;
  std::unordered_set<content::WebContents*> discarded_contents_;
};

class TabMemoryMetricsReporterTest : public testing::Test {
 public:
  TabMemoryMetricsReporterTest()
      : task_runner_(new base::TestMockTimeTaskRunner()) {
    observer_.reset(
        new TestTabMemoryMetricsReporter(task_runner_->GetMockTickClock()));
    observer_->InstallTaskRunner(task_runner_);
  }

  void SetUp() override {
    test_web_contents_factory_.reset(new content::TestWebContentsFactory);

    contents1_ =
        test_web_contents_factory_->CreateWebContents(&testing_profile_);
    contents2_ =
        test_web_contents_factory_->CreateWebContents(&testing_profile_);
    contents3_ =
        test_web_contents_factory_->CreateWebContents(&testing_profile_);
  }

  void TearDown() override { test_web_contents_factory_.reset(); }

  TestTabMemoryMetricsReporter& observer() { return *observer_; }
  const base::TickClock* tick_clock() {
    return task_runner_->GetMockTickClock();
  }
  base::TestMockTimeTaskRunner* task_runner() { return task_runner_.get(); }

  content::WebContents* contents1() { return contents1_; }
  content::WebContents* contents2() { return contents2_; }
  content::WebContents* contents3() { return contents3_; }

 private:
  std::unique_ptr<TestTabMemoryMetricsReporter> observer_;

  // Required for asynchronous calculations.
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  std::unique_ptr<content::TestWebContentsFactory> test_web_contents_factory_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
  content::WebContents* contents1_;
  content::WebContents* contents2_;
  content::WebContents* contents3_;
};

TEST_F(TabMemoryMetricsReporterTest, StartTrackingWithUnloaded) {
  observer().OnStartTracking(contents1(), LoadingState::UNLOADED);
  EXPECT_FALSE(observer().update_timer_for_testing().IsRunning());
  EXPECT_FALSE(observer().TopMonitoredContent());
}

TEST_F(TabMemoryMetricsReporterTest, StartTrackingWithLoading) {
  observer().OnStartTracking(contents1(), LoadingState::LOADING);
  EXPECT_FALSE(observer().update_timer_for_testing().IsRunning());
  EXPECT_FALSE(observer().TopMonitoredContent());
}

TEST_F(TabMemoryMetricsReporterTest, StartTrackingWithLoaded) {
  observer().OnStartTracking(contents1(), LoadingState::LOADED);
  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());
}

TEST_F(TabMemoryMetricsReporterTest, OnLoadingStateChangeWithUnloaded) {
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::UNLOADED);
  EXPECT_FALSE(observer().update_timer_for_testing().IsRunning());
  EXPECT_FALSE(observer().TopMonitoredContent());
}

TEST_F(TabMemoryMetricsReporterTest, OnLoadingStateChangeWithLoading) {
  observer().OnLoadingStateChange(contents1(), LoadingState::UNLOADED,
                                  LoadingState::LOADING);
  EXPECT_FALSE(observer().update_timer_for_testing().IsRunning());
  EXPECT_FALSE(observer().TopMonitoredContent());
}

TEST_F(TabMemoryMetricsReporterTest, OnLoadingStateChangeWithLoaded) {
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());
}

TEST_F(TabMemoryMetricsReporterTest, OnStopTracking) {
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());
  observer().OnStopTracking(contents1(), LoadingState::UNLOADED);
  EXPECT_FALSE(observer().update_timer_for_testing().IsRunning());
  EXPECT_FALSE(observer().TopMonitoredContent());
}

TEST_F(TabMemoryMetricsReporterTest, TrackingThreeWithLoaded) {
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(tick_clock());
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->AdvanceMockTickClock(base::TimeDelta::FromMinutes(1));
  observer().OnLoadingStateChange(contents2(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->AdvanceMockTickClock(base::TimeDelta::FromMinutes(1));
  observer().OnLoadingStateChange(contents3(), LoadingState::LOADING,
                                  LoadingState::LOADED);

  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());

  observer().OnStopTracking(contents2(), LoadingState::UNLOADED);
  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());

  observer().OnStopTracking(contents1(), LoadingState::UNLOADED);
  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents3(), observer().TopMonitoredContent());

  observer().OnStopTracking(contents3(), LoadingState::UNLOADED);
  EXPECT_FALSE(observer().update_timer_for_testing().IsRunning());
  EXPECT_FALSE(observer().TopMonitoredContent());
}

TEST_F(TabMemoryMetricsReporterTest, EmitMemoryDumpAfterOneMinute) {
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(tick_clock());
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1U, observer().emit_count());
  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());
  EXPECT_EQ(4, observer().NextEmitTimeFromNow().InMinutes());
}

TEST_F(TabMemoryMetricsReporterTest, EmitMemoryDumpAfterFiveMinutes) {
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(tick_clock());
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->FastForwardBy(base::TimeDelta::FromMinutes(5));
  EXPECT_EQ(2U, observer().emit_count());
  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());
  EXPECT_EQ(5, observer().NextEmitTimeFromNow().InMinutes());
}

TEST_F(TabMemoryMetricsReporterTest, EmitMemoryDumpAfterTenMinutes) {
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(tick_clock());
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->FastForwardBy(base::TimeDelta::FromMinutes(10));
  EXPECT_EQ(3U, observer().emit_count());
  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());
  EXPECT_EQ(5, observer().NextEmitTimeFromNow().InMinutes());
}

TEST_F(TabMemoryMetricsReporterTest, EmitMemoryDumpAfterFifteenMinutes) {
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(tick_clock());
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->FastForwardBy(base::TimeDelta::FromMinutes(15));
  EXPECT_EQ(4U, observer().emit_count());
  EXPECT_FALSE(observer().update_timer_for_testing().IsRunning());
  EXPECT_FALSE(observer().TopMonitoredContent());
}

TEST_F(TabMemoryMetricsReporterTest, EmitMemoryDumpSkipFiveMinutes) {
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(tick_clock());
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->AdvanceMockTickClock(base::TimeDelta::FromMinutes(5));
  task_runner()->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1U, observer().emit_count());
  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());
  EXPECT_EQ(4, observer().NextEmitTimeFromNow().InMinutes());
}

TEST_F(TabMemoryMetricsReporterTest, EmitMemoryDumpSkipTenMinutes) {
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(tick_clock());
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->AdvanceMockTickClock(base::TimeDelta::FromMinutes(10));
  task_runner()->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1U, observer().emit_count());
  EXPECT_TRUE(observer().update_timer_for_testing().IsRunning());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());
  EXPECT_EQ(4, observer().NextEmitTimeFromNow().InMinutes());
}

TEST_F(TabMemoryMetricsReporterTest, EmitMemoryDumpSkipFifteenMinutes) {
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(tick_clock());
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->AdvanceMockTickClock(base::TimeDelta::FromMinutes(15));
  task_runner()->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1U, observer().emit_count());
  EXPECT_FALSE(observer().update_timer_for_testing().IsRunning());
  EXPECT_FALSE(observer().TopMonitoredContent());
}

TEST_F(TabMemoryMetricsReporterTest, SecondContentComeAfter9_5Minutes) {
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(tick_clock());
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->FastForwardBy(base::TimeDelta::FromMinutes(9) +
                               base::TimeDelta::FromSeconds(30));
  EXPECT_EQ(2U, observer().emit_count());
  EXPECT_EQ(30, observer().NextEmitTimeFromNow().InSeconds());
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());

  observer().OnLoadingStateChange(contents2(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  EXPECT_EQ(contents1(), observer().TopMonitoredContent());
  EXPECT_EQ(30, observer().NextEmitTimeFromNow().InSeconds());
}

TEST_F(TabMemoryMetricsReporterTest, EmitMemoryDumpForDiscardedContent) {
  ScopedSetTickClockForTesting scoped_set_tick_clock_for_testing_(tick_clock());
  observer().OnLoadingStateChange(contents1(), LoadingState::LOADING,
                                  LoadingState::LOADED);
  task_runner()->FastForwardBy(base::TimeDelta::FromMinutes(1));
  EXPECT_EQ(1U, observer().emit_count());
  observer().DiscardContent(contents1());
  task_runner()->FastForwardBy(base::TimeDelta::FromMinutes(4));
  EXPECT_EQ(1U, observer().emit_count());
  EXPECT_FALSE(observer().update_timer_for_testing().IsRunning());
  EXPECT_FALSE(observer().TopMonitoredContent());
}

}  // namespace resource_coordinator
