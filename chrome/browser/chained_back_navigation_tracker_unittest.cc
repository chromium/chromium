// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chained_back_navigation_tracker.h"

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"

class ChainedBackNavigationTrackerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChainedBackNavigationTrackerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  std::vector<GURL> test_urls() {
    std::vector<GURL> urls;
    for (uint32_t i = 0; i < min_navigation_cnt_ * 2; ++i) {
      urls.push_back(GURL("http://foo/" + base::NumberToString(i)));
    }
    return urls;
  }

  const uint32_t min_navigation_cnt_ =
      ChainedBackNavigationTracker::kMinimumChainedBackNavigationLength;
  const int64_t max_navigation_interval_ = ChainedBackNavigationTracker::
      kMaxChainedBackNavigationIntervalInMilliseconds;
};

TEST_F(ChainedBackNavigationTrackerTest, ChainedBackNavigationStatus) {
  const std::vector<GURL> urls = test_urls();
  for (const GURL& url : urls) {
    NavigateAndCommit(url);
  }

  ChainedBackNavigationTracker::CreateForWebContents(web_contents());
  const ChainedBackNavigationTracker* tracker =
      ChainedBackNavigationTracker::FromWebContents(web_contents());
  ASSERT_TRUE(tracker);

  // Before any back navigation, the return value for these two checker
  // functions should be false.
  ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
  ASSERT_FALSE(tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());

  for (uint32_t i = 1; i < urls.size(); ++i) {
    content::NavigationSimulator::GoBack(web_contents());
    // Since `RecordBackButtonClickForChainedBackNavigation()` is never called,
    // `IsBackButtonChainedBackNavigationRecentlyPerformed()` should always
    // returns false.
    ASSERT_FALSE(tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());
    // The check should only return true when the number of consecutive back
    // navigation is smaller than `kMinimumChainedBackNavigationLength`.
    if (i < min_navigation_cnt_) {
      ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
    } else {
      ASSERT_TRUE(tracker->IsChainedBackNavigationRecentlyPerformed());
    }
  }
}

TEST_F(ChainedBackNavigationTrackerTest,
       ChainedBackNavigationStatus_ResetCountIfIntervalIsTooLong) {
  const std::vector<GURL> urls = test_urls();
  for (const GURL& url : urls) {
    NavigateAndCommit(url);
  }

  ChainedBackNavigationTracker::CreateForWebContents(web_contents());
  const ChainedBackNavigationTracker* tracker =
      ChainedBackNavigationTracker::FromWebContents(web_contents());
  ASSERT_TRUE(tracker);

  // Before any back navigation, the return value for these two checker
  // functions should be false.
  ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
  ASSERT_FALSE(tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());

  for (uint32_t i = 1; i < min_navigation_cnt_; ++i) {
    content::NavigationSimulator::GoBack(web_contents());
    // The checks should always return false since the number of consecutive
    // back navigation is smaller than `kMinimumChainedBackNavigationLength`.
    ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
    ASSERT_FALSE(tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());
  }

  // After waiting for sufficiently long interval, the counter should be reset
  // so the checks should always return false.
  task_environment()->FastForwardBy(
      base::Milliseconds(max_navigation_interval_ * 2));
  ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
  ASSERT_FALSE(tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());

  for (uint32_t i = 1; i < min_navigation_cnt_; ++i) {
    content::NavigationSimulator::GoBack(web_contents());
    ASSERT_FALSE(tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());
    ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
  }
}

TEST_F(
    ChainedBackNavigationTrackerTest,
    ChainedBackNavigationStatus_ResetCountIfNonBackForwardNavigationHappens) {
  const std::vector<GURL> urls = test_urls();
  for (const GURL& url : urls) {
    NavigateAndCommit(url);
  }

  ChainedBackNavigationTracker::CreateForWebContents(web_contents());
  const ChainedBackNavigationTracker* tracker =
      ChainedBackNavigationTracker::FromWebContents(web_contents());
  ASSERT_TRUE(tracker);

  // Before any back navigation, the return value for these two checker
  // functions should be false.
  ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
  ASSERT_FALSE(tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());

  for (uint32_t i = 1; i < min_navigation_cnt_; ++i) {
    content::NavigationSimulator::GoBack(web_contents());
    ASSERT_FALSE(tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());
    ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
  }

  // After performing another non history navigation, the counter should be
  // reset so the checks should always return false.
  NavigateAndCommit(GURL("http://bar/1"));
  ASSERT_EQ(0u, tracker->chained_back_navigation_count_);

  for (uint32_t i = 1; i < min_navigation_cnt_; ++i) {
    content::NavigationSimulator::GoBack(web_contents());
    ASSERT_FALSE(tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());
    ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
  }
}

TEST_F(ChainedBackNavigationTrackerTest,
       ChainedBackNavigationStatus_BackButtonClicked) {
  const std::vector<GURL> urls = test_urls();
  for (const GURL& url : urls) {
    NavigateAndCommit(url);
  }

  ChainedBackNavigationTracker::CreateForWebContents(web_contents());
  ChainedBackNavigationTracker* tracker =
      ChainedBackNavigationTracker::FromWebContents(web_contents());
  ASSERT_TRUE(tracker);

  // Before any back navigation, the return value for these two checker
  // functions should be false.
  ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
  ASSERT_FALSE(tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());

  for (uint32_t i = 1; i < urls.size(); ++i) {
    tracker->RecordBackButtonClickForChainedBackNavigation();
    content::NavigationSimulator::GoBack(web_contents());
    // The checks should only return true when the number of consecutive back
    // navigation is greater than or equal to
    // `kMinimumChainedBackNavigationLength`.
    if (i >= min_navigation_cnt_) {
      ASSERT_TRUE(tracker->IsChainedBackNavigationRecentlyPerformed());
      ASSERT_TRUE(
          tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());
    } else {
      ASSERT_FALSE(tracker->IsChainedBackNavigationRecentlyPerformed());
      ASSERT_FALSE(
          tracker->IsBackButtonChainedBackNavigationRecentlyPerformed());
    }
  }
}
