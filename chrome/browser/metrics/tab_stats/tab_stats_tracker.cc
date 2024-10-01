// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/background_mode_manager.h"
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)

namespace metrics {

namespace {

// The interval at which the DailyEvent::CheckInterval function should be
// called.
constexpr base::TimeDelta kDailyEventIntervalTimeDelta = base::Minutes(30);

// The interval at which the heartbeat tab metrics should be reported.
const base::TimeDelta kTabsHeartbeatReportingInterval = base::Minutes(5);

// The global TabStatsTracker instance.
TabStatsTracker* g_tab_stats_tracker_instance = nullptr;

void UmaHistogramCounts10000WithBatteryStateVariant(const char* histogram_name,
                                                    size_t value) {
  auto* power_monitor = base::PowerMonitor::GetInstance();
  DCHECK(power_monitor->IsInitialized());

  base::UmaHistogramCounts10000(histogram_name, value);

  const char* suffix =
      power_monitor->IsOnBatteryPower() ? ".OnBattery" : ".PluggedIn";

  base::UmaHistogramCounts10000(base::StrCat({histogram_name, suffix}), value);
}

}  // namespace

// static
const char TabStatsTracker::UmaStatsReportingDelegate::
    kNumberOfTabsOnResumeHistogramName[] = "Tabs.NumberOfTabsOnResume";
const char
    TabStatsTracker::UmaStatsReportingDelegate::kMaxTabsInADayHistogramName[] =
        "Tabs.MaxTabsInADay";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kMaxTabsPerWindowInADayHistogramName[] = "Tabs.MaxTabsPerWindowInADay";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kMaxWindowsInADayHistogramName[] = "Tabs.MaxWindowsInADay";
const char
    TabStatsTracker::UmaStatsReportingDelegate::kTabCountHistogramName[] =
        "Tabs.TabCount";
const char
    TabStatsTracker::UmaStatsReportingDelegate::kWindowCountHistogramName[] =
        "Tabs.WindowCount";
const char
    TabStatsTracker::UmaStatsReportingDelegate::kWindowWidthHistogramName[] =
        "Tabs.WindowWidth";

// Daily discard/reload histograms.
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyDiscardsExternalHistogramName[] = "Discarding.DailyDiscards.External";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyDiscardsUrgentHistogramName[] = "Discarding.DailyDiscards.Urgent";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyDiscardsProactiveHistogramName[] =
        "Discarding.DailyDiscards.Proactive";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyDiscardsSuggestedHistogramName[] =
        "Discarding.DailyDiscards.Suggested";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyReloadsExternalHistogramName[] = "Discarding.DailyReloads.External";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyReloadsUrgentHistogramName[] = "Discarding.DailyReloads.Urgent";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyReloadsProactiveHistogramName[] = "Discarding.DailyReloads.Proactive";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyReloadsSuggestedHistogramName[] = "Discarding.DailyReloads.Suggested";

const char TabStatsTracker::UmaStatsReportingDelegate::
    kTabDuplicateCountSingleWindowHistogramName[] =
        "Tabs.Duplicates.Count.SingleWindow";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kTabDuplicateCountAllProfileWindowsHistogramName[] =
        "Tabs.Duplicates.Count.AllProfileWindows";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kTabDuplicatePercentageSingleWindowHistogramName[] =
        "Tabs.Duplicates.Percentage.SingleWindow";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kTabDuplicatePercentageAllProfileWindowsHistogramName[] =
        "Tabs.Duplicates.Percentage.AllProfileWindows";

const TabStatsDataStore::TabsStats& TabStatsTracker::tab_stats() const {
  return tab_stats_data_store_->tab_stats();
}

TabStatsTracker::TabStatsTracker(PrefService* pref_service)
    : reporting_delegate_(std::make_unique<UmaStatsReportingDelegate>()),
      tab_stats_data_store_(std::make_unique<TabStatsDataStore>(pref_service)),
      daily_event_(std::make_unique<DailyEvent>(
          pref_service,
          ::prefs::kTabStatsDailySample,
          // Empty to skip recording the daily event type histogram.
          /* histogram_name=*/std::string())) {
  DCHECK(pref_service);

  // Add owned observers to the list manually since they are about to be
  // initialized. Subsequent observers should be added with
  // AddObserverAndSetInitialState().
  tab_stats_observers_.AddObserver(tab_stats_data_store_.get());

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
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);

  // Setup daily reporting of the stats aggregated in |tab_stats_data_store|.
  daily_event_->AddObserver(std::make_unique<TabStatsDailyObserver>(
      reporting_delegate_.get(), tab_stats_data_store_.get()));

  // Call the CheckInterval method to see if the data need to be immediately
  // reported.
  daily_event_->CheckInterval();
  daily_event_timer_.Start(FROM_HERE, kDailyEventIntervalTimeDelta,
                           daily_event_.get(), &DailyEvent::CheckInterval);

  heartbeat_timer_.Start(FROM_HERE, kTabsHeartbeatReportingInterval,
                         base::BindRepeating(&TabStatsTracker::OnHeartbeatEvent,
                                             base::Unretained(this)));

  g_browser_process->GetTabManager()->AddObserver(this);
}

TabStatsTracker::~TabStatsTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  BrowserList::GetInstance()->RemoveObserver(this);
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  g_browser_process->GetTabManager()->RemoveObserver(this);
}

// static
void TabStatsTracker::SetInstance(std::unique_ptr<TabStatsTracker> instance) {
  CHECK(!g_tab_stats_tracker_instance);
  g_tab_stats_tracker_instance = instance.release();
}

// static
void TabStatsTracker::ClearInstance() {
  CHECK(g_tab_stats_tracker_instance);
  delete g_tab_stats_tracker_instance;
  g_tab_stats_tracker_instance = nullptr;
}

// static
TabStatsTracker* TabStatsTracker::GetInstance() {
  CHECK(g_tab_stats_tracker_instance);
  return g_tab_stats_tracker_instance;
}

// static
bool TabStatsTracker::HasInstance() {
  return g_tab_stats_tracker_instance != nullptr;
}

void TabStatsTracker::AddObserverAndSetInitialState(
    TabStatsObserver* observer) {
  tab_stats_observers_.AddObserver(observer);

  // Initialization of |this| is complete at this point and all existing
  // Browsers are already observed. TabStatsObserver functions are called
  // directly only for |observer| which is new and needs to be caught up to the
  // current state.
  BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : *browser_list) {
    observer->OnWindowAdded();
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      auto* wc = browser->tab_strip_model()->GetWebContentsAt(i);
      observer->OnTabAdded(wc);
      if (wc->GetCurrentlyPlayingVideoCount())
        observer->OnVideoStartedPlaying(wc);
      if (wc->IsCurrentlyAudible())
        observer->OnTabIsAudibleChanged(wc);
      if (wc->HasActiveEffectivelyFullscreenVideo())
        observer->OnMediaEffectivelyFullscreenChanged(wc, true);
    }
  }
}

void TabStatsTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(::prefs::kTabStatsTotalTabCountMax, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsMaxTabsPerWindow, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsWindowCountMax, 0);
  DailyEvent::RegisterPref(registry, ::prefs::kTabStatsDailySample);

  // Preferences for saving discard/reload counts.
  registry->RegisterIntegerPref(::prefs::kTabStatsDiscardsExternal, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsDiscardsUrgent, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsDiscardsProactive, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsDiscardsSuggested, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsReloadsExternal, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsReloadsUrgent, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsReloadsProactive, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsReloadsSuggested, 0);
}

void TabStatsTracker::TabStatsDailyObserver::OnDailyEvent(
    DailyEvent::IntervalType type) {
  reporting_delegate_->ReportDailyMetrics(data_store_->tab_stats());
  data_store_->ResetMaximumsToCurrentState();
  data_store_->ClearTabDiscardAndReloadCounts();
}

class TabStatsTracker::WebContentsUsageObserver
    : public content::WebContentsObserver {
 public:
  WebContentsUsageObserver(content::WebContents* web_contents,
                           TabStatsTracker* tab_stats_tracker)
      : content::WebContentsObserver(web_contents),
        tab_stats_tracker_(tab_stats_tracker),
        ukm_source_id_(
            web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()) {}

  WebContentsUsageObserver(const WebContentsUsageObserver&) = delete;
  WebContentsUsageObserver& operator=(const WebContentsUsageObserver&) = delete;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    // Treat browser-initiated navigations as user interactions.
    if (!navigation_handle->IsRendererInitiated()) {
      for (TabStatsObserver& tab_stats_observer :
           tab_stats_tracker_->tab_stats_observers_) {
        tab_stats_observer.OnTabInteraction(web_contents());
      }
    }
    // Update navigation time for UKM reporting.
    navigation_time_ = navigation_handle->NavigationStart();
  }

  void PrimaryPageChanged(content::Page& page) override {
    ukm_source_id_ = page.GetMainDocument().GetPageUkmSourceId();

    // Update observers.
    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnPrimaryMainFrameNavigationCommitted(web_contents());
    }
  }

  void DidGetUserInteraction(const blink::WebInputEvent& event) override {
    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnTabInteraction(web_contents());
    }
  }

  void OnVisibilityChanged(content::Visibility visibility) override {
    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnTabVisibilityChanged(web_contents());
    }
  }

  void WebContentsDestroyed() override {
    if (ukm_source_id_) {
      ukm::builders::TabManager_TabLifetime(ukm_source_id_)
          .SetTimeSinceNavigation(
              (base::TimeTicks::Now() - navigation_time_).InMilliseconds())
          .Record(ukm::UkmRecorder::Get());
    }

    tab_stats_tracker_->OnWebContentsDestroyed(web_contents());
    // The call above will free |this| and so nothing should be done on this
    // object starting from here.
  }

  void OnAudioStateChanged(bool audible) override {
    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnTabIsAudibleChanged(web_contents());
    }
  }

  void MediaEffectivelyFullscreenChanged(bool is_fullscreen) override {
    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnMediaEffectivelyFullscreenChanged(web_contents(),
                                                             is_fullscreen);
    }
  }

  void MediaStartedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& media_type,
      const content::MediaPlayerId& id) override {
    if (!media_type.has_video)
      return;
    video_playing_count_++;
    if (video_playing_count_ == 1) {
      for (TabStatsObserver& tab_stats_observer :
           tab_stats_tracker_->tab_stats_observers_) {
        tab_stats_observer.OnVideoStartedPlaying(web_contents());
      }
    }
  }

  void MediaStoppedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& media_type,
      const content::MediaPlayerId& id,
      content::WebContentsObserver::MediaStoppedReason reason) override {
    if (!media_type.has_video)
      return;
    video_playing_count_--;
    if (video_playing_count_ == 0) {
      for (TabStatsObserver& tab_stats_observer :
           tab_stats_tracker_->tab_stats_observers_) {
        tab_stats_observer.OnVideoStoppedPlaying(web_contents());
      }
    }
  }

  void MediaDestroyed(const content::MediaPlayerId& id) override {
    for (auto& tab_stats_observer : tab_stats_tracker_->tab_stats_observers_)
      tab_stats_observer.OnMediaDestroyed(web_contents());
  }

  void WasDiscarded() override {
    if (ukm_source_id_) {
      ukm::builders::TabManager_TabLifetime(ukm_source_id_)
          .SetTimeSinceNavigation(
              (base::TimeTicks::Now() - navigation_time_).InMilliseconds())
          .Record(ukm::UkmRecorder::Get());
      ukm_source_id_ = 0;
    }

    for (TabStatsObserver& tab_stats_observer :
         tab_stats_tracker_->tab_stats_observers_) {
      tab_stats_observer.OnTabDiscarded(web_contents());
    }
  }

 private:
  raw_ptr<TabStatsTracker> tab_stats_tracker_;
  // The last navigation time associated with this tab.
  base::TimeTicks navigation_time_ = base::TimeTicks::Now();
  // Updated when a navigation is finished.
  ukm::SourceId ukm_source_id_ = 0;
  // The number of video currently playing in this tab.
  int video_playing_count_ = 0;
};

void TabStatsTracker::OnBrowserAdded(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
    tab_stats_observer.OnWindowAdded();
  }
  browser->tab_strip_model()->AddObserver(this);
}

void TabStatsTracker::OnBrowserRemoved(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
    tab_stats_observer.OnWindowRemoved();
  }
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
    for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
      tab_stats_observer.OnTabReplaced(replace->old_contents,
                                       replace->new_contents);
    }
    web_contents_usage_observers_.insert(std::make_pair(
        replace->new_contents, std::make_unique<WebContentsUsageObserver>(
                                   replace->new_contents, this)));
    web_contents_usage_observers_.erase(replace->old_contents);
  }
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

void TabStatsTracker::OnInitialOrInsertedTab(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If we already have a WebContentsObserver for this tab then it means that
  // it's already tracked and it's being dragged into a new window, there's
  // nothing to do here.
  if (!base::Contains(web_contents_usage_observers_, web_contents)) {
    for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
      tab_stats_observer.OnTabAdded(web_contents);
    }
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
  for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
    tab_stats_observer.OnTabRemoved(web_contents);
  }
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
  UmaHistogramCounts10000WithBatteryStateVariant(
      kNumberOfTabsOnResumeHistogramName, tab_count);
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportDailyMetrics(
    const TabStatsDataStore::TabsStats& tab_stats) {
  // Don't report the counts if they're equal to 0, this means that Chrome has
  // only been running in the background since the last time the metrics have
  // been reported.
  if (tab_stats.total_tab_count_max == 0)
    return;
  UmaHistogramCounts10000WithBatteryStateVariant(kMaxTabsInADayHistogramName,
                                                 tab_stats.total_tab_count_max);
  UmaHistogramCounts10000WithBatteryStateVariant(
      kMaxTabsPerWindowInADayHistogramName, tab_stats.max_tab_per_window);
  UmaHistogramCounts10000WithBatteryStateVariant(kMaxWindowsInADayHistogramName,
                                                 tab_stats.window_count_max);

  // Reports the discard/reload counts.
  const size_t external_index =
      static_cast<size_t>(LifecycleUnitDiscardReason::EXTERNAL);
  const size_t urgent_index =
      static_cast<size_t>(LifecycleUnitDiscardReason::URGENT);
  const size_t proactive_index =
      static_cast<size_t>(LifecycleUnitDiscardReason::PROACTIVE);
  const size_t suggested_index =
      static_cast<size_t>(LifecycleUnitDiscardReason::SUGGESTED);
  base::UmaHistogramCounts10000(kDailyDiscardsExternalHistogramName,
                                tab_stats.tab_discard_counts[external_index]);
  base::UmaHistogramCounts10000(kDailyDiscardsUrgentHistogramName,
                                tab_stats.tab_discard_counts[urgent_index]);
  base::UmaHistogramCounts10000(kDailyDiscardsProactiveHistogramName,
                                tab_stats.tab_discard_counts[proactive_index]);
  base::UmaHistogramCounts10000(kDailyDiscardsSuggestedHistogramName,
                                tab_stats.tab_discard_counts[suggested_index]);
  base::UmaHistogramCounts10000(kDailyReloadsExternalHistogramName,
                                tab_stats.tab_reload_counts[external_index]);
  base::UmaHistogramCounts10000(kDailyReloadsUrgentHistogramName,
                                tab_stats.tab_reload_counts[urgent_index]);
  base::UmaHistogramCounts10000(kDailyReloadsProactiveHistogramName,
                                tab_stats.tab_reload_counts[proactive_index]);
  base::UmaHistogramCounts10000(kDailyReloadsSuggestedHistogramName,
                                tab_stats.tab_reload_counts[suggested_index]);
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportHeartbeatMetrics(
    const TabStatsDataStore::TabsStats& tab_stats) {
  // Don't report anything if Chrome is running in background with no visible
  // window.
  if (IsChromeBackgroundedWithoutWindows())
    return;

  UmaHistogramCounts10000WithBatteryStateVariant(kTabCountHistogramName,
                                                 tab_stats.total_tab_count);
  UmaHistogramCounts10000WithBatteryStateVariant(kWindowCountHistogramName,
                                                 tab_stats.window_count);
  if (base::FeatureList::IsEnabled(features::kTabDuplicateMetrics)) {
    ReportTabDuplicateMetrics();
  }
  // Record the width of all open browser windows with tabs.
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->type() != Browser::TYPE_NORMAL)
      continue;

    const BrowserWindow* window = browser->window();

    // Only consider visible windows.
    if (!window->IsVisible() || window->IsMinimized())
      continue;

    // Get the window's size (in DIPs).
    const gfx::Size window_size = browser->window()->GetBounds().size();

    // If the size is for some reason 0 in either dimension, skip it.
    if (window_size.IsEmpty())
      continue;

    // A 4K screen is 4096 pixels wide. Doubling this and rounding up to
    // 10000 should give a reasonable upper bound on DIPs. For the
    // minimum width, pick an arbitrary value of 100. Most screens are
    // unlikely to be this small, and likewise a browser window's min
    // width is around this size.
    UMA_HISTOGRAM_CUSTOM_COUNTS(kWindowWidthHistogramName, window_size.width(),
                                100, 10000, 50);
  }
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportTabDuplicateMetrics() {
  std::map<Profile*, DuplicateData> duplicate_data_per_profile;
  for (Browser* const browser : *BrowserList::GetInstance()) {
    if (browser->type() != Browser::TYPE_NORMAL) {
      continue;
    }

    Profile* const profile = browser->profile();
    DuplicateData duplicate_data_multi_window =
        duplicate_data_per_profile[profile];
    DuplicateData duplicate_data_single_window = DuplicateData();

    const int tab_count = browser->tab_strip_model()->count();
    duplicate_data_multi_window.tab_count += tab_count;
    duplicate_data_single_window.tab_count = tab_count;

    for (int index = 0; index < tab_count; index++) {
      content::WebContents* const web_contents =
          browser->tab_strip_model()->GetWebContentsAt(index);
      const GURL url = web_contents->GetURL();
      auto seen_urls_single_window_result =
          duplicate_data_single_window.seen_urls.insert(url);
      if (!seen_urls_single_window_result.second) {
        duplicate_data_single_window.duplicate_count++;
      }
      // Guest mode and incognito should not count for the per-profile metrics
      if (profile->IsOffTheRecord()) {
        continue;
      }
      auto seen_urls_multi_window_result =
          duplicate_data_multi_window.seen_urls.insert(url);
      if (!seen_urls_multi_window_result.second) {
        duplicate_data_multi_window.duplicate_count++;
      }
    }
    duplicate_data_per_profile[profile] = duplicate_data_multi_window;

    base::UmaHistogramCounts100(kTabDuplicateCountSingleWindowHistogramName,
                                duplicate_data_single_window.duplicate_count);
    if (duplicate_data_single_window.tab_count > 0) {
      base::UmaHistogramPercentage(
          kTabDuplicatePercentageSingleWindowHistogramName,
          duplicate_data_single_window.duplicate_count * 100 /
              duplicate_data_single_window.tab_count);
    }
  }
  for (const auto& duplicate_data : duplicate_data_per_profile) {
    // Guest mode and incognito should not count for the per-profile metrics
    Profile* const profile = duplicate_data.first;
    if (profile->IsOffTheRecord()) {
      continue;
    }

    base::UmaHistogramCounts100(
        kTabDuplicateCountAllProfileWindowsHistogramName,
        duplicate_data.second.duplicate_count);
    if (duplicate_data.second.tab_count > 0) {
      base::UmaHistogramPercentage(
          kTabDuplicatePercentageAllProfileWindowsHistogramName,
          duplicate_data.second.duplicate_count * 100 /
              duplicate_data.second.tab_count);
    }
  }
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

TabStatsTracker::UmaStatsReportingDelegate::DuplicateData::DuplicateData() {
  duplicate_count = 0;
  tab_count = 0;
  seen_urls = {};
}

TabStatsTracker::UmaStatsReportingDelegate::DuplicateData::DuplicateData(
    const DuplicateData& other) {
  duplicate_count = other.duplicate_count;
  tab_count = other.tab_count;
  seen_urls = std::set(other.seen_urls);
}

TabStatsTracker::UmaStatsReportingDelegate::DuplicateData::~DuplicateData() =
    default;

}  // namespace metrics
