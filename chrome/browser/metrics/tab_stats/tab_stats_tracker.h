// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_TRACKER_H_
#define CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_data_store.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/metrics/daily_event.h"
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
                        public base::PowerSuspendObserver,
                        public resource_coordinator::TabLifecycleObserver {
 public:
  // Constructor. |pref_service| must outlive this object.
  explicit TabStatsTracker(PrefService* pref_service);

  TabStatsTracker(const TabStatsTracker&) = delete;
  TabStatsTracker& operator=(const TabStatsTracker&) = delete;

  ~TabStatsTracker() override;

  // Sets the |TabStatsTracker| global instance.
  static void SetInstance(std::unique_ptr<TabStatsTracker> instance);
  // Clears and tears down the |TabStatsTracker| global instance.
  static void ClearInstance();

  // Returns the |TabStatsTracker| global instance. CHECKs there is an instance.
  static TabStatsTracker* GetInstance();
  // Returns whether there is a global instance.
  static bool HasInstance();

  // Registers a TabStatsObserver instance. Upon registering the initial state
  // of the observer is made to match the current browser/tab state.
  void AddObserverAndSetInitialState(TabStatsObserver* observer);

  void RemoveObserver(TabStatsObserver* observer) {
    tab_stats_observers_.RemoveObserver(observer);
  }

  // Registers prefs used to track tab stats.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Accessors.
  const TabStatsDataStore::TabsStats& tab_stats() const;

 protected:
  FRIEND_TEST_ALL_PREFIXES(TabStatsTrackerBrowserTest,
                           TabDeletionGetsHandledProperly);
  FRIEND_TEST_ALL_PREFIXES(TabStatsTrackerBrowserTest,
                           TabsAndWindowsAreCountedAccurately);
#if BUILDFLAG(IS_WIN)
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

    TabStatsDailyObserver(const TabStatsDailyObserver&) = delete;
    TabStatsDailyObserver& operator=(const TabStatsDailyObserver&) = delete;

    ~TabStatsDailyObserver() override {}

    // Callback called when the daily event happen.
    void OnDailyEvent(DailyEvent::IntervalType type) override;

   private:
    // The delegate used to report the metrics.
    raw_ptr<UmaStatsReportingDelegate, DanglingUntriaged> reporting_delegate_;

    // The data store that houses the metrics.
    raw_ptr<TabStatsDataStore> data_store_;
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

  // base::PowerSuspendObserver:
  void OnResume() override;

  // resource_coordinator::TabLifecycleObserver:
  void OnDiscardedStateChange(content::WebContents* contents,
                              ::mojom::LifecycleUnitDiscardReason reason,
                              bool is_discarded) override;

  void OnAutoDiscardableStateChange(content::WebContents* contents,
                                    bool is_auto_discardable) override;

  // Functions to call to start tracking a new tab.
  void OnInitialOrInsertedTab(content::WebContents* web_contents);

  // Functions to call when a WebContents get destroyed.
  void OnWebContentsDestroyed(content::WebContents* web_contents);

  // Function to call to report the tab heartbeat metrics.
  void OnHeartbeatEvent();

 private:
  // Observer used to be notified when the state of a WebContents changes or
  // when it's about to be destroyed.
  class WebContentsUsageObserver;

  // For access to |tab_stats_observers_|
  friend class WebContentsUsageObserver;

  // The delegate that reports the events.
  std::unique_ptr<UmaStatsReportingDelegate> reporting_delegate_;

  // The tab stats.
  std::unique_ptr<TabStatsDataStore> tab_stats_data_store_;

  // A daily event for collecting metrics once a day.
  std::unique_ptr<DailyEvent> daily_event_;

  // The list of registered observers.
  base::ObserverList<TabStatsObserver> tab_stats_observers_;

  // The timer used to periodically check if the daily event should be
  // triggered.
  base::RepeatingTimer daily_event_timer_;

  // The timer used to report the heartbeat metrics at regular interval.
  base::RepeatingTimer heartbeat_timer_;

  // The observers that track how the tabs are used.
  std::map<content::WebContents*, std::unique_ptr<WebContentsUsageObserver>>
      web_contents_usage_observers_;

  SEQUENCE_CHECKER(sequence_checker_);
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

  // The name of the histograms that records the current number of tabs/windows.
  static const char kTabCountHistogramName[];
  static const char kWindowCountHistogramName[];

  // The name of the histogram that records each window's width, in DIPs.
  static const char kWindowWidthHistogramName[];

  // The names of the histograms that record daily discard/reload counts caused
  // by external/urgent/proactive/suggested events.
  static const char kDailyDiscardsExternalHistogramName[];
  static const char kDailyDiscardsUrgentHistogramName[];
  static const char kDailyDiscardsProactiveHistogramName[];
  static const char kDailyDiscardsSuggestedHistogramName[];
  static const char kDailyReloadsExternalHistogramName[];
  static const char kDailyReloadsUrgentHistogramName[];
  static const char kDailyReloadsProactiveHistogramName[];
  static const char kDailyReloadsSuggestedHistogramName[];

  // The names of the histograms that record duplicate tab data.
  static const char kTabDuplicateCountSingleWindowHistogramName[];
  static const char kTabDuplicateCountAllProfileWindowsHistogramName[];
  static const char kTabDuplicatePercentageSingleWindowHistogramName[];
  static const char kTabDuplicatePercentageAllProfileWindowsHistogramName[];

  UmaStatsReportingDelegate() = default;

  UmaStatsReportingDelegate(const UmaStatsReportingDelegate&) = delete;
  UmaStatsReportingDelegate& operator=(const UmaStatsReportingDelegate&) =
      delete;

  virtual ~UmaStatsReportingDelegate() = default;

  // Called at resume from sleep/hibernate.
  void ReportTabCountOnResume(size_t tab_count);

  // Called once per day to report the metrics.
  void ReportDailyMetrics(const TabStatsDataStore::TabsStats& tab_stats);

  // Report the tab heartbeat metrics.
  void ReportHeartbeatMetrics(const TabStatsDataStore::TabsStats& tab_stats);

  // Calculate and report the metrics related to tab duplicates, which are
  // re-calculated each time rather than cached like the other metrics due to
  // their complexity.
  void ReportTabDuplicateMetrics();

 protected:
  // Checks if Chrome is running in background with no visible windows, virtual
  // for unittesting.
  virtual bool IsChromeBackgroundedWithoutWindows();

 private:
  struct DuplicateData {
    DuplicateData();
    DuplicateData(const DuplicateData&);
    ~DuplicateData();

    int duplicate_count;
    int tab_count;
    std::set<GURL> seen_urls;
  };
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_TRACKER_H_
