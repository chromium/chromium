// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/power_monitor/power_monitor.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/metrics/daily_event.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#else
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif

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

TabStatsTracker::TabStripInterface::TabStripInterface(
    TabStripInterface::PlatformModel* model)
    : model_(model) {}

TabStatsTracker::TabStripInterface::~TabStripInterface() = default;

void TabStatsTracker::TabStripInterface::ForEachWebContents(
    base::FunctionRef<void(content::WebContents*)> func) const {
  for (size_t i = 0; i < GetTabCount(); ++i) {
    if (auto* web_contents = GetWebContentsAt(i)) {
      func(web_contents);
    }
  }
}

#if BUILDFLAG(IS_ANDROID)

size_t TabStatsTracker::TabStripInterface::GetTabCount() const {
  return tab_model()->GetTabCount();
}

content::WebContents* TabStatsTracker::TabStripInterface::GetActiveWebContents()
    const {
  return tab_model()->GetActiveWebContents();
}

content::WebContents* TabStatsTracker::TabStripInterface::GetWebContentsAt(
    size_t index) const {
  return tab_model()->GetWebContentsAt(index);
}

Profile* TabStatsTracker::TabStripInterface::GetProfile() const {
  return tab_model()->GetProfile();
}

bool TabStatsTracker::TabStripInterface::IsInNormalBrowser() const {
  return true;
}

void TabStatsTracker::TabStripInterface::ActivateTabAtForTesting(size_t index) {
  tab_model()->SetActiveIndex(index);
}

void TabStatsTracker::TabStripInterface::CloseTabAtForTesting(size_t index) {
  tab_model()->CloseTabAt(index);
}

// static
void TabStatsTracker::TabStripInterface::ForEach(
    base::FunctionRef<void(const TabStripInterface&)> func) {
  for (TabModel* tab_model : TabModelList::models()) {
    func(TabStripInterface(tab_model));
  }
}

#else  // !BUILDFLAG(IS_ANDROID)

size_t TabStatsTracker::TabStripInterface::GetTabCount() const {
  return browser()->tab_strip_model()->count();
}

content::WebContents* TabStatsTracker::TabStripInterface::GetActiveWebContents()
    const {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

content::WebContents* TabStatsTracker::TabStripInterface::GetWebContentsAt(
    size_t index) const {
  return browser()->tab_strip_model()->GetWebContentsAt(index);
}

Profile* TabStatsTracker::TabStripInterface::GetProfile() const {
  return browser()->profile();
}

bool TabStatsTracker::TabStripInterface::IsInNormalBrowser() const {
  return browser()->type() == Browser::TYPE_NORMAL;
}

void TabStatsTracker::TabStripInterface::ActivateTabAtForTesting(size_t index) {
  browser()->tab_strip_model()->ActivateTabAt(index);
}

void TabStatsTracker::TabStripInterface::CloseTabAtForTesting(size_t index) {
  browser()->tab_strip_model()->CloseWebContentsAt(
      index, TabCloseTypes::CLOSE_USER_GESTURE);
}

// static
void TabStatsTracker::TabStripInterface::ForEach(
    base::FunctionRef<void(const TabStripInterface&)> func) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    func(TabStripInterface(browser));
  }
}

#endif  // !BUILDFLAG(IS_ANDROID)

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
    kDailyDiscardsFrozenWithGrowingMemoryHistogramName[] =
        "Discarding.DailyDiscards.FrozenWithGrowingMemory";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyReloadsExternalHistogramName[] = "Discarding.DailyReloads.External";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyReloadsUrgentHistogramName[] = "Discarding.DailyReloads.Urgent";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyReloadsProactiveHistogramName[] = "Discarding.DailyReloads.Proactive";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyReloadsSuggestedHistogramName[] = "Discarding.DailyReloads.Suggested";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kDailyReloadsFrozenWithGrowingMemoryHistogramName[] =
        "Discarding.DailyReloads.FrozenWithGrowingMemory";

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

const char TabStatsTracker::UmaStatsReportingDelegate::
    kTabDuplicateExcludingFragmentsCountSingleWindowHistogramName[] =
        "Tabs.DuplicatesExcludingFragments.Count.SingleWindow";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kTabDuplicateExcludingFragmentsCountAllProfileWindowsHistogramName[] =
        "Tabs.DuplicatesExcludingFragments.Count.AllProfileWindows";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kTabDuplicateExcludingFragmentsPercentageSingleWindowHistogramName[] =
        "Tabs.DuplicatesExcludingFragments.Percentage.SingleWindow";
const char TabStatsTracker::UmaStatsReportingDelegate::
    kTabDuplicateExcludingFragmentsPercentageAllProfileWindowsHistogramName[] =
        "Tabs.DuplicatesExcludingFragments.Percentage.AllProfileWindows";

// When initialized, TabWatcher gets the list of existing windows/tabs. There
// shouldn't be any if it's initialized at startup but this will ensure that the
// counts stay accurate if the initialization gets moved to after the creation
// of the first tab.

#if BUILDFLAG(IS_ANDROID)

class TabStatsTracker::TabWatcher final : public TabModelListObserver,
                                          public TabModelObserver,
                                          public TabAndroid::Observer {
 public:
  explicit TabWatcher(TabStatsTracker& tracker) : tracker_(tracker) {
    for (TabModel* tab_model : TabModelList::models()) {
      OnTabModelAdded(tab_model);
      for (int i = 0; i < tab_model->GetTabCount(); ++i) {
        OnTabAdded(tab_model->GetTabAt(i));
      }
      tracker_->OnTabStripNewTabCount(tab_model->GetTabCount());
    }
    TabModelList::AddObserver(this);
  }

  ~TabWatcher() final { TabModelList::RemoveObserver(this); }

  // TabModelListObserver:

  void OnTabModelAdded(TabModel* tab_model) final {
    tracker_->OnTabStripAdded();
    tab_model_observations_.AddObservation(tab_model);
  }

  void OnTabModelRemoved(TabModel* tab_model) final {
    tab_model_observations_.RemoveObservation(tab_model);
    tracker_->OnTabStripRemoved();
  }

  // TabModelObserver:

  void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type) final {
    OnTabAdded(tab);
    auto* tab_model = TabModelList::GetTabModelForTabAndroid(tab);
    tracker_->OnTabStripNewTabCount(CHECK_DEREF(tab_model).GetTabCount());
  }

  void TabRemoved(TabAndroid* tab) final {
    // The tab was removed from the model, either because it closed or moved to
    // a different model. Either way stop watching for the WebContents.
    if (tab_android_observations_.IsObservingSource(tab)) {
      tab_android_observations_.RemoveObservation(tab);
    }
  }

  // TabAndroid::Observer:

  void OnInitWebContents(TabAndroid* tab) final {
    CHECK(tab->web_contents());
    tracker_->OnInitialOrInsertedTab(tab->web_contents());
    tab_android_observations_.RemoveObservation(tab);
  }

 private:
  void OnTabAdded(TabAndroid* tab) {
    if (content::WebContents* web_contents = tab->web_contents()) {
      tracker_->OnInitialOrInsertedTab(web_contents);
    } else if (!tab_android_observations_.IsObservingSource(tab)) {
      // The WebContents hasn't been attached to the tab yet. Start tracking it
      // when TabAndroid::Observer::OnInitWebContents is called. Note OnTabAdded
      // can be called while the tab is already being observed, if it's called
      // from the TabModel constructor while an async DidAddTab notification is
      // in flight.
      tab_android_observations_.AddObservation(tab);
    }
  }

  raw_ref<TabStatsTracker> tracker_;
  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};
  base::ScopedMultiSourceObservation<TabAndroid, TabAndroid::Observer>
      tab_android_observations_{this};
};

#else  // !BUILDFLAG(IS_ANDROID)

class TabStatsTracker::TabWatcher final : public BrowserListObserver,
                                          public TabStripModelObserver {
 public:
  explicit TabWatcher(TabStatsTracker& tracker) : tracker_(tracker) {
    BrowserList* browser_list = BrowserList::GetInstance();
    for (Browser* browser : *browser_list) {
      OnBrowserAdded(browser);
      for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
        content::WebContents* web_contents =
            browser->tab_strip_model()->GetWebContentsAt(i);
        CHECK(web_contents);
        tracker_->OnInitialOrInsertedTab(web_contents);
      }
      tracker_->OnTabStripNewTabCount(browser->tab_strip_model()->count());
    }
    browser_list_observation_.Observe(BrowserList::GetInstance());
  }

  ~TabWatcher() final = default;

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) final {
    tracker_->OnTabStripAdded();
    browser->tab_strip_model()->AddObserver(this);
  }

  void OnBrowserRemoved(Browser* browser) final {
    browser->tab_strip_model()->RemoveObserver(this);
    tracker_->OnTabStripRemoved();
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(TabStripModel* tab_strip_model,
                              const TabStripModelChange& change,
                              const TabStripSelectionChange& selection) final {
    if (change.type() == TabStripModelChange::kInserted) {
      for (const auto& contents : change.GetInsert()->contents) {
        tracker_->OnInitialOrInsertedTab(contents.contents);
      }
      tracker_->OnTabStripNewTabCount(tab_strip_model->count());
    } else if (change.type() == TabStripModelChange::kReplaced) {
      auto* replace = change.GetReplace();
      tracker_->OnTabReplaced(replace->old_contents, replace->new_contents);
    }
  }

 private:
  raw_ref<TabStatsTracker> tracker_;
  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
};

#endif  // !BUILDFLAG(IS_ANDROID)

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
          /* histogram_name=*/std::string())),
      tab_watcher_(std::make_unique<TabWatcher>(*this)) {
  DCHECK(pref_service);

  AddObserverAndSetInitialState(tab_stats_data_store_.get());

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

#if !BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/412634171): Enable this when discarding is supported on
  // Android.
  resource_coordinator::GetTabLifecycleUnitSource()->AddLifecycleObserver(this);
#endif
}

TabStatsTracker::~TabStatsTracker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
#if !BUILDFLAG(IS_ANDROID)
  resource_coordinator::GetTabLifecycleUnitSource()->RemoveLifecycleObserver(
      this);
#endif
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
  TabStripInterface::ForEach([observer](const TabStripInterface& tab_strip) {
    observer->OnWindowAdded();
    tab_strip.ForEachWebContents([observer](content::WebContents* wc) {
      observer->OnTabAdded(wc);
      if (wc->GetCurrentlyPlayingVideoCount()) {
        observer->OnVideoStartedPlaying(wc);
      }
      if (wc->IsCurrentlyAudible()) {
        observer->OnTabIsAudibleChanged(wc);
      }
      if (wc->HasActiveEffectivelyFullscreenVideo()) {
        observer->OnMediaEffectivelyFullscreenChanged(wc, true);
      }
    });
  });
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
  registry->RegisterIntegerPref(
      ::prefs::kTabStatsDiscardsFrozenWithGrowingMemory, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsReloadsExternal, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsReloadsUrgent, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsReloadsProactive, 0);
  registry->RegisterIntegerPref(::prefs::kTabStatsReloadsSuggested, 0);
  registry->RegisterIntegerPref(
      ::prefs::kTabStatsReloadsFrozenWithGrowingMemory, 0);
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
            web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()),
        was_playing_video_(web_contents->GetCurrentlyPlayingVideoCount() > 0) {}

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
    MaybeNotifyVideoStartedStoppedPlaying();
  }

  void MediaStoppedPlaying(
      const content::WebContentsObserver::MediaPlayerInfo& media_type,
      const content::MediaPlayerId& id,
      content::WebContentsObserver::MediaStoppedReason reason) override {
    MaybeNotifyVideoStartedStoppedPlaying();
  }

  void MediaMetadataChanged(
      const content::WebContentsObserver::MediaPlayerInfo& video_type,
      const content::MediaPlayerId& id) override {
    MaybeNotifyVideoStartedStoppedPlaying();
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
  void MaybeNotifyVideoStartedStoppedPlaying() {
    const bool is_playing_video =
        web_contents()->GetCurrentlyPlayingVideoCount() > 0;

    if (!was_playing_video_ && is_playing_video) {
      for (TabStatsObserver& tab_stats_observer :
           tab_stats_tracker_->tab_stats_observers_) {
        tab_stats_observer.OnVideoStartedPlaying(web_contents());
      }
    } else if (was_playing_video_ && !is_playing_video) {
      for (TabStatsObserver& tab_stats_observer :
           tab_stats_tracker_->tab_stats_observers_) {
        tab_stats_observer.OnVideoStoppedPlaying(web_contents());
      }
    }

    was_playing_video_ = is_playing_video;
  }

  raw_ptr<TabStatsTracker> tab_stats_tracker_;
  // The last navigation time associated with this tab.
  base::TimeTicks navigation_time_ = base::TimeTicks::Now();
  // Updated when a navigation is finished.
  ukm::SourceId ukm_source_id_ = 0;
  // Whether video was playing in this tab the last time we checked.
  bool was_playing_video_;
};

content::WebContentsObserver*
TabStatsTracker::GetWebContentsUsageObserverForTesting(
    content::WebContents* web_contents) {
  if (auto it = web_contents_usage_observers_.find(web_contents);
      it != web_contents_usage_observers_.end()) {
    return it->second.get();
  }
  return nullptr;
}

void TabStatsTracker::OnResume() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reporting_delegate_->ReportTabCountOnResume(
      tab_stats_data_store_->tab_stats().total_tab_count);
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/412634171): Enable this when discarding is supported on
// Android.
void TabStatsTracker::OnLifecycleUnitStateChanged(
    resource_coordinator::LifecycleUnit* lifecycle_unit,
    ::mojom::LifecycleUnitState previous_state,
    ::mojom::LifecycleUnitStateChangeReason reason) {
  const ::mojom::LifecycleUnitState new_state = lifecycle_unit->GetState();
  if (previous_state == ::mojom::LifecycleUnitState::DISCARDED ||
      new_state == ::mojom::LifecycleUnitState::DISCARDED) {
    tab_stats_data_store_->OnTabDiscardStateChange(
        lifecycle_unit->GetDiscardReason(),
        new_state == ::mojom::LifecycleUnitState::DISCARDED);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID)

void TabStatsTracker::OnTabStripAdded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
    tab_stats_observer.OnWindowAdded();
  }
}

void TabStatsTracker::OnTabStripRemoved() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
    tab_stats_observer.OnWindowRemoved();
  }
}

void TabStatsTracker::OnTabStripNewTabCount(size_t new_tab_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  tab_stats_data_store_->UpdateMaxTabsPerWindowIfNeeded(new_tab_count);
}

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

void TabStatsTracker::OnTabReplaced(content::WebContents* old_contents,
                                    content::WebContents* new_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (TabStatsObserver& tab_stats_observer : tab_stats_observers_) {
    tab_stats_observer.OnTabReplaced(old_contents, new_contents);
  }
  web_contents_usage_observers_.insert(std::make_pair(
      new_contents,
      std::make_unique<WebContentsUsageObserver>(new_contents, this)));
  web_contents_usage_observers_.erase(old_contents);
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
  const size_t frozen_with_growing_memory_index = static_cast<size_t>(
      LifecycleUnitDiscardReason::FROZEN_WITH_GROWING_MEMORY);
  base::UmaHistogramCounts10000(kDailyDiscardsExternalHistogramName,
                                tab_stats.tab_discard_counts[external_index]);
  base::UmaHistogramCounts10000(kDailyDiscardsUrgentHistogramName,
                                tab_stats.tab_discard_counts[urgent_index]);
  base::UmaHistogramCounts10000(kDailyDiscardsProactiveHistogramName,
                                tab_stats.tab_discard_counts[proactive_index]);
  base::UmaHistogramCounts10000(kDailyDiscardsSuggestedHistogramName,
                                tab_stats.tab_discard_counts[suggested_index]);
  base::UmaHistogramCounts10000(
      kDailyDiscardsFrozenWithGrowingMemoryHistogramName,
      tab_stats.tab_discard_counts[frozen_with_growing_memory_index]);
  base::UmaHistogramCounts10000(kDailyReloadsExternalHistogramName,
                                tab_stats.tab_reload_counts[external_index]);
  base::UmaHistogramCounts10000(kDailyReloadsUrgentHistogramName,
                                tab_stats.tab_reload_counts[urgent_index]);
  base::UmaHistogramCounts10000(kDailyReloadsProactiveHistogramName,
                                tab_stats.tab_reload_counts[proactive_index]);
  base::UmaHistogramCounts10000(kDailyReloadsSuggestedHistogramName,
                                tab_stats.tab_reload_counts[suggested_index]);
  base::UmaHistogramCounts10000(
      kDailyReloadsFrozenWithGrowingMemoryHistogramName,
      tab_stats.tab_reload_counts[frozen_with_growing_memory_index]);
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
    ReportTabDuplicateMetrics(true);
    ReportTabDuplicateMetrics(false);
  }
#if !BUILDFLAG(IS_ANDROID)
  // Record the width of all open browser windows with tabs.
  TabStripInterface::ForEach([&](const TabStripInterface& tab_strip) {
    if (!tab_strip.IsInNormalBrowser()) {
      return;
    }

    const BrowserWindow* window = tab_strip.browser()->window();

    // Only consider visible windows.
    if (!window->IsVisible() || window->IsMinimized()) {
      return;
    }

    // Get the window's size (in DIPs).
    const gfx::Size window_size = window->GetBounds().size();

    // If the size is for some reason 0 in either dimension, skip it.
    if (window_size.IsEmpty()) {
      return;
    }

    // A 4K screen is 4096 pixels wide. Doubling this and rounding up to
    // 10000 should give a reasonable upper bound on DIPs. For the
    // minimum width, pick an arbitrary value of 100. Most screens are
    // unlikely to be this small, and likewise a browser window's min
    // width is around this size.
    UMA_HISTOGRAM_CUSTOM_COUNTS(kWindowWidthHistogramName, window_size.width(),
                                100, 10000, 50);
  });
#endif
}

void TabStatsTracker::UmaStatsReportingDelegate::ReportTabDuplicateMetrics(
    bool exclude_fragments) {
  std::map<Profile*, DuplicateData> duplicate_data_per_profile;
  TabStripInterface::ForEach([&](const TabStripInterface& tab_strip) {
    if (!tab_strip.IsInNormalBrowser()) {
      return;
    }

    Profile* const profile = tab_strip.GetProfile();
    DuplicateData duplicate_data_multi_window =
        duplicate_data_per_profile[profile];
    DuplicateData duplicate_data_single_window = DuplicateData();

    const size_t tab_count = tab_strip.GetTabCount();
    duplicate_data_multi_window.tab_count += tab_count;
    duplicate_data_single_window.tab_count = tab_count;

    tab_strip.ForEachWebContents([&](content::WebContents* web_contents) {
      const GURL full_url = web_contents->GetURL();
      const GURL url = exclude_fragments ? full_url.GetWithoutRef() : full_url;
      auto seen_urls_single_window_result =
          duplicate_data_single_window.seen_urls.insert(url);
      if (!seen_urls_single_window_result.second) {
        duplicate_data_single_window.duplicate_count++;
      }
      // Guest mode and incognito should not count for the per-profile metrics
      if (profile->IsOffTheRecord()) {
        return;
      }
      auto seen_urls_multi_window_result =
          duplicate_data_multi_window.seen_urls.insert(url);
      if (!seen_urls_multi_window_result.second) {
        duplicate_data_multi_window.duplicate_count++;
      }
    });
    duplicate_data_per_profile[profile] = duplicate_data_multi_window;

    base::UmaHistogramCounts100(
        exclude_fragments
            ? kTabDuplicateExcludingFragmentsCountSingleWindowHistogramName
            : kTabDuplicateCountSingleWindowHistogramName,
        duplicate_data_single_window.duplicate_count);
    if (duplicate_data_single_window.tab_count > 0) {
      base::UmaHistogramPercentage(
          exclude_fragments
              ? kTabDuplicateExcludingFragmentsPercentageSingleWindowHistogramName
              : kTabDuplicatePercentageSingleWindowHistogramName,
          duplicate_data_single_window.duplicate_count * 100 /
              duplicate_data_single_window.tab_count);
    }
  });

  for (const auto& duplicate_data : duplicate_data_per_profile) {
    // Guest mode and incognito should not count for the per-profile metrics
    Profile* const profile = duplicate_data.first;
    if (profile->IsOffTheRecord()) {
      continue;
    }

    base::UmaHistogramCounts100(
        exclude_fragments
            ? kTabDuplicateExcludingFragmentsCountAllProfileWindowsHistogramName
            : kTabDuplicateCountAllProfileWindowsHistogramName,
        duplicate_data.second.duplicate_count);
    if (duplicate_data.second.tab_count > 0) {
      base::UmaHistogramPercentage(
          exclude_fragments
              ? kTabDuplicateExcludingFragmentsPercentageAllProfileWindowsHistogramName
              : kTabDuplicatePercentageAllProfileWindowsHistogramName,
          duplicate_data.second.duplicate_count * 100 /
              duplicate_data.second.tab_count);
    }
  }
}

bool TabStatsTracker::UmaStatsReportingDelegate::
    IsChromeBackgroundedWithoutWindows() {
#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  return KeepAliveRegistry::GetInstance()->WouldRestartWithout({
      // Transient startup related KeepAlives, not related to any UI.
      KeepAliveOrigin::SESSION_RESTORE,
      KeepAliveOrigin::BACKGROUND_MODE_MANAGER_STARTUP,

      KeepAliveOrigin::BACKGROUND_SYNC,

      // Notification KeepAlives are not dependent on the Chrome UI being
      // loaded, and can be registered when we were in pure background mode.
      // They just block it to avoid issues. Ignore them when determining if we
      // are in that mode.
      KeepAliveOrigin::NOTIFICATION,
      KeepAliveOrigin::PENDING_NOTIFICATION_CLICK_EVENT,
      KeepAliveOrigin::PENDING_NOTIFICATION_CLOSE_EVENT,
      KeepAliveOrigin::IN_FLIGHT_PUSH_MESSAGE,
  });
#else
  return false;
#endif  // BUILDFLAG(ENABLE_BACKGROUND_MODE)
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
