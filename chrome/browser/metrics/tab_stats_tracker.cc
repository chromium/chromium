// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats_tracker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/power_monitor/power_monitor.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

namespace metrics {

namespace {

// The interval at which the DailyEvent::CheckInterval function should be
// called.
constexpr base::TimeDelta kDailyEventIntervalTimeDelta =
    base::TimeDelta::FromMinutes(30);

// The intervals at which we report the number of unused tabs. This is used for
// all the tab usage histograms listed below.
//
// The 'Tabs.TabUsageIntervalLength' histogram suffixes entry in histograms.xml
// should be kept in sync with these values.
constexpr base::TimeDelta kTabUsageReportingIntervals[] = {
    base::TimeDelta::FromSeconds(30), base::TimeDelta::FromMinutes(1),
    base::TimeDelta::FromMinutes(10), base::TimeDelta::FromHours(1),
    base::TimeDelta::FromHours(5),    base::TimeDelta::FromHours(12)};

#if defined(OS_WIN)
const base::TimeDelta kNativeWindowOcclusionCalculationInterval =
    base::TimeDelta::FromMinutes(10);
#endif

// The interval at which the heartbeat tab metrics should be reported.
const base::TimeDelta kTabsHeartbeatReportingInterval =
    base::TimeDelta::FromMinutes(5);

// The global TabStatsTracker instance.
TabStatsTracker* g_tab_stats_tracker_instance = nullptr;

// Ensure that an interval is a valid one (i.e. listed in
// |kTabUsageReportingIntervals|).
bool IsValidInterval(base::TimeDelta interval) {
  return base::Contains(kTabUsageReportingIntervals, interval);
}

}  // namespace

// static
const char TabStatsTracker::kTabStatsDailyEventHistogramName[] =
    "Tabs.TabsStatsDailyEventInterval";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kNumberOfTabsOnResumeHistogramName[] = "Tabs.NumberOfTabsOnResume";
const char
    TabStatsTracker::UmaStatsReportingDelegate::kMaxTabsInADayHistogramName[] =
        "Tabs.MaxTabsInADay";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kMaxTabsPerWindowInADayHistogramName[] = "Tabs.MaxTabsPerWindowInADay";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kMaxWindowsInADayHistogramName[] = "Tabs.MaxWindowsInADay";

// Tab usage histograms.
const char TabStatsTracker::UmaStatsReportingDelegate::
    kUnusedAndClosedInIntervalHistogramNameBase[] =
        "Tabs.UnusedAndClosedInInterval.Count";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kUnusedTabsInIntervalHistogramNameBase[] = "Tabs.UnusedInInterval.Count";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kUsedAndClosedInIntervalHistogramNameBase[] =
        "Tabs.UsedAndClosedInInterval.Count";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kUsedTabsInIntervalHistogramNameBase[] = "Tabs.UsedInInterval.Count";

const char
    TabStatsTracker::UmaStatsReportingDelegate::kTabCountHistogramName[] =
        "Tabs.TabCount";
const char
    TabStatsTracker::UmaStatsReportingDelegate::kWindowCountHistogramName[] =
        "Tabs.WindowCount";

const char TabStatsTracker::UmaStatsReportingDelegate::
    kFrozenTabPercentageHistogramNameBase[] = "Tabs.FrozenTabPercentage";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kFrozenTabPercentage1To5HiddenTabsHistogramName[] = "1To5HiddenTabs";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kFrozenTabPercentage6To20HiddenTabsHistogramName[] = "6To20HiddenTabs";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kFrozenTabPercentageMoreThan20HiddenTabsHistogramName[] =
        "MoreThan20HiddenTabs";

// Tab discard and reload histogram names in the same order as in discard reason
// enum.
const char* kTabDiscardCountHistogramNames[] = {
    "Discarding.DiscardsPer10Minutes.Extension",
    "Discarding.DiscardsPer10Minutes.Proactive",
    "Discarding.DiscardsPer10Minutes.Urgent",
};

const char* kTabReloadCountHistogramNames[] = {
    "Discarding.ReloadsPer10Minutes.Extension",
    "Discarding.ReloadsPer10Minutes.Proactive",
    "Discarding.ReloadsPer10Minutes.Urgent",
};

static_assert(base::size(kTabDiscardCountHistogramNames) ==
                  static_cast<size_t>(LifecycleUnitDiscardReason::kMaxValue) +
                      1,
              "There must be an entry in kTabDiscardCountHistogramNames for "
              "each discard reason.");
static_assert(base::size(kTabReloadCountHistogramNames) ==
                  static_cast<size_t>(LifecycleUnitDiscardReason::kMaxValue) +
                      1,
              "There must be an entry in kTabReloadCountHistogramNames for "
              "each discard reason.");

const TabStatsDataStore::TabsStats& TabStatsTracker::tab_stats() const {
  return tab_stats_data_store_->tab_stats();
}

TabStatsTracker::TabStatsTracker(PrefService* pref_service)
    : reporting_delegate_(std::make_unique<UmaStatsReportingDelegate>()),
      delegate_(std::make_unique<TabStatsTrackerDelegate>()),
      tab_stats_data_store_(std::make_unique<TabStatsDataStore>(pref_service)),
      daily_event_(
          std::make_unique<DailyEvent>(pref_service,
                                       ::prefs::kTabStatsDailySample,
                                       kTabStatsDailyEventHistogramName)) {
  DCHECK(pref_service);
  // Get the list of existing windows/tabs. There shouldn't be any if this is
  // initialized at startup but this will ensure that the counts stay accurate
  // if the initialization gets moved to after the creation of the first tab.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    OnBrowserAdded(browser);
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i)
      OnInitialOrInsertedTab(browser->tab_strip_model()->GetWebContentsAt(i));
    tab_stats_data_store_->UpdateMaxTabsPerWindowIfNeeded(
        static_cast<size_t>(browser->tab_strip_model()->count()));
  }

  browser_list->AddObserver(this);
  base::PowerMonitor::AddObserver(this);

  daily_event_->AddObserver(std::make_unique<TabStatsDailyObserver>(
      reporting_delegate_.get(), tab_stats_data_store_.get()));
  // Call the CheckInterval method to see if the data need to be immediately
  // reported.
  daily_event_->CheckInterval();
  daily_event_timer_.Start(FROM_HERE, kDailyEventIntervalTimeDelta,
                           daily_event_.get(), &DailyEvent::CheckInterval);

  // Initialize the interval maps and timers associated with them.
  for (base::TimeDelta interval : kTabUsageReportingIntervals) {
    TabStatsDataStore::TabsStateDuringIntervalMap* interval_map =
        tab_stats_data_store_->AddInterval();
    // Setup the timer associated with this interval.
    std::unique_ptr<base::RepeatingTimer> timer =
        std::make_unique<base::RepeatingTimer>();
    timer->Start(
        FROM_HERE, interval,
        base::BindRepeating(&TabStatsTracker::OnInterval,
                            base::Unretained(this), interval, interval_map));
    usage_interval_timers_.push_back(std::move(timer));
  }

// The native window occlusion calculation is specific to Windows.
#if defined(OS_WIN)
  native_window_occlusion_timer_.Start(
      FROM_HERE, kNativeWindowOcclusionCalculationInterval,
      base::BindRepeating(
          &TabStatsTracker::CalculateAndRecordNativeWindowVisibilities,
          base::Unretained(this)));
#endif

  heartbeat_timer_.Start(FROM_HERE, kTabsHeartbeatReportingInterval,
                         base::BindRepeating(&TabStatsTracker::OnHeartbeatEvent,
                                             base::Unretained(this)));

  // Report discarding stats every 10 minutes.
  tab_discard_reload_stats_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMinutes(10),
      base::BindRepeating(&TabStatsTracker::OnTabDiscardCountReportInterval,
                          base::Unretained(this)));

  g_browser_process->GetTabManager()->AddObserver(this);
}

TabStatsTracker::~TabStatsTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BrowserList::GetInstance()->RemoveObserver(this);

  base::PowerMonitor::RemoveObserver(this);
}

// static
void TabStatsTracker::SetInstance(std::unique_ptr<TabStatsTracker> instance) {
  DCHECK_EQ(nullptr, g_tab_stats_tracker_instance);
  g_tab_stats_tracker_instance = instance.release();
}

TabStatsTracker* TabStatsTracker::GetInstance() {
  return g_tab_stats_tracker_instance;
}

void TabStatsTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(::prefs::kTabStatsTotalTabCountMax, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsMaxTabsPerWindow, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsWindowCountMax, 0);
  DailyEvent::RegisterPref(registry, ::prefs::kTabStatsDailySample);
}

void TabStatsTracker::SetDelegateForTesting(
    std::unique_ptr<TabStatsTrackerDelegate> new_delegate) {
  delegate_ = std::move(new_delegate);
}

void TabStatsTracker::TabStatsDailyObserver::OnDailyEvent(
    DailyEvent::IntervalType type) {
  reporting_delegate_->ReportDailyMetrics(data_store_->tab_stats());
  data_store_->ResetMaximumsToCurrentState();
}

class TabStatsTracker::WebContentsUsageObserver
    : public content::WebContentsObserver {
 public:
  WebContentsUsageObserver(content::WebContents* web_contents,
                           TabStatsTracker* tab_stats_tracker)
      : content::WebContentsObserver(web_contents),
        tab_stats_tracker_(tab_stats_tracker) {}

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    // Treat browser-initiated navigations as user interactions.
    if (!navigation_handle->IsRendererInitiated()) {
      tab_stats_tracker_->tab_stats_data_store()->OnTabInteraction(
          web_contents());
    }
  }

  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override {
    tab_stats_tracker_->tab_stats_data_store()->OnTabInteraction(
        web_contents());
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    if (visibility == content::Visibility::VISIBLE)
      tab_stats_tracker_->tab_stats_data_store()->OnTabVisible(web_contents());
  }

  void WebContentsDestroyed() override {
    tab_stats_tracker_->OnWebContentsDestroyed(web_contents());

    // The call above will free |this| and so nothing should be done on this
    // object starting from here.
  }

 private:
  TabStatsTracker* tab_stats_tracker_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsUsageObserver);
};

void TabStatsTracker::OnBrowserAdded(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tab_stats_data_store_->OnWindowAdded();
  browser->tab_strip_model()->AddObserver(this);
}

void TabStatsTracker::OnBrowserRemoved(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tab_stats_data_store_->OnWindowRemoved();
  browser->tab_strip_model()->RemoveObserver(this);
}

void TabStatsTracker::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& contents : change.GetInsert()->contents)
      OnInitialOrInsertedTab(contents.contents);

    tab_stats_data_store_->UpdateMaxTabsPerWindowIfNeeded(
        static_cast<size_t>(tab_strip_model->count()));

    return;
  }

  if (change.type() == TabStripModelChange::kReplaced) {
    auto* replace = change.GetReplace();
    tab_stats_data_store_->OnTabReplaced(replace->old_contents,
                                         replace->new_contents);
    web_contents_usage_observers_.insert(std::make_pair(
        replace->new_contents, std::make_unique<WebContentsUsageObserver>(
                                   replace->new_contents, this)));
    web_contents_usage_observers_.erase(replace->old_contents);
  }
}

void TabStatsTracker::TabChangedAt(content::WebContents* web_contents,
                                   int index,
                                   TabChangeType change_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignore 'loading' and 'title' changes, we're only interested in audio here.
  if (change_type != TabChangeType::kAll)
    return;
  if (web_contents->IsCurrentlyAudible())
    tab_stats_data_store_->OnTabAudible(web_contents);
}

void TabStatsTracker::OnResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reporting_delegate_->ReportTabCountOnResume(
      tab_stats_data_store_->tab_stats().total_tab_count);
}

// resource_coordinator::TabLifecycleObserver:
void TabStatsTracker::OnDiscardedStateChange(
    content::WebContents* contents,
    ::mojom::LifecycleUnitDiscardReason reason,
    bool is_discarded) {
  // Increment the count in the data store for tabs metrics reporting.
  tab_stats_data_store_->OnTabDiscardStateChange(reason, is_discarded);
}

void TabStatsTracker::OnAutoDiscardableStateChange(
    content::WebContents* contents,
    bool is_auto_discardable) {}

void TabStatsTracker::OnInterval(
    base::TimeDelta interval,
    TabStatsDataStore::TabsStateDuringIntervalMap* interval_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(interval_map);
  reporting_delegate_->ReportUsageDuringInterval(*interval_map, interval);
  // Reset the interval data.
  tab_stats_data_store_->ResetIntervalData(interval_map);
}

void TabStatsTracker::OnTabDiscardCountReportInterval() {
  for (size_t reason = 0;
       reason < static_cast<size_t>(LifecycleUnitDiscardReason::kMaxValue) + 1;
       reason++) {
    base::UmaHistogramCounts100(
        kTabDiscardCountHistogramNames[reason],
        tab_stats_data_store_->tab_stats().tab_discard_counts[reason]);
    base::UmaHistogramCounts100(
        kTabReloadCountHistogramNames[reason],
        tab_stats_data_store_->tab_stats().tab_reload_counts[reason]);
  }
  tab_stats_data_store_->ClearTabDiscardAndReloadCounts();
}

void TabStatsTracker::OnInitialOrInsertedTab(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If we already have a WebContentsObserver for this tab then it means that
  // it's already tracked and it's being dragged into a new window, there's
  // nothing to do here.
  if (!base::Contains(web_contents_usage_observers_, web_contents)) {
    tab_stats_data_store_->OnTabAdded(web_contents);
    web_contents_usage_observers_.insert(std::make_pair(
        web_contents,
        std::make_unique<WebContentsUsageObserver>(web_contents, this)));
  }
}

void TabStatsTracker::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(web_contents_usage_observers_, web_contents));
  web_contents_usage_observers_.erase(
      web_contents_usage_observers_.find(web_contents));
  tab_stats_data_store_->OnTabRemoved(web_contents);
}

void TabStatsTracker::OnHeartbeatEvent() {
  reporting_delegate_->ReportHeartbeatMetrics(
      tab_stats_data_store_->tab_stats());
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportTabCountOnResume(
    size_t tab_count) {
  // Don't report the number of tabs on resume if Chrome is running in
  // background with no visible window.
  if (IsChromeBackgroundedWithoutWindows())
    return;
  UMA_HISTOGRAM_COUNTS_10000(kNumberOfTabsOnResumeHistogramName, tab_count);
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportDailyMetrics(
    const TabStatsDataStore::TabsStats& tab_stats) {
  // Don't report the counts if they're equal to 0, this means that Chrome has
  // only been running in the background since the last time the metrics have
  // been reported.
  if (tab_stats.total_tab_count_max == 0)
    return;
  UMA_HISTOGRAM_COUNTS_10000(kMaxTabsInADayHistogramName,
                             tab_stats.total_tab_count_max);
  UMA_HISTOGRAM_COUNTS_10000(kMaxTabsPerWindowInADayHistogramName,
                             tab_stats.max_tab_per_window);
  UMA_HISTOGRAM_COUNTS_10000(kMaxWindowsInADayHistogramName,
                             tab_stats.window_count_max);
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportHeartbeatMetrics(
    const TabStatsDataStore::TabsStats& tab_stats) {
  // Don't report anything if Chrome is running in background with no visible
  // window.
  if (IsChromeBackgroundedWithoutWindows())
    return;

  UMA_HISTOGRAM_COUNTS_10000(kTabCountHistogramName, tab_stats.total_tab_count);
  UMA_HISTOGRAM_COUNTS_10000(kWindowCountHistogramName, tab_stats.window_count);
  ReportFrozenTabPercentage();
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportFrozenTabPercentage() {
  int frozen_tab_count = 0;
  int hidden_tab_count = 0;

  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      content::WebContents* web_contents =
          browser->tab_strip_model()->GetWebContentsAt(i);
      auto* tab_lifecycle_unit_external =
          resource_coordinator::TabLifecycleUnitExternal::FromWebContents(
              web_contents);

      if (!tab_lifecycle_unit_external)
        continue;

      if (tab_lifecycle_unit_external->IsFrozen())
        ++frozen_tab_count;

      if (web_contents->GetVisibility() == content::Visibility::HIDDEN)
        ++hidden_tab_count;
    }
  }

  if (!hidden_tab_count)
    return;

  int frozen_tab_percentage = (100 * frozen_tab_count) / hidden_tab_count;

  std::string frozen_tab_percentage_histogram_suffix;
  if (hidden_tab_count > 20) {
    UMA_HISTOGRAM_PERCENTAGE(
        base::JoinString(
            {kFrozenTabPercentageHistogramNameBase,
             kFrozenTabPercentageMoreThan20HiddenTabsHistogramName},
            "."),
        frozen_tab_percentage);
  } else if (hidden_tab_count > 5) {
    UMA_HISTOGRAM_PERCENTAGE(
        base::JoinString({kFrozenTabPercentageHistogramNameBase,
                          kFrozenTabPercentage6To20HiddenTabsHistogramName},
                         "."),
        frozen_tab_percentage);
  } else {
    UMA_HISTOGRAM_PERCENTAGE(
        base::JoinString({kFrozenTabPercentageHistogramNameBase,
                          kFrozenTabPercentage1To5HiddenTabsHistogramName},
                         "."),
        frozen_tab_percentage);
  }
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportUsageDuringInterval(
    const TabStatsDataStore::TabsStateDuringIntervalMap& interval_map,
    base::TimeDelta interval) {
  // Counts the number of used/unused tabs during this interval, a tabs counts
  // as unused if it hasn't been interacted with or visible during the duration
  // of the interval.
  size_t used_tabs = 0;
  size_t used_and_closed_tabs = 0;
  size_t unused_tabs = 0;
  size_t unused_and_closed_tabs = 0;
  for (const auto& iter : interval_map) {
    // There's currently no distinction between a visible/audible tab and one
    // that has been interacted with in these metrics.
    // TODO(sebmarchand): Add a metric that track the number of tab that have
    // been visible/audible but not interacted with during an interval,
    // https://crbug.com/800828.
    if (iter.second.interacted_during_interval ||
        iter.second.visible_or_audible_during_interval) {
      if (iter.second.exists_currently)
        ++used_tabs;
      else
        ++used_and_closed_tabs;
    } else {
      if (iter.second.exists_currently)
        ++unused_tabs;
      else
        ++unused_and_closed_tabs;
    }
  }

  std::string used_and_closed_histogram_name = GetIntervalHistogramName(
      UmaStatsReportingDelegate::kUsedAndClosedInIntervalHistogramNameBase,
      interval);
  std::string used_histogram_name = GetIntervalHistogramName(
      UmaStatsReportingDelegate::kUsedTabsInIntervalHistogramNameBase,
      interval);
  std::string unused_and_closed_histogram_name = GetIntervalHistogramName(
      UmaStatsReportingDelegate::kUnusedAndClosedInIntervalHistogramNameBase,
      interval);
  std::string unused_histogram_name = GetIntervalHistogramName(
      UmaStatsReportingDelegate::kUnusedTabsInIntervalHistogramNameBase,
      interval);

  base::UmaHistogramCounts10000(used_and_closed_histogram_name,
                                used_and_closed_tabs);
  base::UmaHistogramCounts10000(used_histogram_name, used_tabs);
  base::UmaHistogramCounts10000(unused_and_closed_histogram_name,
                                unused_and_closed_tabs);
  base::UmaHistogramCounts10000(unused_histogram_name, unused_tabs);
}

// static
std::string
TabStatsTracker::UmaStatsReportingDelegate::GetIntervalHistogramName(
    const char* base_name,
    base::TimeDelta interval) {
  DCHECK(IsValidInterval(interval));
  return base::StringPrintf("%s_%zu", base_name,
                            static_cast<size_t>(interval.InSeconds()));
}

bool TabStatsTracker::UmaStatsReportingDelegate::
    IsChromeBackgroundedWithoutWindows() {
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  if (g_browser_process && g_browser_process->background_mode_manager()
                               ->IsBackgroundWithoutWindows()) {
    return true;
  }
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
  return false;
}

}  // namespace metrics
