// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/memory/oom_memory_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
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
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "net/base/network_change_notifier.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/resource_coordinator/tab_manager_delegate_chromeos.h"
#endif

using base::TimeTicks;
using content::BrowserThread;
using content::WebContents;

namespace resource_coordinator {
namespace {

using LoadingState = TabLoadTracker::LoadingState;

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
  raw_ptr<TabManager> tab_manager_;
};

TabManager::TabManager(TabLoadTracker* tab_load_tracker)
    : browser_tab_strip_tracker_(this, nullptr),
      is_session_restore_loading_tabs_(false),
      restored_tab_count_(0u),
      tab_load_tracker_(tab_load_tracker) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  delegate_ =
      std::make_unique<TabManagerDelegate>(weak_ptr_factory_.GetWeakPtr());
#endif
  browser_tab_strip_tracker_.Init();
  session_restore_observer_ =
      std::make_unique<TabManagerSessionRestoreObserver>(this);

  stats_collector_ = std::make_unique<TabManagerStatsCollector>();
  tab_load_tracker_->AddObserver(this);
}

TabManager::~TabManager() {
  tab_load_tracker_->RemoveObserver(this);
}

void TabManager::Start() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  delegate_->StartPeriodicOOMScoreUpdate();
#endif

// MemoryPressureMonitor is not implemented on Linux so far and tabs are never
// discarded.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS)
  // Don't handle memory pressure events here if this is done by
  // PerformanceManager.
  if (!base::FeatureList::IsEnabled(
          performance_manager::features::
              kUrgentDiscardingFromPerformanceManager)) {
    // Create a |MemoryPressureListener| to listen for memory events when
    // MemoryCoordinator is disabled. When MemoryCoordinator is enabled
    // it asks TabManager to do tab discarding.
    base::MemoryPressureMonitor* monitor = base::MemoryPressureMonitor::Get();
    if (monitor) {
      RegisterMemoryPressureListener();
      base::MemoryPressureListener::MemoryPressureLevel level =
          monitor->GetCurrentPressureLevel();
      if (level ==
          base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
        OnMemoryPressure(level);
      }
    }
  }
#endif

  // Create the graph observer. This is the source of page almost idle data and
  // EQT measurements.
  // TODO(sebmarchand): Remove the "IsAvailable" check, or merge the TM into the
  // PM. The TM and PM must always exist together.
  if (performance_manager::PerformanceManagerImpl::IsAvailable()) {
    performance_manager::PerformanceManagerImpl::CallOnGraphImpl(
        FROM_HERE, base::BindOnce(
                       [](std::unique_ptr<ResourceCoordinatorSignalObserver>
                              rc_signal_observer,
                          performance_manager::GraphImpl* graph) {
                         graph->PassToGraph(std::move(rc_signal_observer));
                       },
                       std::make_unique<ResourceCoordinatorSignalObserver>(
                           weak_ptr_factory_.GetWeakPtr())));
  }

  g_browser_process->resource_coordinator_parts()
      ->tab_lifecycle_unit_source()
      ->Start();
}

LifecycleUnitVector TabManager::GetSortedLifecycleUnits() {
  LifecycleUnitVector sorted_lifecycle_units(lifecycle_units_.begin(),
                                             lifecycle_units_.end());
  // Sort lifecycle_units with ascending importance.
  std::sort(sorted_lifecycle_units.begin(), sorted_lifecycle_units.end(),
            [](LifecycleUnit* a, LifecycleUnit* b) {
              return a->GetSortKey() < b->GetSortKey();
            });
  return sorted_lifecycle_units;
}

void TabManager::DiscardTab(LifecycleUnitDiscardReason reason,
                            TabDiscardDoneCB tab_discard_done) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Call Chrome OS specific low memory handling process.
  delegate_->LowMemoryKill(reason, std::move(tab_discard_done));
#else
  DiscardTabImpl(reason, std::move(tab_discard_done));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

WebContents* TabManager::DiscardTabByExtension(content::WebContents* contents) {
  if (contents) {
    TabLifecycleUnitExternal* tab_lifecycle_unit_external =
        TabLifecycleUnitExternal::FromWebContents(contents);
    DCHECK(tab_lifecycle_unit_external);
    if (tab_lifecycle_unit_external->DiscardTab(
            LifecycleUnitDiscardReason::EXTERNAL)) {
      return tab_lifecycle_unit_external->GetWebContents();
    }
    return nullptr;
  }

  return DiscardTabImpl(LifecycleUnitDiscardReason::EXTERNAL);
}

void TabManager::DiscardTabFromMemoryPressure() {
  DCHECK(!base::FeatureList::IsEnabled(
      performance_manager::features::kUrgentDiscardingFromPerformanceManager));

#if BUILDFLAG(IS_CHROMEOS)
  // Output a log with per-process memory usage and number of file descriptors,
  // as well as GPU memory details. Discard happens without waiting for the log
  // (https://crbug.com/850545) Per comment at
  // https://crrev.com/c/chromium/src/+/1980282/3#message-d45cc354e7776d7e3d208e22dd2f6bbca3e9eae8,
  // this log is used to diagnose issues on ChromeOS. Do not output it on other
  // platforms since it is not used and data shows it can create IO thread hangs
  // (https://crbug.com/1040522).
  memory::OomMemoryDetails::Log("Tab Discards Memory details");
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Start handling memory pressure. Suppress further notifications before
  // completion in case a slow handler queues up multiple dispatches of this
  // method and inadvertently discards more than necessary tabs/apps in a burst.
  UnregisterMemoryPressureListener();

  TabDiscardDoneCB tab_discard_done(base::BindOnce(
      &TabManager::OnTabDiscardDone, weak_ptr_factory_.GetWeakPtr()));
  DiscardTab(LifecycleUnitDiscardReason::URGENT, std::move(tab_discard_done));
}

void TabManager::AddObserver(TabLifecycleObserver* observer) {
  TabLifecycleUnitExternal::AddTabLifecycleObserver(observer);
}

void TabManager::RemoveObserver(TabLifecycleObserver* observer) {
  TabLifecycleUnitExternal::RemoveTabLifecycleObserver(observer);
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
bool TabManager::IsInternalPage(const GURL& url) {
  // There are many chrome:// UI URLs, but only look for the ones that users
  // are likely to have open. Most of the benefit is the from NTP URL.
  const char* const kInternalPagePrefixes[] = {
      chrome::kChromeUIDownloadsURL, chrome::kChromeUIHistoryURL,
      chrome::kChromeUINewTabURL, chrome::kChromeUISettingsURL};
  // Prefix-match against the table above. Use strncmp to avoid allocating
  // memory to convert the URL prefix constants into std::strings.
  for (size_t i = 0; i < std::size(kInternalPagePrefixes); ++i) {
    if (!strncmp(url.spec().c_str(), kInternalPagePrefixes[i],
                 strlen(kInternalPagePrefixes[i])))
      return true;
  }
  return false;
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
      DiscardTabFromMemoryPressure();
      return;
  }
  NOTREACHED();
}

void TabManager::OnTabDiscardDone() {
  base::MemoryPressureMonitor* monitor = base::MemoryPressureMonitor::Get();
  if (!monitor)
    return;

  // Create a MemoryPressureListener instance to re-register to the observer.
  // Note that we've just finished handling memory pressure and async
  // tab/app discard might haven't taken effect yet. Don't check memory pressure
  // level or act on it, or we might over-discard tabs or apps.
  RegisterMemoryPressureListener();
}

void TabManager::RegisterMemoryPressureListener() {
  DCHECK(!memory_pressure_listener_);
  // Use sync memory pressure listener.
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&TabManager::OnMemoryPressure,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void TabManager::UnregisterMemoryPressureListener() {
  // Destroying the memory pressure listener to unregister from the observer.
  memory_pressure_listener_.reset();
}

void TabManager::OnActiveTabChanged(content::WebContents* old_contents,
                                    content::WebContents* new_contents) {
  // If |old_contents| is set, that tab has switched from being active to
  // inactive, so record the time of that transition.
  if (old_contents) {
    // Only record switch-to-tab metrics when a switch happens, i.e.
    // |old_contents| is set.
    stats_collector_->RecordSwitchToTab(old_contents, new_contents);
  }
}

void TabManager::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kReplaced) {
    auto* replace = change.GetReplace();
    WebContentsData::CopyState(replace->old_contents, replace->new_contents);
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
    stats_collector_->OnTabIsLoaded(web_contents);
  }
}

void TabManager::OnStopTracking(content::WebContents* web_contents,
                                LoadingState loading_state) {
  GetWebContentsData(web_contents)->SetTabLoadingState(loading_state);
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
    LifecycleUnitDiscardReason reason,
    TabDiscardDoneCB tab_discard_done) {
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

void TabManager::OnWebContentsDestroyed(content::WebContents* contents) {
  stats_collector_->OnWebContentsDestroyed(contents);
}

void TabManager::OnLifecycleUnitDestroyed(LifecycleUnit* lifecycle_unit) {
  lifecycle_units_.erase(lifecycle_unit);
}

void TabManager::OnLifecycleUnitCreated(LifecycleUnit* lifecycle_unit) {
  // Add an observer to be notified of destruction.
  lifecycle_unit->AddObserver(this);

  lifecycle_units_.insert(lifecycle_unit);
}

}  // namespace resource_coordinator
