// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

class ContextualTasksTabVisitTrackerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ContextualTasksTabVisitTrackerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Start from a hidden state to ensure we can test transition to visible.
    web_contents()->WasHidden();

    tracker_owner_ =
        std::make_unique<ContextualTasksTabVisitTracker>(web_contents());
    tracker_ = tracker_owner_.get();

    tracker_->SetClockForTesting(task_environment()->GetMockTickClock());
  }

  void TearDown() override {
    tracker_ = nullptr;
    tracker_owner_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<ContextualTasksTabVisitTracker> tracker_owner_;
  raw_ptr<ContextualTasksTabVisitTracker> tracker_;
};

TEST_F(ContextualTasksTabVisitTrackerTest, InitialState) {
  EXPECT_EQ(tracker_->GetDurationOfCurrentOrLastVisit(), base::TimeDelta());
  EXPECT_EQ(tracker_->GetDurationSinceLastActive(), std::nullopt);
}

TEST_F(ContextualTasksTabVisitTrackerTest, TracksCurrentVisitDuration) {
  // Simulates the tab being shown.
  web_contents()->WasShown();

  task_environment()->FastForwardBy(base::Seconds(10));

  // Should return "live" duration (Now - Start) while visible.
  EXPECT_EQ(tracker_->GetDurationOfCurrentOrLastVisit(), base::Seconds(10));
  // Should return 0 while visible.
  EXPECT_EQ(tracker_->GetDurationSinceLastActive(), base::TimeDelta());
}

TEST_F(ContextualTasksTabVisitTrackerTest, TracksHiddenTabLastVisitDuration) {
  web_contents()->WasShown();
  task_environment()->FastForwardBy(base::Seconds(10));

  // Simulates the tab being hidden.
  web_contents()->WasHidden();

  // Should return the duration of the completed visit.
  EXPECT_EQ(tracker_->GetDurationOfCurrentOrLastVisit(), base::Seconds(10));
  EXPECT_EQ(tracker_->GetDurationSinceLastActive(), base::Seconds(0));

  // Advance time while hidden; the returned duration should remain the same.
  task_environment()->FastForwardBy(base::Seconds(50));
  EXPECT_EQ(tracker_->GetDurationOfCurrentOrLastVisit(), base::Seconds(10));
  EXPECT_EQ(tracker_->GetDurationSinceLastActive(), base::Seconds(50));
}

TEST_F(ContextualTasksTabVisitTrackerTest, MultipleVisits) {
  // Visit 1: 5 seconds.
  web_contents()->WasShown();
  task_environment()->FastForwardBy(base::Seconds(5));
  web_contents()->WasHidden();
  EXPECT_EQ(tracker_->GetDurationOfCurrentOrLastVisit(), base::Seconds(5));
  EXPECT_EQ(tracker_->GetDurationSinceLastActive(), base::Seconds(0));

  task_environment()->FastForwardBy(base::Seconds(10));
  EXPECT_EQ(tracker_->GetDurationSinceLastActive(), base::Seconds(10));

  // Visit 2: 15 seconds.
  web_contents()->WasShown();
  task_environment()->FastForwardBy(base::Seconds(15));
  web_contents()->WasHidden();

  // Should return the duration of the most recent visit (15s).
  EXPECT_EQ(tracker_->GetDurationOfCurrentOrLastVisit(), base::Seconds(15));
  EXPECT_EQ(tracker_->GetDurationSinceLastActive(), base::Seconds(0));
}

}  // namespace contextual_tasks
