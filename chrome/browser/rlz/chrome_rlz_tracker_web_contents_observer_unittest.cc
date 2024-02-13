// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/chrome_rlz_tracker_web_contents_observer.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/rlz/mock_rlz_tracker_delegate.h"
#include "components/rlz/rlz_tracker.h"

using ::testing::_;
using ::testing::Return;

class ChromeRLZTrackerWebContentsObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeRLZTrackerWebContentsObserverTest() {}
  ~ChromeRLZTrackerWebContentsObserverTest() override {}

  rlz::MockRLZTrackerDelegate* delegate() { return delegate_; }

 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    auto delegate = std::make_unique<rlz::MockRLZTrackerDelegate>();
    delegate_ = delegate.get();
    rlz::RLZTracker::SetRlzDelegate(std::move(delegate));
  }

  void TearDown() override {
    rlz::RLZTracker::SetRlzChromeHomePageSearchRecordedForTesting(false);
    delegate_ = nullptr;
    rlz::RLZTracker::ClearRlzDelegateForTesting();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 private:
  raw_ptr<rlz::MockRLZTrackerDelegate> delegate_;
};

TEST_F(ChromeRLZTrackerWebContentsObserverTest, PerformHomepageSearch) {
  EXPECT_CALL(*delegate(), GetBrand(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate(), IsBrandOrganic(_)).WillRepeatedly(Return(false));

  ChromeRLZTrackerWebContentsObserver::CreateForWebContentsIfNeeded(
      web_contents());
  ChromeRLZTrackerWebContentsObserver* observer =
      ChromeRLZTrackerWebContentsObserver::FromWebContents(web_contents());
  EXPECT_TRUE(observer);

  // Search callback is not run for an invalid navigation.
  EXPECT_CALL(*delegate(), RunHomepageSearchCallback()).Times(0);
  NavigateAndCommit(
      GURL("https://www.google.com"),
      static_cast<ui::PageTransition>(ui::PAGE_TRANSITION_LINK |
                                      ui::PAGE_TRANSITION_HOME_PAGE));
  task_environment()->RunUntilIdle();

  // Search callback is run for a valid navigation.
  EXPECT_CALL(*delegate(), RunHomepageSearchCallback());
  NavigateAndCommit(GURL("https://www.google.com/search?q=test"));
  task_environment()->RunUntilIdle();

  // Observer has been removed after a valid search.
  observer =
      ChromeRLZTrackerWebContentsObserver::FromWebContents(web_contents());
  EXPECT_FALSE(observer);
}

TEST_F(ChromeRLZTrackerWebContentsObserverTest,
       NotCreateObserverForEmptyBrand) {
  EXPECT_CALL(*delegate(), GetBrand(_)).WillRepeatedly(Return(false));
  ChromeRLZTrackerWebContentsObserver::CreateForWebContentsIfNeeded(
      web_contents());
  ChromeRLZTrackerWebContentsObserver* observer =
      ChromeRLZTrackerWebContentsObserver::FromWebContents(web_contents());
  EXPECT_FALSE(observer);
}

TEST_F(ChromeRLZTrackerWebContentsObserverTest,
       NotCreateObserverForOrganicBrand) {
  EXPECT_CALL(*delegate(), GetBrand(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate(), IsBrandOrganic(_)).WillRepeatedly(Return(true));

  ChromeRLZTrackerWebContentsObserver::CreateForWebContentsIfNeeded(
      web_contents());
  ChromeRLZTrackerWebContentsObserver* observer =
      ChromeRLZTrackerWebContentsObserver::FromWebContents(web_contents());
  EXPECT_FALSE(observer);
}

TEST_F(ChromeRLZTrackerWebContentsObserverTest,
       NotCreateObserverIfSearchRecorded) {
  EXPECT_CALL(*delegate(), GetBrand(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate(), IsBrandOrganic(_)).WillRepeatedly(Return(false));
  rlz::RLZTracker::SetRlzChromeHomePageSearchRecordedForTesting(true);

  ChromeRLZTrackerWebContentsObserver::CreateForWebContentsIfNeeded(
      web_contents());
  ChromeRLZTrackerWebContentsObserver* observer =
      ChromeRLZTrackerWebContentsObserver::FromWebContents(web_contents());
  EXPECT_FALSE(observer);
}

TEST_F(ChromeRLZTrackerWebContentsObserverTest,
       RemoveObserverOnNavigationIfSearchRecorded) {
  EXPECT_CALL(*delegate(), GetBrand(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*delegate(), IsBrandOrganic(_)).WillRepeatedly(Return(false));

  ChromeRLZTrackerWebContentsObserver::CreateForWebContentsIfNeeded(
      web_contents());
  ChromeRLZTrackerWebContentsObserver* observer =
      ChromeRLZTrackerWebContentsObserver::FromWebContents(web_contents());
  EXPECT_TRUE(observer);

  // Simulate that the search has been performed in other web contents.
  rlz::RLZTracker::SetRlzChromeHomePageSearchRecordedForTesting(true);

  // Navigating the web contents will remove the observer.
  EXPECT_CALL(*delegate(), RunHomepageSearchCallback()).Times(0);
  NavigateAndCommit(GURL("https://www.google.com/search?q=test"));
  task_environment()->RunUntilIdle();
  observer =
      ChromeRLZTrackerWebContentsObserver::FromWebContents(web_contents());
  EXPECT_FALSE(observer);
}
