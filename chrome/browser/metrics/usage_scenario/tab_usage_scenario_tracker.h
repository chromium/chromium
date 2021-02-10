// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USAGE_SCENARIO_TAB_USAGE_SCENARIO_TRACKER_H_
#define CHROME_BROWSER_METRICS_USAGE_SCENARIO_TAB_USAGE_SCENARIO_TRACKER_H_

#include "base/containers/flat_set.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_observer.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_tracker.h"
#include "content/public/browser/visibility.h"

namespace content {
class WebContents;
}  // namespace content

namespace metrics {

// Used to relay information gathered from TabStatsTracker to
// UsageScenarioDataStore. No information is stored in this class.
class TabUsageScenarioTracker : public TabStatsObserver {
 public:
  // This class will not own |usage_scenario_data_store| so it needs to be
  // outlived by it.
  explicit TabUsageScenarioTracker(
      UsageScenarioDataStoreImpl* usage_scenario_data_store);
  TabUsageScenarioTracker(const TabUsageScenarioTracker& rhs) = delete;
  TabUsageScenarioTracker& operator=(const TabUsageScenarioTracker& rhs) =
      delete;
  ~TabUsageScenarioTracker() override;

  // TabStatsObserver:
  void OnTabAdded(content::WebContents* web_contents) override;
  void OnTabRemoved(content::WebContents* web_contents) override;
  void OnTabVisibilityChanged(content::WebContents* web_contents,
                              content::Visibility visibility) override;
  void OnTabInteraction(content::WebContents* web_contents) override;

  // Testing trampolines:
  void OnTabAddedForTesting(content::WebContents* web_contents,
                            content::Visibility initial_visibility);

 private:
  // Internal versions of the TabStatsObserver functions that are more easily
  // testable.
  void OnTabAdded(content::WebContents* web_contents,
                  content::Visibility initial_visibility);

  // Non-owning. Needs to outlive this class.
  UsageScenarioDataStoreImpl* usage_scenario_data_store_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Keep track of the visible web-contents.
  base::flat_set<content::WebContents*> visible_contents_;

  // Used to verify that all access to |usage_scenario_data_store_| goes through
  // the same sequence as the one that created this object.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_USAGE_SCENARIO_TAB_USAGE_SCENARIO_TRACKER_H_
