// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_manager.h"

#include <stddef.h>

#include <algorithm>
#include <array>
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
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
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
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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

TabManager::TabManager() = default;
TabManager::~TabManager() = default;

void TabManager::Start() {
  // Create the graph observer. This is the source of page almost idle data and
  // EQT measurements.
  // TODO(sebmarchand): Remove the "IsAvailable" check, or merge the TM into the
  // PM. The TM and PM must always exist together.
  if (performance_manager::PerformanceManager::IsAvailable()) {
    performance_manager::Graph* graph =
        performance_manager::PerformanceManager::GetGraph();
    graph->PassToGraph(
        std::make_unique<TabManagerResourceCoordinatorSignalObserver>());
  }

  g_browser_process->resource_coordinator_parts()
      ->tab_lifecycle_unit_source()
      ->Start();
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

  return DiscardTabImpl(
      LifecycleUnitDiscardReason::EXTERNAL,
      performance_manager::policies::kNonVisiblePagesUrgentProtectionTime);
}

///////////////////////////////////////////////////////////////////////////////
// TabManager, private:

// static
bool TabManager::IsInternalPage(const GURL& url) {
  // There are many chrome:// UI URLs, but only look for the ones that users
  // are likely to have open. Most of the benefit is the from NTP URL.
  const auto kInternalPagePrefixes = std::to_array<const char*>({
      chrome::kChromeUIDownloadsURL,
      chrome::kChromeUIHistoryURL,
      chrome::kChromeUINewTabURL,
      chrome::kChromeUISettingsURL,
  });
  // Prefix-match against the table above.
  for (const char* prefix : kInternalPagePrefixes) {
    if (base::StartsWith(url.spec(), prefix)) {
      return true;
    }
  }
  return false;
}

content::WebContents* TabManager::DiscardTabImpl(
    LifecycleUnitDiscardReason reason,
    base::TimeDelta minimum_time_in_background_to_discard) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  performance_manager::Graph* graph =
      performance_manager::PerformanceManager::GetGraph();
  CHECK(graph);
  return performance_manager::policies::PageDiscardingHelper::GetFromGraph(
             graph)
      ->DiscardAPage(reason, minimum_time_in_background_to_discard)
      .first_content_after_discard;
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
