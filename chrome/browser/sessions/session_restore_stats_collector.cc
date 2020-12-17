// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_stats_collector.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace {

using content::NavigationController;
using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::Source;
using content::WebContents;

SessionRestoreStatsCollector* g_instance = nullptr;

// Returns the RenderWidgetHostView associated with a NavigationController.
RenderWidgetHostView* GetRenderWidgetHostView(
    NavigationController* tab) {
  WebContents* web_contents = tab->GetWebContents();
  if (web_contents)
    return web_contents->GetRenderWidgetHostView();
  return nullptr;
}

// Returns the RenderWidgetHost associated with a NavigationController.
RenderWidgetHost* GetRenderWidgetHost(
    NavigationController* tab) {
  content::RenderWidgetHostView* render_widget_host_view =
      GetRenderWidgetHostView(tab);
  if (render_widget_host_view)
    return render_widget_host_view->GetRenderWidgetHost();
  return nullptr;
}

}  // namespace

SessionRestoreStatsCollector::TabLoaderStats::TabLoaderStats()
    : tab_count(0u),
      tab_first_paint_reason(PAINT_FINISHED_UMA_MAX) {}

SessionRestoreStatsCollector::TabState::TabState(
    NavigationController* controller)
    : controller(controller),
      was_hidden_or_occluded(false),
      observed_host(nullptr) {}

SessionRestoreStatsCollector::SessionRestoreStatsCollector(
    const base::TimeTicks& restore_started,
    std::unique_ptr<StatsReportingDelegate> reporting_delegate)
    : non_restored_tab_painted_first_(false),
      hidden_or_occluded_tab_ignored_(false),
      restore_started_(restore_started),
      reporting_delegate_(std::move(reporting_delegate)) {
  DCHECK(!g_instance);
  g_instance = this;

  registrar_.Add(
      this,
      content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,
      content::NotificationService::AllSources());
}

SessionRestoreStatsCollector::~SessionRestoreStatsCollector() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
SessionRestoreStatsCollector* SessionRestoreStatsCollector::GetOrCreateInstance(
    base::TimeTicks restore_started,
    std::unique_ptr<StatsReportingDelegate> reporting_delegate) {
  if (g_instance)
    return g_instance;
  return new SessionRestoreStatsCollector(restore_started,
                                          std::move(reporting_delegate));
}

void SessionRestoreStatsCollector::TrackTabs(
    const std::vector<SessionRestoreDelegate::RestoredTab>& tabs) {
  const base::TimeTicks now = base::TimeTicks::Now();
  tab_loader_stats_.tab_count += tabs.size();
  for (const auto& tab : tabs) {
    // Report the time since the tab was active. If the tab is visible the
    // last active time is right now, so report zero.
    base::TimeDelta time_since_active;
    if (tab.contents()->GetVisibility() != content::Visibility::VISIBLE)
      time_since_active = now - tab.contents()->GetLastActiveTime();
    reporting_delegate_->ReportTabTimeSinceActive(time_since_active);

    // Get the active navigation entry. Restored tabs should always have one.
    auto* controller = &tab.contents()->GetController();
    auto* nav_entry =
        controller->GetEntryAtIndex(controller->GetCurrentEntryIndex());
    DCHECK(nav_entry);

    RegisterForNotifications(controller);
  }
}

void SessionRestoreStatsCollector::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      // This happens when a tab has been closed. A tab can be in any state
      // when this occurs. Simply stop tracking the tab.
      WebContents* web_contents = Source<WebContents>(source).ptr();
      NavigationController* tab = &web_contents->GetController();
      RemoveTab(tab);
      break;
    }
    case content::
        NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES: {
      // This notification is across all tabs in the browser so notifications
      // will arrive for tabs that the collector is not explicitly tracking.

      // Only process this event if first paint hasn't been seen and this is a
      // paint of a tab that has not been hidden or occluded.
      RenderWidgetHost* render_widget_host =
          Source<RenderWidgetHost>(source).ptr();
      if (render_widget_host->GetView() &&
          render_widget_host->GetView()->IsShowing()) {
        TabState* tab_state = GetTabState(render_widget_host);
        if (tab_state) {
          // Ignore first paint of a restored tab that was hidden or occluded
          // before first paint. If another restored tab is painted, its paint
          // time will be recorded.
          if (tab_state->was_hidden_or_occluded) {
            hidden_or_occluded_tab_ignored_ = true;
            break;
          }
          // This is a paint for a tab that is explicitly being tracked so
          // update the statistics. Otherwise the host is for a tab that's not
          // being tracked. Thus some other tab has visibility and has rendered
          // and there's no point in tracking the time to first paint. This can
          // happen because the user opened a different tab or restored tabs
          // to an already existing browser and an existing tab was in the
          // foreground.
          base::TimeDelta time_to_paint =
              base::TimeTicks::Now() - restore_started_;
          tab_loader_stats_.foreground_tab_first_paint = time_to_paint;
        } else {
          non_restored_tab_painted_first_ = true;
        }

        ReportStatsAndSelfDestroy();
      }
      break;
    }
    default:
      NOTREACHED() << "Unknown notification received:" << type;
      break;
  }
}

void SessionRestoreStatsCollector::RenderWidgetHostVisibilityChanged(
    content::RenderWidgetHost* widget_host,
    bool became_visible) {
  auto* tab_state = GetTabState(widget_host);
  if (tab_state && !became_visible)
    tab_state->was_hidden_or_occluded = true;
}

void SessionRestoreStatsCollector::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  observer_.Remove(widget_host);

  auto* tab_state = GetTabState(widget_host);
  if (tab_state)
    tab_state->observed_host = nullptr;
}

void SessionRestoreStatsCollector::RemoveTab(NavigationController* tab) {
  // Stop observing this tab.
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    Source<WebContents>(tab->GetWebContents()));
  auto tab_it = tabs_tracked_.find(tab);
  DCHECK(tab_it != tabs_tracked_.end());
  TabState& tab_state = tab_it->second;
  if (tab_state.observed_host)
    observer_.Remove(tab_state.observed_host);

  // Remove the tab from the |tabs_tracked_| map.
  tabs_tracked_.erase(tab_it);

  // It is possible for all restored contents to be destroyed before a first
  // paint has arrived.
  if (tabs_tracked_.empty())
    ReportStatsAndSelfDestroy();
}

SessionRestoreStatsCollector::TabState*
SessionRestoreStatsCollector::RegisterForNotifications(
    NavigationController* tab) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 Source<WebContents>(tab->GetWebContents()));
  auto result = tabs_tracked_.insert(std::make_pair(tab, TabState(tab)));
  DCHECK(result.second);
  TabState* tab_state = &result.first->second;
  // Register for RenderWidgetHostVisibilityChanged notifications for this tab.
  tab_state->observed_host = GetRenderWidgetHost(tab);
  if (tab_state->observed_host)
    observer_.Add(tab_state->observed_host);
  return tab_state;
}

SessionRestoreStatsCollector::TabState*
SessionRestoreStatsCollector::GetTabState(NavigationController* tab) {
  // This lookup can fail because the call can arrive for tabs that have
  // already loaded (user forced) and are no longer tracked.
  auto it = tabs_tracked_.find(tab);
  if (it == tabs_tracked_.end())
    return nullptr;
  return &it->second;
}

SessionRestoreStatsCollector::TabState*
SessionRestoreStatsCollector::GetTabState(RenderWidgetHost* tab) {
  for (auto& pair : tabs_tracked_) {
    if (pair.second.observed_host == tab)
      return &pair.second;
  }
  // It's possible for this lookup to fail as paint events can be received for
  // tabs that aren't being tracked.
  return nullptr;
}

void SessionRestoreStatsCollector::ReportStatsAndSelfDestroy() {
  if (!tab_loader_stats_.foreground_tab_first_paint.is_zero()) {
    tab_loader_stats_.tab_first_paint_reason = PAINT_FINISHED_UMA_DONE;
  } else if (non_restored_tab_painted_first_) {
    tab_loader_stats_.tab_first_paint_reason =
        PAINT_FINISHED_NON_RESTORED_TAB_PAINTED_FIRST;
  } else if (hidden_or_occluded_tab_ignored_) {
    tab_loader_stats_.tab_first_paint_reason =
        PAINT_FINISHED_UMA_NO_COMPLETELY_VISIBLE_TABS;
  } else {
    tab_loader_stats_.tab_first_paint_reason = PAINT_FINISHED_UMA_NO_PAINT;
  }
  reporting_delegate_->ReportTabLoaderStats(tab_loader_stats_);

  delete this;
}

SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    UmaStatsReportingDelegate() = default;

void SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    ReportTabLoaderStats(const TabLoaderStats& tab_loader_stats) {
  if (!tab_loader_stats.foreground_tab_first_paint.is_zero()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("SessionRestore.ForegroundTabFirstPaint4",
                               tab_loader_stats.foreground_tab_first_paint,
                               base::TimeDelta::FromMilliseconds(100),
                               base::TimeDelta::FromMinutes(16), 50);

    std::string time_for_count = base::StringPrintf(
        "SessionRestore.ForegroundTabFirstPaint4_%u",
        static_cast<unsigned int>(tab_loader_stats.tab_count));
    base::HistogramBase* counter_for_count = base::Histogram::FactoryTimeGet(
        time_for_count, base::TimeDelta::FromMilliseconds(100),
        base::TimeDelta::FromMinutes(16), 50,
        base::Histogram::kUmaTargetedHistogramFlag);
    counter_for_count->AddTime(tab_loader_stats.foreground_tab_first_paint);
  }
  UMA_HISTOGRAM_ENUMERATION(
      "SessionRestore.ForegroundTabFirstPaint4.FinishReason",
      tab_loader_stats.tab_first_paint_reason, PAINT_FINISHED_UMA_MAX);
}

void SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    ReportTabTimeSinceActive(base::TimeDelta elapsed) {
  UMA_HISTOGRAM_CUSTOM_TIMES("SessionRestore.RestoredTab.TimeSinceActive",
                             elapsed, base::TimeDelta::FromSeconds(10),
                             base::TimeDelta::FromDays(7), 100);
}
