// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager.h"

#include <stddef.h>

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/memory/oom_memory_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/background_tab_navigation_throttle.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_manager_resource_coordinator_signal_observer.h"
#include "chrome/browser/resource_coordinator/tab_manager_stats_collector.h"
#include "chrome/browser/resource_coordinator/tab_manager_web_contents_data.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "components/metrics/system_memory_stats_recorder.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/page_importance_signals.h"
#include "net/base/network_change_notifier.h"
#include "third_party/blink/public/platform/web_sudden_termination_disabler_type.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/resource_coordinator/tab_manager_delegate_chromeos.h"
#endif

using base::TimeDelta;
using base::TimeTicks;
using content::BrowserThread;
using content::WebContents;

namespace resource_coordinator {
namespace {

using LoadingState = TabLoadTracker::LoadingState;

// The default timeout time after which the next background tab gets loaded if
// the previous tab has not finished loading yet. This is ignored in kPaused
// loading mode.
constexpr TimeDelta kDefaultBackgroundTabLoadTimeout =
    TimeDelta::FromSeconds(10);

// The number of loading slots for background tabs. TabManager will start to
// load the next background tab when the loading slots free up.
constexpr size_t kNumOfLoadingSlots = 1;

// The default interval in seconds after which to adjust the oom_score_adj
// value.
constexpr int kAdjustmentIntervalSeconds = 10;

struct LifecycleUnitAndSortKey {
  explicit LifecycleUnitAndSortKey(LifecycleUnit* lifecycle_unit)
      : lifecycle_unit(lifecycle_unit),
        sort_key(lifecycle_unit->GetSortKey()) {}

  bool operator<(const LifecycleUnitAndSortKey& other) const {
    return sort_key < other.sort_key;
  }
  bool operator>(const LifecycleUnitAndSortKey& other) const {
    return sort_key > other.sort_key;
  }

  LifecycleUnit* lifecycle_unit;
  LifecycleUnit::SortKey sort_key;
};

std::unique_ptr<base::trace_event::ConvertableToTraceFormat> DataAsTraceValue(
    TabManager::BackgroundTabLoadingMode mode,
    size_t num_of_pending_navigations,
    size_t num_of_loading_contents) {
  std::unique_ptr<base::trace_event::TracedValue> data(
      new base::trace_event::TracedValue());
  data->SetInteger("background_tab_loading_mode", mode);
  data->SetInteger("num_of_pending_navigations", num_of_pending_navigations);
  data->SetInteger("num_of_loading_contents", num_of_loading_contents);
  return std::move(data);
}

int GetNumLoadedLifecycleUnits(LifecycleUnitSet lifecycle_unit_set) {
  int num_loaded_lifecycle_units = 0;
  for (auto* lifecycle_unit : lifecycle_unit_set) {
    LifecycleUnitState state = lifecycle_unit->GetState();
    if (state != LifecycleUnitState::DISCARDED &&
        state != LifecycleUnitState::PENDING_DISCARD) {
      num_loaded_lifecycle_units++;
    }
  }
  return num_loaded_lifecycle_units;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TabManager

class TabManager::TabManagerSessionRestoreObserver final
    : public SessionRestoreObserver {
 public:
  explicit TabManagerSessionRestoreObserver(TabManager* tab_manager)
      : tab_manager_(tab_manager) {
    SessionRestore::AddObserver(this);
  }

  ~TabManagerSessionRestoreObserver() { SessionRestore::RemoveObserver(this); }

  // SessionRestoreObserver implementation:
  void OnSessionRestoreStartedLoadingTabs() override {
    tab_manager_->OnSessionRestoreStartedLoadingTabs();
  }

  void OnSessionRestoreFinishedLoadingTabs() override {
    tab_manager_->OnSessionRestoreFinishedLoadingTabs();
  }

  void OnWillRestoreTab(WebContents* web_contents) override {
    tab_manager_->OnWillRestoreTab(web_contents);
  }

 private:
  TabManager* tab_manager_;
};

constexpr base::TimeDelta TabManager::kDefaultMinTimeToPurge;

TabManager::TabManager()
    : state_transitions_callback_(
          base::BindRepeating(&TabManager::PerformStateTransitions,
                              base::Unretained(this))),
      browser_tab_strip_tracker_(this, nullptr, nullptr),
      is_session_restore_loading_tabs_(false),
      restored_tab_count_(0u),
      background_tab_loading_mode_(BackgroundTabLoadingMode::kStaggered),
      loading_slots_(kNumOfLoadingSlots),
      weak_ptr_factory_(this) {
#if defined(OS_CHROMEOS)
  delegate_.reset(new TabManagerDelegate(weak_ptr_factory_.GetWeakPtr()));
#endif
  browser_tab_strip_tracker_.Init();
  session_restore_observer_.reset(new TabManagerSessionRestoreObserver(this));
  if (PageSignalReceiver::IsEnabled()) {
    resource_coordinator_signal_observer_.reset(
        new ResourceCoordinatorSignalObserver());
  }
  stats_collector_.reset(new TabManagerStatsCollector());
  proactive_freeze_discard_params_ =
      GetStaticProactiveTabFreezeAndDiscardParams();
  TabLoadTracker::Get()->AddObserver(this);
  intervention_policy_database_.reset(new InterventionPolicyDatabase());

  // TabManager works in the absence of DesktopSessionDurationTracker for tests.
  if (metrics::DesktopSessionDurationTracker::IsInitialized())
    metrics::DesktopSessionDurationTracker::Get()->AddObserver(this);
}

TabManager::~TabManager() {
  TabLoadTracker::Get()->RemoveObserver(this);
  resource_coordinator_signal_observer_.reset();
  Stop();

  if (metrics::DesktopSessionDurationTracker::IsInitialized())
    metrics::DesktopSessionDurationTracker::Get()->RemoveObserver(this);
}

void TabManager::Start() {
  background_tab_loading_mode_ = BackgroundTabLoadingMode::kStaggered;

#if defined(OS_WIN) || defined(OS_MACOSX)
  // Note that discarding is now enabled by default. This check is kept as a
  // kill switch.
  // TODO(georgesak): remote this when deemed not needed anymore.
  if (!base::FeatureList::IsEnabled(features::kAutomaticTabDiscarding))
    return;
#endif

  if (!update_timer_.IsRunning()) {
    update_timer_.Start(FROM_HERE,
                        TimeDelta::FromSeconds(kAdjustmentIntervalSeconds),
                        this, &TabManager::UpdateTimerCallback);
  }

// MemoryPressureMonitor is not implemented on Linux so far and tabs are never
// discarded.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
  // Create a |MemoryPressureListener| to listen for memory events when
  // MemoryCoordinator is disabled. When MemoryCoordinator is enabled
  // it asks TabManager to do tab discarding.
  base::MemoryPressureMonitor* monitor = base::MemoryPressureMonitor::Get();
  if (monitor && !base::FeatureList::IsEnabled(features::kMemoryCoordinator)) {
    memory_pressure_listener_.reset(new base::MemoryPressureListener(
        base::Bind(&TabManager::OnMemoryPressure, base::Unretained(this))));
    base::MemoryPressureListener::MemoryPressureLevel level =
        monitor->GetCurrentPressureLevel();
    if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
      OnMemoryPressure(level);
    }
  }
#endif
  // purge-and-suspend param is used for Purge+Suspend finch experiment
  // in the following way:
  // https://docs.google.com/document/d/1hPHkKtXXBTlsZx9s-9U17XC-ofEIzPo9FYbBEc7PPbk/edit?usp=sharing
  std::string purge_and_suspend_time = variations::GetVariationParamValue(
      "PurgeAndSuspendAggressive", "purge-and-suspend-time");
  unsigned int min_time_to_purge_sec = 0;
  if (purge_and_suspend_time.empty() ||
      !base::StringToUint(purge_and_suspend_time, &min_time_to_purge_sec))
    min_time_to_purge_ = kDefaultMinTimeToPurge;
  else
    min_time_to_purge_ = base::TimeDelta::FromSeconds(min_time_to_purge_sec);

  std::string max_purge_and_suspend_time = variations::GetVariationParamValue(
      "PurgeAndSuspendAggressive", "max-purge-and-suspend-time");
  unsigned int max_time_to_purge_sec = 0;
  // If max-purge-and-suspend-time is not specified or
  // max-purge-and-suspend-time is not valid (not number or smaller than
  // min-purge-and-suspend-time), use default max-time-to-purge, i.e.
  // min-time-to-purge times kDefaultMinMaxTimeToPurgeRatio.
  if (max_purge_and_suspend_time.empty() ||
      !base::StringToUint(max_purge_and_suspend_time, &max_time_to_purge_sec) ||
      max_time_to_purge_sec < min_time_to_purge_.InSeconds())
    max_time_to_purge_ = min_time_to_purge_ * kDefaultMinMaxTimeToPurgeRatio;
  else
    max_time_to_purge_ = base::TimeDelta::FromSeconds(max_time_to_purge_sec);
}

void TabManager::Stop() {
  update_timer_.Stop();
  force_load_timer_.reset();
  memory_pressure_listener_.reset();
}

LifecycleUnitVector TabManager::GetSortedLifecycleUnits() {
  std::vector<LifecycleUnitAndSortKey> lifecycle_units_and_sort_keys;
  lifecycle_units_and_sort_keys.reserve(lifecycle_units_.size());
  for (auto* lifecycle_unit : lifecycle_units_)
    lifecycle_units_and_sort_keys.emplace_back(lifecycle_unit);
  std::sort(lifecycle_units_and_sort_keys.begin(),
            lifecycle_units_and_sort_keys.end());

  LifecycleUnitVector sorted_lifecycle_units;
  sorted_lifecycle_units.reserve(lifecycle_units_.size());
  for (auto& lifecycle_unit_and_sort_key : lifecycle_units_and_sort_keys) {
    sorted_lifecycle_units.push_back(
        lifecycle_unit_and_sort_key.lifecycle_unit);
  }

  return sorted_lifecycle_units;
}

void TabManager::DiscardTab(LifecycleUnitDiscardReason reason) {
  if (reason == LifecycleUnitDiscardReason::URGENT)
    stats_collector_->RecordWillDiscardUrgently(GetNumAliveTabs());

#if defined(OS_CHROMEOS)
  // Call Chrome OS specific low memory handling process.
  delegate_->LowMemoryKill(reason);
#else
  DiscardTabImpl(reason);
#endif  // defined(OS_CHROMEOS)
}

WebContents* TabManager::DiscardTabByExtension(content::WebContents* contents) {
  if (contents) {
    TabLifecycleUnitExternal* tab_lifecycle_unit_external =
        TabLifecycleUnitExternal::FromWebContents(contents);
    DCHECK(tab_lifecycle_unit_external);
    if (tab_lifecycle_unit_external->DiscardTab())
      return tab_lifecycle_unit_external->GetWebContents();
    return nullptr;
  }

  return DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL);
}

void TabManager::LogMemoryAndDiscardTab(LifecycleUnitDiscardReason reason) {
  // Discard immediately without waiting for LogMemory() (https://crbug/850545).
  // Consider removing LogMemory() at all if nobody cares about the log.
  LogMemory("Tab Discards Memory details");
  PurgeMemoryAndDiscardTab(reason);
}

void TabManager::LogMemory(const std::string& title) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  memory::OomMemoryDetails::Log(title);
}

void TabManager::AddObserver(TabLifecycleObserver* observer) {
  TabLifecycleUnitExternal::AddTabLifecycleObserver(observer);
}

void TabManager::RemoveObserver(TabLifecycleObserver* observer) {
  TabLifecycleUnitExternal::RemoveTabLifecycleObserver(observer);
}

bool TabManager::CanPurgeBackgroundedRenderer(int render_process_id) const {
  for (LifecycleUnit* lifecycle_unit : lifecycle_units_) {
    TabLifecycleUnitExternal* tab_lifecycle_unit_external =
        lifecycle_unit->AsTabLifecycleUnitExternal();
    // For now, all LifecycleUnits are TabLifecycleUnitExternals.
    DCHECK(tab_lifecycle_unit_external);
    content::WebContents* content =
        tab_lifecycle_unit_external->GetWebContents();
    DCHECK(content);

    if (content->IsCrashed())
      continue;
    if (content->GetMainFrame()->GetProcess()->GetID() != render_process_id)
      continue;
    if (!lifecycle_unit->CanPurge())
      return false;
  }
  return true;
}

size_t TabManager::GetBackgroundTabLoadingCount() const {
  if (!IsInBackgroundTabOpeningSession())
    return 0;

  return loading_contents_.size();
}

size_t TabManager::GetBackgroundTabPendingCount() const {
  if (!IsInBackgroundTabOpeningSession())
    return 0;

  return pending_navigations_.size();
}

int TabManager::GetTabCount() const {
  int tab_count = 0;
  for (auto* browser : *BrowserList::GetInstance())
    tab_count += browser->tab_strip_model()->count();
  return tab_count;
}

// static
bool TabManager::IsTabInSessionRestore(WebContents* web_contents) {
  return GetWebContentsData(web_contents)->is_in_session_restore();
}

// static
bool TabManager::IsTabRestoredInForeground(WebContents* web_contents) {
  return GetWebContentsData(web_contents)->is_restored_in_foreground();
}

///////////////////////////////////////////////////////////////////////////////
// TabManager, private:

// static
void TabManager::PurgeMemoryAndDiscardTab(LifecycleUnitDiscardReason reason) {
  TabManager* manager = g_browser_process->GetTabManager();
  manager->PurgeBrowserMemory();
  manager->DiscardTab(reason);
}

// static
bool TabManager::IsInternalPage(const GURL& url) {
  // There are many chrome:// UI URLs, but only look for the ones that users
  // are likely to have open. Most of the benefit is the from NTP URL.
  const char* const kInternalPagePrefixes[] = {
      chrome::kChromeUIDownloadsURL, chrome::kChromeUIHistoryURL,
      chrome::kChromeUINewTabURL, chrome::kChromeUISettingsURL};
  // Prefix-match against the table above. Use strncmp to avoid allocating
  // memory to convert the URL prefix constants into std::strings.
  for (size_t i = 0; i < arraysize(kInternalPagePrefixes); ++i) {
    if (!strncmp(url.spec().c_str(), kInternalPagePrefixes[i],
                 strlen(kInternalPagePrefixes[i])))
      return true;
  }
  return false;
}

void TabManager::PurgeBrowserMemory() {
  // Based on experimental evidence, attempts to free memory from renderers
  // have been too slow to use in OOM situations (V8 garbage collection) or
  // do not lead to persistent decreased usage (image/bitmap caches). This
  // function therefore only targets large blocks of memory in the browser.
  // Note that other objects will listen to MemoryPressureListener events
  // to release memory.
  for (auto* web_contents : AllTabContentses()) {
    // Screenshots can consume ~5 MB per web contents for platforms that do
    // touch back/forward.
    web_contents->GetController().ClearAllScreenshots();
  }
}

// This function is called when |update_timer_| fires. It will adjust the clock
// if needed (if it detects that the machine was asleep) and will fire the stats
// updating on ChromeOS via the delegate. This function also tries to purge
// cache memory.
void TabManager::UpdateTimerCallback() {
  // If Chrome is shutting down, do not do anything.
  if (g_browser_process->IsShuttingDown())
    return;

  if (BrowserList::GetInstance()->empty())
    return;

#if defined(OS_CHROMEOS)
  // This starts the CrOS specific OOM adjustments in /proc/<pid>/oom_score_adj.
  delegate_->AdjustOomPriorities();
#endif

  PurgeBackgroundedTabsIfNeeded();
}

base::TimeDelta TabManager::GetTimeToPurge(
    base::TimeDelta min_time_to_purge,
    base::TimeDelta max_time_to_purge) const {
  return base::TimeDelta::FromSeconds(base::RandInt(
      min_time_to_purge.InSeconds(), max_time_to_purge.InSeconds()));
}

bool TabManager::ShouldPurgeNow(content::WebContents* content) const {
  if (GetWebContentsData(content)->is_purged())
    return false;
  if (TabLifecycleUnitExternal::FromWebContents(content)->IsDiscarded())
    return false;

  base::TimeDelta time_passed =
      NowTicks() - GetWebContentsData(content)->LastInactiveTime();
  return time_passed > GetWebContentsData(content)->time_to_purge();
}

void TabManager::PurgeBackgroundedTabsIfNeeded() {
  for (LifecycleUnit* lifecycle_unit : lifecycle_units_) {
    TabLifecycleUnitExternal* tab_lifecycle_unit_external =
        lifecycle_unit->AsTabLifecycleUnitExternal();
    // For now, all LifecycleUnits are TabLifecycleUnitExternals.
    DCHECK(tab_lifecycle_unit_external);
    content::WebContents* content =
        tab_lifecycle_unit_external->GetWebContents();
    DCHECK(content);

    if (content->IsCrashed())
      continue;

    content::RenderProcessHost* render_process_host =
        content->GetMainFrame()->GetProcess();
    int render_process_id = render_process_host->GetID();

    if (!render_process_host->IsProcessBackgrounded())
      continue;
    if (!CanPurgeBackgroundedRenderer(render_process_id))
      continue;

    bool purge_now = ShouldPurgeNow(content);
    if (!purge_now)
      continue;

    // Since |content|'s tab is kept inactive and background for more than
    // time-to-purge time, its purged state changes: false => true.
    GetWebContentsData(content)->set_is_purged(true);
    // TODO(tasak): rename PurgeAndSuspend with a better name, e.g.
    // RequestPurgeCache, because we don't suspend any renderers.
    render_process_host->PurgeAndSuspend();
  }
}

void TabManager::PauseBackgroundTabOpeningIfNeeded() {
  TRACE_EVENT_INSTANT0("navigation",
                       "TabManager::PauseBackgroundTabOpeningIfNeeded",
                       TRACE_EVENT_SCOPE_THREAD);
  if (IsInBackgroundTabOpeningSession()) {
    stats_collector_->TrackPausedBackgroundTabs(pending_navigations_.size());
    stats_collector_->OnBackgroundTabOpeningSessionEnded();
  }

  background_tab_loading_mode_ = BackgroundTabLoadingMode::kPaused;
}

void TabManager::ResumeBackgroundTabOpeningIfNeeded() {
  TRACE_EVENT_INSTANT0("navigation",
                       "TabManager::ResumeBackgroundTabOpeningIfNeeded",
                       TRACE_EVENT_SCOPE_THREAD);
  background_tab_loading_mode_ = BackgroundTabLoadingMode::kStaggered;
  LoadNextBackgroundTabIfNeeded();

  if (IsInBackgroundTabOpeningSession())
    stats_collector_->OnBackgroundTabOpeningSessionStarted();
}

void TabManager::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  // If Chrome is shutting down, do not do anything.
  if (g_browser_process->IsShuttingDown())
    return;

  // TODO(crbug.com/762775): Pause or resume background tab opening based on
  // memory pressure signal after it becomes more reliable.
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      return;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      LogMemoryAndDiscardTab(LifecycleUnitDiscardReason::URGENT);
      return;
  }
  NOTREACHED();
  // TODO(skuhne): If more memory pressure levels are introduced, consider
  // calling PurgeBrowserMemory() before CRITICAL is reached.
}

void TabManager::OnActiveTabChanged(content::WebContents* old_contents,
                                    content::WebContents* new_contents) {
  // An active tab is not purged.
  // Calling GetWebContentsData() early ensures that the WebContentsData is
  // created for |new_contents|, which |stats_collector_| expects.
  GetWebContentsData(new_contents)->set_is_purged(false);

  // If |old_contents| is set, that tab has switched from being active to
  // inactive, so record the time of that transition.
  if (old_contents) {
    GetWebContentsData(old_contents)->SetLastInactiveTime(NowTicks());
    // Re-setting time-to-purge every time a tab becomes inactive.
    GetWebContentsData(old_contents)
        ->set_time_to_purge(
            GetTimeToPurge(min_time_to_purge_, max_time_to_purge_));
    // Only record switch-to-tab metrics when a switch happens, i.e.
    // |old_contents| is set.
    stats_collector_->RecordSwitchToTab(old_contents, new_contents);
  }

  ResumeTabNavigationIfNeeded(new_contents);
}

void TabManager::OnTabInserted(content::WebContents* contents,
                               bool foreground) {
  // Only interested in background tabs, as foreground tabs get taken care of by
  // OnActiveTabChanged.
  if (foreground)
    return;

  // A new background tab is similar to having a tab switch from being active to
  // inactive.
  GetWebContentsData(contents)->SetLastInactiveTime(NowTicks());
  // Re-setting time-to-purge every time a tab becomes inactive.
  GetWebContentsData(contents)->set_time_to_purge(
      GetTimeToPurge(min_time_to_purge_, max_time_to_purge_));
}

void TabManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted) {
    for (const auto& delta : change.deltas()) {
      OnTabInserted(delta.insert.contents,
                    delta.insert.contents == selection.new_contents);
    }
  } else if (change.type() == TabStripModelChange::kReplaced) {
    for (const auto& delta : change.deltas()) {
      WebContentsData::CopyState(delta.replace.old_contents,
                                 delta.replace.new_contents);
    }
  }

  if (selection.active_tab_changed() && !tab_strip_model->empty())
    OnActiveTabChanged(selection.old_contents, selection.new_contents);
}

void TabManager::OnStartTracking(content::WebContents* web_contents,
                                 LoadingState loading_state) {
  GetWebContentsData(web_contents)->SetTabLoadingState(loading_state);
}

void TabManager::OnLoadingStateChange(content::WebContents* web_contents,
                                      LoadingState old_loading_state,
                                      LoadingState new_loading_state) {
  GetWebContentsData(web_contents)->SetTabLoadingState(new_loading_state);

  if (new_loading_state == LoadingState::LOADED) {
    bool was_in_background_tab_opening_session =
        IsInBackgroundTabOpeningSession();

    loading_contents_.erase(web_contents);
    stats_collector_->OnTabIsLoaded(web_contents);
    LoadNextBackgroundTabIfNeeded();

    if (was_in_background_tab_opening_session &&
        !IsInBackgroundTabOpeningSession()) {
      stats_collector_->OnBackgroundTabOpeningSessionEnded();
    }

    // Once a tab is loaded, it might be eligible for freezing.
    SchedulePerformStateTransitions(base::TimeDelta());
  }
}

void TabManager::OnStopTracking(content::WebContents* web_contents,
                                LoadingState loading_state) {
  GetWebContentsData(web_contents)->SetTabLoadingState(loading_state);
}

void TabManager::OnSessionStarted(base::TimeTicks session_start) {
  // LifecycleUnits might become eligible for proactive discarding when Chrome
  // starts being used.
  SchedulePerformStateTransitions(base::TimeDelta());
}

// static
TabManager::WebContentsData* TabManager::GetWebContentsData(
    content::WebContents* contents) {
  WebContentsData::CreateForWebContents(contents);
  return WebContentsData::FromWebContents(contents);
}

// TODO(jamescook): This should consider tabs with references to other tabs,
// such as tabs created with JavaScript window.open(). Potentially consider
// discarding the entire set together, or use that in the priority computation.
content::WebContents* TabManager::DiscardTabImpl(
    LifecycleUnitDiscardReason reason) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (LifecycleUnit* lifecycle_unit : GetSortedLifecycleUnits()) {
    DecisionDetails decision_details;
    if (lifecycle_unit->CanDiscard(reason, &decision_details) &&
        lifecycle_unit->Discard(reason)) {
      TabLifecycleUnitExternal* tab_lifecycle_unit_external =
          lifecycle_unit->AsTabLifecycleUnitExternal();
      // For now, all LifecycleUnits are TabLifecycleUnitExternals.
      DCHECK(tab_lifecycle_unit_external);

      return tab_lifecycle_unit_external->GetWebContents();
    }
  }

  return nullptr;
}

void TabManager::OnSessionRestoreStartedLoadingTabs() {
  DCHECK(!is_session_restore_loading_tabs_);
  is_session_restore_loading_tabs_ = true;
}

void TabManager::OnSessionRestoreFinishedLoadingTabs() {
  DCHECK(is_session_restore_loading_tabs_);
  is_session_restore_loading_tabs_ = false;
  restored_tab_count_ = 0u;
}

void TabManager::OnWillRestoreTab(WebContents* contents) {
  WebContentsData* data = GetWebContentsData(contents);
  DCHECK(!data->is_in_session_restore());
  data->SetIsInSessionRestore(true);
  data->SetIsRestoredInForeground(contents->GetVisibility() !=
                                  content::Visibility::HIDDEN);
  restored_tab_count_++;

  // TabUIHelper is initialized in TabHelpers::AttachTabHelpers. But this place
  // gets called earlier than that. So for restored tabs, also initialize their
  // TabUIHelper here.
  TabUIHelper::CreateForWebContents(contents);
  TabUIHelper::FromWebContents(contents)->set_created_by_session_restore(true);
}

content::NavigationThrottle::ThrottleCheckResult
TabManager::MaybeThrottleNavigation(BackgroundTabNavigationThrottle* throttle) {
  content::WebContents* contents =
      throttle->navigation_handle()->GetWebContents();
  DCHECK_EQ(contents->GetVisibility(), content::Visibility::HIDDEN);

  // Skip delaying the navigation if this tab is in session restore, whose
  // loading is already controlled by TabLoader.
  if (GetWebContentsData(contents)->is_in_session_restore())
    return content::NavigationThrottle::PROCEED;

  if (background_tab_loading_mode_ == BackgroundTabLoadingMode::kStaggered &&
      !IsInBackgroundTabOpeningSession()) {
    stats_collector_->OnBackgroundTabOpeningSessionStarted();
  }

  stats_collector_->TrackNewBackgroundTab(pending_navigations_.size(),
                                          loading_contents_.size());

  if (!base::FeatureList::IsEnabled(
          features::kStaggeredBackgroundTabOpeningExperiment) ||
      CanLoadNextTab()) {
    loading_contents_.insert(contents);
    stats_collector_->TrackBackgroundTabLoadAutoStarted();
    return content::NavigationThrottle::PROCEED;
  }

  // Notify TabUIHelper that the navigation is delayed, so that the tab UI such
  // as favicon and title can be updated accordingly.
  TabUIHelper::FromWebContents(contents)->NotifyInitialNavigationDelayed(true);
  pending_navigations_.push_back(throttle);
  std::stable_sort(pending_navigations_.begin(), pending_navigations_.end(),
                   ComparePendingNavigations);

  TRACE_EVENT_INSTANT1(
      "navigation", "TabManager::MaybeThrottleNavigation",
      TRACE_EVENT_SCOPE_THREAD, "data",
      DataAsTraceValue(background_tab_loading_mode_,
                       pending_navigations_.size(), loading_contents_.size()));

  StartForceLoadTimer();
  return content::NavigationThrottle::DEFER;
}

bool TabManager::IsInBackgroundTabOpeningSession() const {
  if (background_tab_loading_mode_ != BackgroundTabLoadingMode::kStaggered)
    return false;

  return !(pending_navigations_.empty() && loading_contents_.empty());
}

bool TabManager::CanLoadNextTab() const {
  if (background_tab_loading_mode_ != BackgroundTabLoadingMode::kStaggered)
    return false;

  // TabManager can only load the next tab when the loading slots free up. The
  // loading slot limit can be exceeded when |force_load_timer_| fires or when
  // the user selects a background tab.
  if (loading_contents_.size() < loading_slots_)
    return true;

  return false;
}

void TabManager::OnDidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  auto it = pending_navigations_.begin();
  while (it != pending_navigations_.end()) {
    BackgroundTabNavigationThrottle* throttle = *it;
    if (throttle->navigation_handle() == navigation_handle) {
      TRACE_EVENT_INSTANT1("navigation", "TabManager::OnDidFinishNavigation",
                           TRACE_EVENT_SCOPE_THREAD,
                           "found_navigation_handle_to_remove", true);
      pending_navigations_.erase(it);
      break;
    }
    it++;
  }
}

void TabManager::OnWebContentsDestroyed(content::WebContents* contents) {
  bool was_in_background_tab_opening_session =
      IsInBackgroundTabOpeningSession();

  RemovePendingNavigationIfNeeded(contents);
  loading_contents_.erase(contents);
  stats_collector_->OnWebContentsDestroyed(contents);
  LoadNextBackgroundTabIfNeeded();

  if (was_in_background_tab_opening_session &&
      !IsInBackgroundTabOpeningSession()) {
    stats_collector_->OnBackgroundTabOpeningSessionEnded();
  }
}

void TabManager::StartForceLoadTimer() {
  TRACE_EVENT_INSTANT1(
      "navigation", "TabManager::StartForceLoadTimer", TRACE_EVENT_SCOPE_THREAD,
      "data",
      DataAsTraceValue(background_tab_loading_mode_,
                       pending_navigations_.size(), loading_contents_.size()));

  if (force_load_timer_)
    force_load_timer_->Stop();
  else
    force_load_timer_ = std::make_unique<base::OneShotTimer>(GetTickClock());

  force_load_timer_->Start(FROM_HERE,
                           GetTabLoadTimeout(kDefaultBackgroundTabLoadTimeout),
                           this, &TabManager::LoadNextBackgroundTabIfNeeded);
}

void TabManager::LoadNextBackgroundTabIfNeeded() {
  TRACE_EVENT_INSTANT2(
      "navigation", "TabManager::LoadNextBackgroundTabIfNeeded",
      TRACE_EVENT_SCOPE_THREAD, "is_force_load_timer_running",
      IsForceLoadTimerRunning(), "data",
      DataAsTraceValue(background_tab_loading_mode_,
                       pending_navigations_.size(), loading_contents_.size()));

  if (background_tab_loading_mode_ != BackgroundTabLoadingMode::kStaggered)
    return;

  // Do not load more background tabs until TabManager can load the next tab.
  // Ignore this constraint if the timer fires to force loading the next
  // background tab.
  if (IsForceLoadTimerRunning() && !CanLoadNextTab())
    return;

  if (pending_navigations_.empty())
    return;

  stats_collector_->OnWillLoadNextBackgroundTab(!IsForceLoadTimerRunning());
  BackgroundTabNavigationThrottle* throttle = pending_navigations_.front();
  pending_navigations_.erase(pending_navigations_.begin());
  ResumeNavigation(throttle);
  stats_collector_->TrackBackgroundTabLoadAutoStarted();

  StartForceLoadTimer();
}

void TabManager::ResumeTabNavigationIfNeeded(content::WebContents* contents) {
  BackgroundTabNavigationThrottle* throttle =
      RemovePendingNavigationIfNeeded(contents);
  if (throttle) {
    ResumeNavigation(throttle);
    stats_collector_->TrackBackgroundTabLoadUserInitiated();
  }
}

void TabManager::ResumeNavigation(BackgroundTabNavigationThrottle* throttle) {
  content::WebContents* contents =
      throttle->navigation_handle()->GetWebContents();
  loading_contents_.insert(contents);
  TabUIHelper::FromWebContents(contents)->NotifyInitialNavigationDelayed(false);

  throttle->ResumeNavigation();
}

BackgroundTabNavigationThrottle* TabManager::RemovePendingNavigationIfNeeded(
    content::WebContents* contents) {
  auto it = pending_navigations_.begin();
  while (it != pending_navigations_.end()) {
    BackgroundTabNavigationThrottle* throttle = *it;
    if (throttle->navigation_handle()->GetWebContents() == contents) {
      pending_navigations_.erase(it);
      return throttle;
    }
    it++;
  }
  return nullptr;
}

// static
bool TabManager::ComparePendingNavigations(
    const BackgroundTabNavigationThrottle* first,
    const BackgroundTabNavigationThrottle* second) {
  bool first_is_internal_page =
      IsInternalPage(first->navigation_handle()->GetURL());
  bool second_is_internal_page =
      IsInternalPage(second->navigation_handle()->GetURL());

  if (first_is_internal_page != second_is_internal_page)
    return !first_is_internal_page;

  return false;
}

int TabManager::GetNumAliveTabs() const {
  int tab_count = 0;
  for (auto* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    for (int index = 0; index < tab_strip_model->count(); ++index) {
      content::WebContents* contents = tab_strip_model->GetWebContentsAt(index);
      if (!TabLifecycleUnitExternal::FromWebContents(contents)->IsDiscarded())
        ++tab_count;
    }
  }

  tab_count -= pending_navigations_.size();
  DCHECK_GE(tab_count, 0);

  return tab_count;
}

bool TabManager::IsTabLoadingForTest(content::WebContents* contents) const {
  if (base::ContainsKey(loading_contents_, contents))
    return true;
  DCHECK_NE(LoadingState::LOADING,
            GetWebContentsData(contents)->tab_loading_state());
  return false;
}

bool TabManager::IsNavigationDelayedForTest(
    const content::NavigationHandle* navigation_handle) const {
  for (const auto* it : pending_navigations_) {
    if (it->navigation_handle() == navigation_handle)
      return true;
  }
  return false;
}

bool TabManager::IsForceLoadTimerRunning() const {
  return force_load_timer_ && force_load_timer_->IsRunning();
}

base::TimeDelta TabManager::GetTimeInBackgroundBeforeProactiveDiscard() const {
  // Exceed high threshold - in excessive state.
  if (num_loaded_lifecycle_units_ >=
      proactive_freeze_discard_params_.high_loaded_tab_count) {
    return base::TimeDelta();
  }

  // Exceed moderate threshold - in high state.
  if (num_loaded_lifecycle_units_ >=
      proactive_freeze_discard_params_.moderate_loaded_tab_count) {
    return proactive_freeze_discard_params_.high_occluded_timeout;
  }

  // Exceed low threshold - in moderate state.
  if (num_loaded_lifecycle_units_ >=
      proactive_freeze_discard_params_.low_loaded_tab_count) {
    return proactive_freeze_discard_params_.moderate_occluded_timeout;
  }

  // Didn't meet any thresholds - in low state.
  return proactive_freeze_discard_params_.low_occluded_timeout;
}

void TabManager::SchedulePerformStateTransitions(base::TimeDelta delay) {
  if (!state_transitions_timer_) {
    state_transitions_timer_ =
        std::make_unique<base::OneShotTimer>(GetTickClock());
  }

  state_transitions_timer_->Start(FROM_HERE, delay,
                                  state_transitions_callback_);
}

void TabManager::PerformStateTransitions() {
  if (!base::FeatureList::IsEnabled(features::kProactiveTabFreezeAndDiscard))
    return;

  base::TimeTicks next_state_transition_time = base::TimeTicks::Max();
  const base::TimeTicks now = NowTicks();
  LifecycleUnit* oldest_discardable_lifecycle_unit = nullptr;
  LifecycleUnit* oldest_frozen_lifecycle_unit = nullptr;

  for (LifecycleUnit* lifecycle_unit : lifecycle_units_) {
    // Maybe freeze the LifecycleUnit.
    next_state_transition_time =
        std::min(MaybeFreezeLifecycleUnit(lifecycle_unit, now),
                 next_state_transition_time);

    // Keep track of the discardable LifecycleUnit that has been hidden for the
    // longest time. It might be discarded below.
    DecisionDetails discard_details;
    if (lifecycle_unit->CanDiscard(LifecycleUnitDiscardReason::PROACTIVE,
                                   &discard_details)) {
      if (!oldest_discardable_lifecycle_unit ||
          lifecycle_unit->GetChromeUsageTimeWhenHidden() <
              oldest_discardable_lifecycle_unit
                  ->GetChromeUsageTimeWhenHidden()) {
        oldest_discardable_lifecycle_unit = lifecycle_unit;
      }
    }

    // Keep track of the LifecycleUnit that has been frozen for the longest
    // time. It might be unfrozen below.
    if (lifecycle_unit->GetState() == LifecycleUnitState::FROZEN &&
        (!oldest_frozen_lifecycle_unit ||
         lifecycle_unit->GetWallTimeWhenHidden() <
             oldest_frozen_lifecycle_unit->GetWallTimeWhenHidden())) {
      oldest_frozen_lifecycle_unit = lifecycle_unit;
    }
  }

  // Unfreeze the LifecycleUnit that has been frozen for the longest time if it
  // has been frozen long enough and a sufficient amount of time elapsed since
  // the last unfreeze.
  if (proactive_freeze_discard_params_.should_periodically_unfreeze &&
      oldest_frozen_lifecycle_unit) {
    next_state_transition_time =
        std::min(MaybeUnfreezeLifecycleUnit(oldest_frozen_lifecycle_unit, now),
                 next_state_transition_time);
  }

  // Proactively discard the LifecycleUnit that has been hidden for the longest
  // time if it at least GetTimeInBackgroundBeforeProactiveDiscard() of Chrome
  // usage time has elapsed since it was hidden.
  //
  // Note: Discarding a LifecycleUnit might change the value returned by
  // GetTimeInBackgroundBeforeProactiveDiscard(). Therefore, discard only the
  // oldest LifecycleUnit, rather than discarding all LifecycleUnits that have
  // been non-visible long enough. If a discard happens,
  // MaybeDiscardLifecycleUnit() returns a zero TimeTicks and another call to
  // PerformStateTransitions() is scheduled immediately to check if another
  // discard should happen.
  if (oldest_discardable_lifecycle_unit && ShouldProactivelyDiscardTabs()) {
    next_state_transition_time = std::min(
        MaybeDiscardLifecycleUnit(oldest_discardable_lifecycle_unit, now),
        next_state_transition_time);
  }

  // Schedule the next call to PerformStateTransitions().
  DCHECK(!state_transitions_timer_->IsRunning());
  if (!next_state_transition_time.is_max())
    SchedulePerformStateTransitions(next_state_transition_time - now);
}

base::TimeTicks TabManager::MaybeFreezeLifecycleUnit(
    LifecycleUnit* lifecycle_unit,
    base::TimeTicks now) {
  DecisionDetails freeze_details;
  if (!lifecycle_unit->CanFreeze(&freeze_details))
    return base::TimeTicks::Max();

  const base::TimeTicks freeze_time =
      std::max(lifecycle_unit->GetWallTimeWhenHidden() +
                   proactive_freeze_discard_params_.freeze_timeout,
               // Do not refreeze a tab before the refreeze timeout has expired.
               lifecycle_unit->GetStateChangeTime() +
                   proactive_freeze_discard_params_.refreeze_timeout);

  if (now >= freeze_time) {
    lifecycle_unit->Freeze();
    return base::TimeTicks::Max();
  }

  return freeze_time;
}

base::TimeTicks TabManager::MaybeUnfreezeLifecycleUnit(
    LifecycleUnit* lifecycle_unit,
    base::TimeTicks now) {
  DCHECK_EQ(lifecycle_unit->GetState(), LifecycleUnitState::FROZEN);

  const base::TimeTicks unfreeze_time = std::max(
      lifecycle_unit->GetStateChangeTime() +
          proactive_freeze_discard_params_.unfreeze_timeout,
      last_unfreeze_time_ + proactive_freeze_discard_params_.refreeze_timeout);

  if (now >= unfreeze_time) {
    last_unfreeze_time_ = now;
    lifecycle_unit->Unfreeze();
    return now + proactive_freeze_discard_params_.refreeze_timeout;
  }

  return unfreeze_time;
}

base::TimeTicks TabManager::MaybeDiscardLifecycleUnit(
    LifecycleUnit* lifecycle_unit,
    base::TimeTicks now) {
  const base::TimeDelta usage_time_not_visible =
      usage_clock_.GetTotalUsageTime() -
      lifecycle_unit->GetChromeUsageTimeWhenHidden();
  const base::TimeDelta time_until_discard =
      GetTimeInBackgroundBeforeProactiveDiscard() - usage_time_not_visible;

  if (time_until_discard <= base::TimeDelta()) {
    lifecycle_unit->Discard(LifecycleUnitDiscardReason::PROACTIVE);
    // Request another call to check if another discard should happen.
    return base::TimeTicks();
  }

  if (usage_clock_.IsInUse())
    return now + time_until_discard;

  return base::TimeTicks::Max();
}

void TabManager::OnLifecycleUnitStateChanged(
    LifecycleUnit* lifecycle_unit,
    LifecycleUnitState last_state,
    LifecycleUnitStateChangeReason reason) {
  LifecycleUnitState state = lifecycle_unit->GetState();
  bool was_discarded = (last_state == LifecycleUnitState::PENDING_DISCARD ||
                        last_state == LifecycleUnitState::DISCARDED);
  bool is_discarded = (state == LifecycleUnitState::PENDING_DISCARD ||
                       state == LifecycleUnitState::DISCARDED);

  if (is_discarded && !was_discarded) {
    num_loaded_lifecycle_units_--;
  } else if (was_discarded && !is_discarded) {
    num_loaded_lifecycle_units_++;
    // Incrementing the number of loaded tabs might change the return value of
    // GetTimeInBackgroundBeforeProactiveDiscard(). Schedule a call to
    // PerformStateTransitions() to determine if a tab should be discarded in
    // response to that change.
    SchedulePerformStateTransitions(base::TimeDelta());
  }

  DCHECK_EQ(num_loaded_lifecycle_units_,
            GetNumLoadedLifecycleUnits(lifecycle_units_));
}

void TabManager::OnLifecycleUnitVisibilityChanged(
    LifecycleUnit* lifecycle_unit,
    content::Visibility visibility) {
  SchedulePerformStateTransitions(base::TimeDelta());
}

void TabManager::OnLifecycleUnitDestroyed(LifecycleUnit* lifecycle_unit) {
  if (lifecycle_unit->GetState() != LifecycleUnitState::DISCARDED &&
      lifecycle_unit->GetState() != LifecycleUnitState::PENDING_DISCARD) {
    num_loaded_lifecycle_units_--;
  }
  lifecycle_units_.erase(lifecycle_unit);

  DCHECK_EQ(num_loaded_lifecycle_units_,
            GetNumLoadedLifecycleUnits(lifecycle_units_));

  SchedulePerformStateTransitions(base::TimeDelta());
}

void TabManager::OnLifecycleUnitCreated(LifecycleUnit* lifecycle_unit) {
  lifecycle_units_.insert(lifecycle_unit);
  if (lifecycle_unit->GetState() != LifecycleUnitState::DISCARDED)
    num_loaded_lifecycle_units_++;

  // Add an observer to be notified of destruction.
  lifecycle_unit->AddObserver(this);

  DCHECK_EQ(num_loaded_lifecycle_units_,
            GetNumLoadedLifecycleUnits(lifecycle_units_));

  SchedulePerformStateTransitions(base::TimeDelta());
}

bool TabManager::ShouldProactivelyDiscardTabs() {
  if (!proactive_freeze_discard_params_.should_proactively_discard)
    return false;

  // Don't proactively discard tabs while offline.
  if (net::NetworkChangeNotifier::IsOffline())
    return false;

  return true;
}

}  // namespace resource_coordinator
