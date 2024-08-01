// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/resource_coordinator/tab_manager.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_capture_indicator.h"
#include "chrome/browser/memory/oom_memory_details.h"
#include "chrome/browser/resource_coordinator/resource_coordinator_parts.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_manager_resource_coordinator_signal_observer.h"
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
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/performance_manager.h"
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
  void OnWillRestoreTab(WebContents* web_contents) override {
    tab_manager_->OnWillRestoreTab(web_contents);
  }

 private:
  raw_ptr<TabManager> tab_manager_;
};

TabManager::TabManager() {
  session_restore_observer_ =
      std::make_unique<TabManagerSessionRestoreObserver>(this);
}

TabManager::~TabManager() = default;

void TabManager::Start() {
  // Create the graph observer. This is the source of page almost idle data and
  // EQT measurements.
  // TODO(sebmarchand): Remove the "IsAvailable" check, or merge the TM into the
  // PM. The TM and PM must always exist together.
  if (performance_manager::PerformanceManager::IsAvailable()) {
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindOnce([](performance_manager::Graph* graph) {
          graph->PassToGraph(
              std::make_unique<TabManagerResourceCoordinatorSignalObserver>());
        }));
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
  DiscardTabImpl(reason, std::move(tab_discard_done));
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

void TabManager::AddObserver(TabLifecycleObserver* observer) {
  TabLifecycleUnitExternal::AddTabLifecycleObserver(observer);
}

void TabManager::RemoveObserver(TabLifecycleObserver* observer) {
  TabLifecycleUnitExternal::RemoveTabLifecycleObserver(observer);
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

void TabManager::OnWillRestoreTab(WebContents* contents) {
  // TabUIHelper is initialized in TabHelpers::AttachTabHelpers. But this place
  // gets called earlier than that. So for restored tabs, also initialize their
  // TabUIHelper here.
  TabUIHelper::CreateForWebContents(contents);
  TabUIHelper::FromWebContents(contents)->set_created_by_session_restore(true);
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
