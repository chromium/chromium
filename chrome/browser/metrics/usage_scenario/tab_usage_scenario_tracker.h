// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USAGE_SCENARIO_TAB_USAGE_SCENARIO_TRACKER_H_
#define CHROME_BROWSER_METRICS_USAGE_SCENARIO_TAB_USAGE_SCENARIO_TRACKER_H_

#include <optional>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_observer.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "content/public/browser/visibility.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/display/display_observer.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

namespace metrics {

// Used to relay information gathered from TabStatsTracker to
// UsageScenarioDataStore. No information is stored in this class.
class TabUsageScenarioTracker : public TabStatsObserver,
                                public display::DisplayObserver {
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
  void OnTabReplaced(content::WebContents* old_contents,
                     content::WebContents* new_contents) override;
  void OnTabVisibilityChanged(content::WebContents* web_contents) override;
  void OnTabDiscarded(content::WebContents* web_contents) override;
  void OnTabInteraction(content::WebContents* web_contents) override;
  void OnTabIsAudibleChanged(content::WebContents* web_contents) override;
  void OnMediaEffectivelyFullscreenChanged(content::WebContents* web_contents,
                                           bool is_fullscreen) override;
  void OnMediaDestroyed(content::WebContents* web_contents) override;
  void OnPrimaryMainFrameNavigationCommitted(
      content::WebContents* web_contents) override;
  void OnVideoStartedPlaying(content::WebContents* web_contents) override;
  void OnVideoStoppedPlaying(content::WebContents* web_contents) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;

 protected:
  // Invoked when displays are added or removed.
  void OnNumDisplaysChanged();

 private:
  using VisibleTabsMap = base::flat_map<content::WebContents*,
                                        std::pair<ukm::SourceId, url::Origin>>;

  // Can be overridden in tests.
  virtual int GetNumDisplays();

  // Should be called when |visible_tab_iter| switch from being visible to non
  // visible. |visible_tab_iter| should be an iterator in |visible_contents_|.
  void OnTabBecameHidden(VisibleTabsMap::iterator* visible_tab_iter);

  // Should be called when a WebContents is being destroyed, there's 2 possible
  // causes for this:
  //   - The tab that contains it is being removed.
  //   - A tab's WebContents is being replaced.
  void OnWebContentsRemoved(content::WebContents* web_contents);

  void InsertContentsInMapOfVisibleTabs(content::WebContents* web_contents);

  // Non-owning. Needs to outlive this class.
  raw_ptr<UsageScenarioDataStoreImpl> usage_scenario_data_store_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Keep track of the WebContents currently playing video.
  base::flat_set<raw_ptr<content::WebContents, CtnExperimental>>
      contents_playing_video_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The last reading of the number of displays.
  std::optional<int> last_num_displays_;

  // Keep track of the visible WebContents and the navigation data associated to
  // them. The associated sourceID for tabs that don't have committed a main
  // frame navigation is ukm::kInvalidSourceID and the origin is empty.
  VisibleTabsMap visible_tabs_ GUARDED_BY_CONTEXT(sequence_checker_);

  // WebContents currently playing video fullscreen.
  base::flat_set<raw_ptr<content::WebContents, DanglingUntriaged>>
      contents_playing_video_fullscreen_;

  display::ScopedDisplayObserver display_observer_{this};

  // Used to verify that all access to |usage_scenario_data_store_| goes through
  // the same sequence as the one that created this object.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_USAGE_SCENARIO_TAB_USAGE_SCENARIO_TRACKER_H_
