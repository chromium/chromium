// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_STATS_TRACKER_H_
#define CHROME_BROWSER_METRICS_TAB_STATS_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/tab_stats_data_store.h"
#include "chrome/browser/metrics/tab_stats_tracker_delegate.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/metrics/daily_event.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {
FORWARD_DECLARE_TEST(TabStatsTrackerBrowserTest,
                     TabDeletionGetsHandledProperly);

// Class for tracking and recording the tabs and browser windows usage.
//
// This class is meant to be used as a singleton by calling the SetInstance
// method, e.g.:
//     TabStatsTracker::SetInstance(
//         std::make_unique<TabStatsTracker>(g_browser_process->local_state()));
class TabStatsTracker : public TabStripModelObserver,
                        public BrowserListObserver,
                        public base::PowerObserver,
                        public resource_coordinator::TabLifecycleObserver {
 public:
  // Constructor. |pref_service| must outlive this object.
  explicit TabStatsTracker(PrefService* pref_service);
  ~TabStatsTracker() override;

  // Sets the |TabStatsTracker| global instance.
  static void SetInstance(std::unique_ptr<TabStatsTracker> instance);

  // Returns the |TabStatsTracker| global instance.
  static TabStatsTracker* GetInstance();

  // Registers prefs used to track tab stats.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void SetDelegateForTesting(
      std::unique_ptr<TabStatsTrackerDelegate> new_delegate);

  // Accessors.
  const TabStatsDataStore::TabsStats& tab_stats() const;

 protected:
  FRIEND_TEST_ALL_PREFIXES(TabStatsTrackerBrowserTest, FrozenTabPercentage);
  FRIEND_TEST_ALL_PREFIXES(TabStatsTrackerBrowserTest,
                           TabDeletionGetsHandledProperly);
#if defined(OS_WIN)
  FRIEND_TEST_ALL_PREFIXES(TabStatsTrackerBrowserTest,
                           TestCalculateAndRecordNativeWindowVisibilities);
#endif

  // The UmaStatsReportingDelegate is responsible for delivering statistics
  // reported by the TabStatsTracker via UMA.
  class UmaStatsReportingDelegate;

  // The observer that's used by |daily_event_| to report the metrics.
  class TabStatsDailyObserver : public DailyEvent::Observer {
   public:
    // Constructor. |reporting_delegate| and |data_store| must outlive this
    // object.
    TabStatsDailyObserver(UmaStatsReportingDelegate* reporting_delegate,
                          TabStatsDataStore* data_store)
        : reporting_delegate_(reporting_delegate), data_store_(data_store) {}
    ~TabStatsDailyObserver() override {}

    // Callback called when the daily event happen.
    void OnDailyEvent(DailyEvent::IntervalType type) override;

   private:
    // The delegate used to report the metrics.
    UmaStatsReportingDelegate* reporting_delegate_;

    // The data store that houses the metrics.
    TabStatsDataStore* data_store_;

    DISALLOW_COPY_AND_ASSIGN(TabStatsDailyObserver);
  };

  // Accessors, exposed for unittests:
  TabStatsDataStore* tab_stats_data_store() {
    return tab_stats_data_store_.get();
  }
  base::RepeatingTimer* daily_event_timer_for_testing() {
    return &daily_event_timer_;
  }
  DailyEvent* daily_event_for_testing() { return daily_event_.get(); }
  UmaStatsReportingDelegate* reporting_delegate_for_testing() {
    return reporting_delegate_.get();
  }
  std::vector<std::unique_ptr<base::RepeatingTimer>>*
  usage_interval_timers_for_testing() {
    return &usage_interval_timers_;
  }
  base::RepeatingTimer* heartbeat_timer_for_testing() {
    return &heartbeat_timer_;
  }

  // Reset the |reporting_delegate_| object to |reporting_delegate|, for testing
  // purposes.
  void reset_reporting_delegate_for_testing(
      UmaStatsReportingDelegate* reporting_delegate) {
    reporting_delegate_.reset(reporting_delegate);
  }

  // Reset the DailyEvent object to |daily_event|, for testing purposes.
  void reset_daily_event_for_testing(DailyEvent* daily_event) {
    daily_event_.reset(daily_event);
  }

  void reset_data_store_for_testing(TabStatsDataStore* data_store) {
    tab_stats_data_store_.reset(data_store);
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void TabChangedAt(content::WebContents* web_contents,
                    int index,
                    TabChangeType change_type) override;
  // base::PowerObserver:
  void OnResume() override;

  // resource_coordinator::TabLifecycleObserver:
  void OnDiscardedStateChange(content::WebContents* contents,
                              ::mojom::LifecycleUnitDiscardReason reason,
                              bool is_discarded) override;

  void OnAutoDiscardableStateChange(content::WebContents* contents,
                                    bool is_auto_discardable) override;

  // Callback when an interval timer triggers.
  void OnInterval(base::TimeDelta interval,
                  TabStatsDataStore::TabsStateDuringIntervalMap* interval_map);

  // Functions to call to start tracking a new tab.
  void OnInitialOrInsertedTab(content::WebContents* web_contents);

  // Functions to call when a WebContents get destroyed.
  void OnWebContentsDestroyed(content::WebContents* web_contents);

#if defined(OS_WIN)
  // Function to call aura_extra::ComputeNativeWindowOcclusionStatus() and
  // record the Visibility of all Chrome browser windows on Windows.
  void CalculateAndRecordNativeWindowVisibilities();
#endif

  // Function to call to report the tab heartbeat metrics.
  void OnHeartbeatEvent();

  // The name of the histogram used to report that the daily event happened.
  static const char kTabStatsDailyEventHistogramName[];

 private:
  // Observer used to be notified when the state of a WebContents changes or
  // when it's about to be destroyed.
  class WebContentsUsageObserver;

  // The delegate that reports the events.
  std::unique_ptr<UmaStatsReportingDelegate> reporting_delegate_;

  // Delegate to collect data;
  std::unique_ptr<TabStatsTrackerDelegate> delegate_;

  // The tab stats.
  std::unique_ptr<TabStatsDataStore> tab_stats_data_store_;

  // A daily event for collecting metrics once a day.
  std::unique_ptr<DailyEvent> daily_event_;

  // The timer used to periodically check if the daily event should be
  // triggered.
  base::RepeatingTimer daily_event_timer_;

#if defined(OS_WIN)
  // The timer used to periodically calculate the occlusion status of native
  // windows on Windows.
  base::RepeatingTimer native_window_occlusion_timer_;
#endif

  // The timers used to analyze how tabs are used during a given interval of
  // time.
  std::vector<std::unique_ptr<base::RepeatingTimer>> usage_interval_timers_;

  // The timer used to report the heartbeat metrics at regular interval.
  base::RepeatingTimer heartbeat_timer_;

  // The timer used to report tab discard and reload count histograms at regular
  // interval.
  base::RepeatingTimer tab_discard_reload_stats_timer_;

  // The observers that track how the tabs are used.
  std::map<content::WebContents*, std::unique_ptr<WebContentsUsageObserver>>
      web_contents_usage_observers_;

  // Called at regular time intervals to report tab discard count histograms.
  void OnTabDiscardCountReportInterval();

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(TabStatsTracker);
};

// The reporting delegate, which reports metrics via UMA.
class TabStatsTracker::UmaStatsReportingDelegate {
 public:
  // The name of the histogram that records the number of tabs total at resume
  // from sleep/hibernate.
  static const char kNumberOfTabsOnResumeHistogramName[];

  // The name of the histogram that records the maximum number of tabs opened in
  // a day.
  static const char kMaxTabsInADayHistogramName[];

  // The name of the histogram that records the maximum number of tabs opened in
  // the same window in a day.
  static const char kMaxTabsPerWindowInADayHistogramName[];

  // The name of the histogram that records the maximum number of windows
  // opened in a day.
  static const char kMaxWindowsInADayHistogramName[];

  // The name of the histograms that records how tabs have been used during a
  // given period of time. Will be appended with '_T' with T being the interval
  // window (in seconds).
  static const char kUnusedAndClosedInIntervalHistogramNameBase[];
  static const char kUnusedTabsInIntervalHistogramNameBase[];
  static const char kUsedAndClosedInIntervalHistogramNameBase[];
  static const char kUsedTabsInIntervalHistogramNameBase[];

  // The name of the histograms that records the current number of tabs/windows.
  static const char kTabCountHistogramName[];
  static const char kWindowCountHistogramName[];

  // The name of the histogram that records the percentage of hidden tabs that
  // are frozen. Separated into buckets based on the number of hidden tabs.
  static const char kFrozenTabPercentageHistogramNameBase[];
  static const char kFrozenTabPercentage1To5HiddenTabsHistogramName[];
  static const char kFrozenTabPercentage6To20HiddenTabsHistogramName[];
  static const char kFrozenTabPercentageMoreThan20HiddenTabsHistogramName[];

  UmaStatsReportingDelegate() {}
  virtual ~UmaStatsReportingDelegate() {}

  // Called at resume from sleep/hibernate.
  void ReportTabCountOnResume(size_t tab_count);

  // Called once per day to report the metrics.
  void ReportDailyMetrics(const TabStatsDataStore::TabsStats& tab_stats);

  // Report the tab heartbeat metrics.
  void ReportHeartbeatMetrics(const TabStatsDataStore::TabsStats& tab_stats);

  // Report some information about how tabs have been used during a given
  // interval of time.
  void ReportUsageDuringInterval(
      const TabStatsDataStore::TabsStateDuringIntervalMap& interval_map,
      base::TimeDelta interval);

#if defined(OS_WIN)
  void RecordNativeWindowVisibilities(size_t num_occluded,
                                      size_t num_visible,
                                      size_t num_hidden);
#endif

 protected:
  // Generates the name of the histograms that will track tab usage during a
  // given period of time.
  static std::string GetIntervalHistogramName(const char* base_name,
                                              base::TimeDelta interval);

  // Checks if Chrome is running in background with no visible windows, virtual
  // for unittesting.
  virtual bool IsChromeBackgroundedWithoutWindows();

 private:
  // Report the percentage of hidden tabs that are frozen.
  void ReportFrozenTabPercentage();

  DISALLOW_COPY_AND_ASSIGN(UmaStatsReportingDelegate);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_TAB_STATS_TRACKER_H_
