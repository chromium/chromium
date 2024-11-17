// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHAINED_BACK_NAVIGATION_TRACKER_H_
#define CHROME_BROWSER_CHAINED_BACK_NAVIGATION_TRACKER_H_

#include "base/gtest_prod_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

// This class tracks chained back navigations (consecutive back navigations with
// a short interval between them) by observing navigation events from
// WebContents and providing functions to record back button clicks.
class ChainedBackNavigationTracker
    : public content::WebContentsObserver,
      public content::WebContentsUserData<ChainedBackNavigationTracker> {
 public:
  ~ChainedBackNavigationTracker() override;

  ChainedBackNavigationTracker(const ChainedBackNavigationTracker&) = delete;
  ChainedBackNavigationTracker& operator=(const ChainedBackNavigationTracker&) =
      delete;

  // content::WebContentsObserver
  void DidStartNavigation(content::NavigationHandle* navigation) override;

  // Notifies the `ChainedBackNavigationTracker` that a back/forward button is
  // clicked. This will be used to determined if a chained back navigation is
  // performed by back button.
  void RecordBackButtonClickForChainedBackNavigation();

  // The two functions below should be called to determine if the relevant user
  // education promotion (such as the one for back navigation menu) should be
  // displayed according to the corresponding trigger condition.
  // Returns if a chained back/forward navigation is performed no earlier that
  // the `kMaxChainedBackNavigationIntervalInMilliseconds` ago.
  bool IsChainedBackNavigationRecentlyPerformed() const;
  // Returns if a chained back button click events that caused chained back
  // navigation is performed no earlier than the
  // `kMaxChainedBackNavigationIntervalInMilliseconds` ago.
  bool IsBackButtonChainedBackNavigationRecentlyPerformed() const;

  // The threshold for two back/forward navigation to be considered chained.
  static const int64_t kMaxChainedBackNavigationIntervalInMilliseconds = 3000;
  // The minimum number of back/forward navigations in a chain for the
  // `ChainedBackNavigationTracker` to claim that a chained back navigation is
  // performed.
  static const uint32_t kMinimumChainedBackNavigationLength = 3u;

 private:
  friend class content::WebContentsUserData<ChainedBackNavigationTracker>;

  FRIEND_TEST_ALL_PREFIXES(
      ChainedBackNavigationTrackerTest,
      ChainedBackNavigationStatus_ResetCountIfNonBackForwardNavigationHappens);
  FRIEND_TEST_ALL_PREFIXES(ChainedBackNavigationTrackerBrowserTest,
                           SubframeBackNavigationIsCountedAsChained);
  FRIEND_TEST_ALL_PREFIXES(
      ChainedBackNavigationTrackerBrowserTest,
      RendererInitiatedBackNavigationIsNotCountedAsChained);

  explicit ChainedBackNavigationTracker(content::WebContents* contents);

  // Helper functions that modify the `last_back_navigation_time_` and
  // `chained_back_navigation_count_`.
  // The chained back navigation count should increment when a back/forward
  // navigation is performed and the interval between the current time tick and
  // the last recorded time tick is smaller than the threshold.
  void IncrementChainedBackNavigationCount();
  // The chained back navigation count should be reset to 0 if a
  // non-back/forward navigation is performed.
  void ResetChainedBackNavigationCount();

  // Chained back navigation variables that are used to record the number of
  // consecutive back navigation and back button click events with interval
  // shorter than some threshold.
  uint32_t chained_back_navigation_count_ = 0;
  uint32_t chained_back_button_click_count_ = 0;
  base::TimeTicks last_back_navigation_time_;
  base::TimeTicks last_back_button_click_time_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_CHAINED_BACK_NAVIGATION_TRACKER_H_
