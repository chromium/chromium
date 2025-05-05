// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_TRACKER_H_
#define CHROME_BROWSER_METRICS_TAB_STATS_TAB_STATS_TRACKER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/function_ref.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_observer.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_data_store.h"
#include "components/metrics/daily_event.h"
#include "content/public/browser/web_contents_observer.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#endif

class PrefRegistrySimple;
class PrefService;
class Profile;

#if BUILDFLAG(IS_ANDROID)
class TabModel;
#else
class Browser;
#endif

namespace content {
class WebContents;
}

namespace metrics {
FORWARD_DECLARE_TEST(TabStatsTrackerBrowserTest,
                     TabDeletionGetsHandledProperly);

// Class for tracking and recording the tabs and browser windows usage.
//
// This class is meant to be used as a singleton by calling the SetInstance
// method, e.g.:
//     TabStatsTracker::SetInstance(
//         std::make_unique<TabStatsTracker>(g_browser_process->local_state()));
class TabStatsTracker :
#if !BUILDFLAG(IS_ANDROID)
    public resource_coordinator::LifecycleUnitObserver,
#endif
    public base::PowerSuspendObserver {
 public:
  // Abstraction of a Browser + TabStripModel (on desktop) or a TabModel (on
  // Android).
  class TabStripInterface;

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

  content::WebContentsObserver* GetWebContentsUsageObserverForTesting(
      content::WebContents* web_contents);

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

    ~TabStatsDailyObserver() override = default;

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

  // base::PowerSuspendObserver:
  void OnResume() override;

#if !BUILDFLAG(IS_ANDROID)
  // resource_coordinator::LifecycleUnitObserver:
  void OnLifecycleUnitStateChanged(
      resource_coordinator::LifecycleUnit* lifecycle_unit,
      ::mojom::LifecycleUnitState previous_state,
      ::mojom::LifecycleUnitStateChangeReason reason) override;
#endif

  // Functions to call when a tab strip (or the Android equivalent) is added,
  // removed or modified.
  void OnTabStripAdded();
  void OnTabStripRemoved();
  void OnTabStripNewTabCount(size_t tab_count);

  // Functions to call to start tracking a new tab.
  void OnInitialOrInsertedTab(content::WebContents* web_contents);
  void OnTabReplaced(content::WebContents* old_contents,
                     content::WebContents* new_contents);

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

  // A class that watches for tabs to be added and removed. Abstracts away
  // tab strip differences on Android and desktop.
  class TabWatcher;

  // For access to OnTabStripAdded() and OnTabStripRemoved().
  friend class TabWatcher;

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

  std::unique_ptr<TabWatcher> tab_watcher_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// A Browser + TabStripModel (on desktop) or a TabModel (on Android).
// The TabStripInterface must not outlive the underlying model.
class TabStatsTracker::TabStripInterface {
 public:
#if BUILDFLAG(IS_ANDROID)
  using PlatformModel = TabModel;

  const TabModel* tab_model() const { return model_.get(); }
  TabModel* tab_model() { return model_.get(); }
#else
  using PlatformModel = Browser;

  const Browser* browser() const { return model_.get(); }
  Browser* browser() { return model_.get(); }
#endif

  explicit TabStripInterface(PlatformModel* model);
  ~TabStripInterface();

  TabStripInterface(const TabStripInterface&) = delete;
  TabStripInterface& operator=(const TabStripInterface&) = delete;

  // Calls `func` for each tab in the tab strip that has a non-null
  // WebContents. On Android, tabs will be skipped if their WebContents isn't
  // initialized yet.
  void ForEachWebContents(
      base::FunctionRef<void(content::WebContents*)> func) const;

  // Returns the count of tabs in this tab strip.
  size_t GetTabCount() const;

  // Returns the active tab for this tab strip. On Android this may return
  // nullptr if the tab's WebContents isn't initialized yet.
  content::WebContents* GetActiveWebContents() const;

  // Returns the tab at `index` of this tab strip. On Android this may return
  // nullptr if the tab's WebContents isn't initialized yet.
  content::WebContents* GetWebContentsAt(size_t index) const;

  // Returns the profile this tab strip is attached to.
  Profile* GetProfile() const;

  // Returns true if this tab strip is attached to a TYPE_NORMAL Browser.
  // Always returns true on Android.
  bool IsInNormalBrowser() const;

  // Activates the tab at `index` of this tab strip.
  void ActivateTabAtForTesting(size_t index);

  // Closes the tab at `index` of this tab strip.
  void CloseTabAtForTesting(size_t index);

  // Calls `func` for each existing Browser + TabStripModel (or TabModel on
  // Android).
  static void ForEach(base::FunctionRef<void(const TabStripInterface&)> func);

 private:
  raw_ptr<PlatformModel> model_;
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
  // for each discard reason.
  static const char kDailyDiscardsExternalHistogramName[];
  static const char kDailyDiscardsUrgentHistogramName[];
  static const char kDailyDiscardsProactiveHistogramName[];
  static const char kDailyDiscardsSuggestedHistogramName[];
  static const char kDailyDiscardsFrozenWithGrowingMemoryHistogramName[];
  static const char kDailyReloadsExternalHistogramName[];
  static const char kDailyReloadsUrgentHistogramName[];
  static const char kDailyReloadsProactiveHistogramName[];
  static const char kDailyReloadsSuggestedHistogramName[];
  static const char kDailyReloadsFrozenWithGrowingMemoryHistogramName[];

  // The names of the histograms that record duplicate tab data.
  static const char kTabDuplicateCountSingleWindowHistogramName[];
  static const char kTabDuplicateCountAllProfileWindowsHistogramName[];
  static const char kTabDuplicatePercentageSingleWindowHistogramName[];
  static const char kTabDuplicatePercentageAllProfileWindowsHistogramName[];
  static const char
      kTabDuplicateExcludingFragmentsCountSingleWindowHistogramName[];
  static const char
      kTabDuplicateExcludingFragmentsCountAllProfileWindowsHistogramName[];
  static const char
      kTabDuplicateExcludingFragmentsPercentageSingleWindowHistogramName[];
  static const char
      kTabDuplicateExcludingFragmentsPercentageAllProfileWindowsHistogramName[];

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
  // their complexity. |exclude_fragments| will treat two tabs with the same
  // URL apart from trailing fragments as duplicates, otherwise will only treat
  // exact URL matches as duplicates.
  void ReportTabDuplicateMetrics(bool exclude_fragments);

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
