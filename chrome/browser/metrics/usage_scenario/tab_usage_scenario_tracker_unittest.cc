// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/tab_usage_scenario_tracker.h"

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

// Inherit from ChromeRenderViewHostTestHarness for access to test profile.
class TabUsageScenarioTrackerTest : public ChromeRenderViewHostTestHarness {
 protected:
  std::unique_ptr<content::WebContents> CreateWebContents() {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    return contents;
  }

  UsageScenarioDataStoreImpl usage_scenario_data_store_;
  TabUsageScenarioTracker tab_usage_scenario_tracker_{
      &usage_scenario_data_store_};
};

}  // namespace

TEST_F(TabUsageScenarioTrackerTest, NewVisibleTabMeansOneVisibleWindow) {
  auto contents = CreateWebContents();
  tab_usage_scenario_tracker_.OnTabAddedForTesting(
      contents.get(), content::Visibility::VISIBLE);

  // Only one WebContent was shown which means only one visible window.
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.GetIntervalDataForTesting();
  ASSERT_EQ(interval_data.max_visible_window_count, 1);
}

TEST_F(TabUsageScenarioTrackerTest, VisibilityUpdateOnVisibleWindowIsNoop) {
  auto contents = CreateWebContents();
  tab_usage_scenario_tracker_.OnTabAddedForTesting(
      contents.get(), content::Visibility::VISIBLE);
  tab_usage_scenario_tracker_.OnTabVisibilityChanged(
      contents.get(), content::Visibility::VISIBLE);

  // Only one WebContent was shown which means only one visible window.
  // The call to OnVisibilityChanged should not create a visible window count
  // higher than the number of windows.
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.GetIntervalDataForTesting();
  ASSERT_EQ(interval_data.max_visible_window_count, 1);
}

TEST_F(TabUsageScenarioTrackerTest, HidingWebContentsMakesWindowInvisible) {
  // WebContents starts out visible.
  auto contents = CreateWebContents();
  tab_usage_scenario_tracker_.OnTabAddedForTesting(
      contents.get(), content::Visibility::VISIBLE);
  tab_usage_scenario_tracker_.OnTabVisibilityChanged(
      contents.get(), content::Visibility::VISIBLE);

  // WebContents is hidden.
  tab_usage_scenario_tracker_.OnTabVisibilityChanged(
      contents.get(), content::Visibility::HIDDEN);

  // Grab the interval data.
  UsageScenarioDataStore::IntervalData interval_data =
      usage_scenario_data_store_.ResetIntervalData();

  // One WebContents was shown for part of the interval so one visible window.
  ASSERT_EQ(interval_data.max_visible_window_count, 1);

  // End a new interval, no WebContents was shown for the duration so no visible
  // window.
  interval_data = usage_scenario_data_store_.ResetIntervalData();
  ASSERT_EQ(interval_data.max_visible_window_count, 0);
}

}  // namespace metrics
