// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_stats_collector.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
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

// The enumeration values stored in the "SessionRestore.Actions" histogram.
enum SessionRestoreActionsUma {
  // Counts the total number of session restores that have occurred.
  SESSION_RESTORE_ACTIONS_UMA_INITIATED = 0,
  // Counts the number of session restores that have seen deferred tab loadings
  // for whatever reason (almost certainly due to memory pressure).
  SESSION_RESTORE_ACTIONS_UMA_DEFERRED_TABS = 1,
  // The size of this enum. Must be the last entry.
  SESSION_RESTORE_ACTIONS_UMA_MAX,
};

// Emits a SessionRestore.Actions UMA event.
void EmitUmaSessionRestoreActionEvent(SessionRestoreActionsUma action) {
  UMA_HISTOGRAM_ENUMERATION("SessionRestore.Actions", action,
                            SESSION_RESTORE_ACTIONS_UMA_MAX);
}

// The enumeration of values stored in the "SessionRestore.TabActions"
// histogram.
enum SessionRestoreTabActionsUma {
  // Incremented for each tab created in a session restore.
  SESSION_RESTORE_TAB_ACTIONS_UMA_TAB_CREATED = 0,
  // Incremented for each tab that session restore decides not to load.
  SESSION_RESTORE_TAB_ACTIONS_UMA_TAB_LOADING_DEFERRED = 1,
  // Incremented for each tab that is successfully loaded.
  SESSION_RESTORE_TAB_ACTIONS_UMA_TAB_LOADED = 2,
  // Incremented for each session-restore-deferred tab that is subsequently
  // loaded.
  SESSION_RESTORE_TAB_ACTIONS_UMA_DEFERRED_TAB_LOADED = 3,
  // Incremented for each tab that starts loading due to the session restore.
  SESSION_RESTORE_TAB_ACTIONS_UMA_TAB_LOAD_STARTED = 4,
  // The size of this enum. Must be the last entry.
  SESSION_RESTORE_TAB_ACTIONS_UMA_MAX,
};

// Emits a SessionRestore.TabActions UMA event.
void EmitUmaSessionRestoreTabActionEvent(SessionRestoreTabActionsUma action) {
  UMA_HISTOGRAM_ENUMERATION("SessionRestore.TabActions", action,
                            SESSION_RESTORE_TAB_ACTIONS_UMA_MAX);
}

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

// Determines if the RenderWidgetHostView associated with a given
// NavigationController is visible.
bool IsShowing(NavigationController* tab) {
  content::RenderWidgetHostView* render_widget_host_view =
      GetRenderWidgetHostView(tab);
  return render_widget_host_view && render_widget_host_view->IsShowing();
}

}  // namespace

SessionRestoreStatsCollector::TabLoaderStats::TabLoaderStats()
    : tab_count(0u),
      tabs_deferred(0u),
      tabs_load_started(0u),
      tabs_loaded(0u),
      tab_first_paint_reason(PAINT_FINISHED_UMA_MAX) {}

SessionRestoreStatsCollector::TabState::TabState(
    NavigationController* controller)
    : controller(controller),
      is_deferred(false),
      was_hidden_or_occluded(false),
      loading_state(TAB_IS_NOT_LOADING),
      observed_host(nullptr) {}

SessionRestoreStatsCollector::SessionRestoreStatsCollector(
    const base::TimeTicks& restore_started,
    std::unique_ptr<StatsReportingDelegate> reporting_delegate)
    : done_tracking_non_deferred_tabs_(false),
      got_first_foreground_load_(false),
      waiting_for_first_paint_(true),
      non_restored_tab_painted_first_(false),
      hidden_or_occluded_tab_ignored_(false),
      restore_started_(restore_started),
      waiting_for_load_tab_count_(0u),
      loading_tab_count_(0u),
      deferred_tab_count_(0u),
      tick_clock_(new base::DefaultTickClock()),
      reporting_delegate_(std::move(reporting_delegate)) {
  this_retainer_ = this;
}

SessionRestoreStatsCollector::~SessionRestoreStatsCollector() {
}

void SessionRestoreStatsCollector::TrackTabs(
    const std::vector<SessionRestoreDelegate::RestoredTab>& tabs) {
  // Anytime new tabs are added, they are immediately "non deferred".
  done_tracking_non_deferred_tabs_ = false;

  // If this is the first call to TrackTabs then start observing events.
  if (tab_loader_stats_.tab_count == 0) {
    registrar_.Add(
        this,
        content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,
        content::NotificationService::AllSources());
  }

  const base::TimeTicks now = base::TimeTicks::Now();
  tab_loader_stats_.tab_count += tabs.size();
  waiting_for_load_tab_count_ += tabs.size();
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

    // Report the site engagement score for the restored tab.
    auto* engagement_svc = SiteEngagementService::Get(
        Profile::FromBrowserContext(tab.contents()->GetBrowserContext()));
    double engagement =
        engagement_svc->GetDetails(nav_entry->GetURL()).total_score;
    reporting_delegate_->ReportTabSiteEngagementScore(engagement);

    TabState* tab_state = RegisterForNotifications(controller);
    // The tab might already be loading if it is active in a visible window.
    if (!controller->NeedsReload())
      MarkTabAsLoading(tab_state);
  }
}

void SessionRestoreStatsCollector::DeferTab(NavigationController* tab) {
  TabState* tab_state = GetTabState(tab);

  // If the tab is no longer being tracked it has already finished loading.
  // This can occur if the user forces the tab to load before the entire session
  // restore is over, and the TabLoader then decides it would defer loading of
  // that tab.
  if (!tab_state)
    return;

  // Mark this tab as deferred, if its still being tracked. A tab should not be
  // marked as deferred twice.
  DCHECK(!tab_state->is_deferred);
  tab_state->is_deferred = true;
  ++deferred_tab_count_;
  ++tab_loader_stats_.tabs_deferred;

  // A tab that didn't start loading before it was deferred is not to be
  // actively monitored for loading.
  if (tab_state->loading_state == TAB_IS_NOT_LOADING) {
    DCHECK_LT(0u, waiting_for_load_tab_count_);
    if (--waiting_for_load_tab_count_ == 0)
      ReleaseIfDoneTracking();
  }

  reporting_delegate_->ReportTabDeferred();
}

void SessionRestoreStatsCollector::OnWillLoadNextTab(bool timeout) {
  UMA_HISTOGRAM_BOOLEAN("SessionRestore.TabLoadTimeout", timeout);
}

void SessionRestoreStatsCollector::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_START: {
      // This occurs when a tab has started to load. This can be because of
      // the tab loader (only for non-deferred tabs) or because the user clicked
      // on the tab.
      NavigationController* tab = Source<NavigationController>(source).ptr();
      TabState* tab_state = GetTabState(tab);
      MarkTabAsLoading(tab_state);
      break;
    }
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      // This happens when a tab has been closed. A tab can be in any state
      // when this occurs. Simply stop tracking the tab.
      WebContents* web_contents = Source<WebContents>(source).ptr();
      NavigationController* tab = &web_contents->GetController();
      RemoveTab(tab);
      break;
    }
    case content::NOTIFICATION_LOAD_STOP: {
      // This occurs to loading tabs when they have finished loading. The tab
      // may or may not already have painted at this point.

      // Update the tab state and any global state as necessary.
      NavigationController* tab = Source<NavigationController>(source).ptr();
      TabState* tab_state = GetTabState(tab);
      DCHECK(tab_state);
      tab_state->loading_state = TAB_IS_LOADED;
      DCHECK_LT(0u, loading_tab_count_);
      --loading_tab_count_;
      if (!tab_state->is_deferred) {
        DCHECK_LT(0u, waiting_for_load_tab_count_);
        --waiting_for_load_tab_count_;
      }

      if (tab_state->is_deferred) {
        reporting_delegate_->ReportDeferredTabLoaded();
      } else {
        DCHECK(!done_tracking_non_deferred_tabs_);
        ++tab_loader_stats_.tabs_loaded;
      }

      // Update statistics for foreground tabs.
      base::TimeDelta time_to_load = tick_clock_->NowTicks() - restore_started_;
      if (!got_first_foreground_load_ && IsShowing(tab_state->controller)) {
        got_first_foreground_load_ = true;
        DCHECK(!done_tracking_non_deferred_tabs_);
        tab_loader_stats_.foreground_tab_first_loaded = time_to_load;
      }

      // Update statistics for all tabs, if this wasn't a deferred tab. This is
      // done here and not in ReleaseIfDoneTracking because it is possible to
      // wait for a paint long after all loads have completed.
      if (!done_tracking_non_deferred_tabs_ && !tab_state->is_deferred)
        tab_loader_stats_.non_deferred_tabs_loaded = time_to_load;

      // By default tabs transition to being tracked for paint events after the
      // load event has been seen. However, if the first paint event has already
      // been seen then this is not necessary and the tab can be removed.
      if (!waiting_for_first_paint_)
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
      if (waiting_for_first_paint_ && render_widget_host->GetView() &&
          render_widget_host->GetView()->IsShowing()) {
        TabState* tab_state = GetTabState(render_widget_host);
        if (tab_state) {
          // Ignore first paint of a restored tab that was hidden or occluded
          // before first paint. If an other restored tab is painted, its
          // paint time will be recorded.
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
              tick_clock_->NowTicks() - restore_started_;
          DCHECK(!done_tracking_non_deferred_tabs_);
          tab_loader_stats_.foreground_tab_first_paint = time_to_paint;
        } else {
          non_restored_tab_painted_first_ = true;
        }
        waiting_for_first_paint_ = false;

        // Once first paint has been observed the entire to-paint tracking
        // mechanism is no longer needed.
        registrar_.Remove(
            this,
            content::
                NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,
            content::NotificationService::AllSources());

        // Remove any tabs that have loaded. These were only being kept around
        // while waiting for a paint event.
        std::vector<NavigationController*> loaded_tabs;
        for (auto& map_entry : tabs_tracked_) {
          TabState& tab_state = map_entry.second;
          if (tab_state.loading_state == TAB_IS_LOADED)
            loaded_tabs.push_back(tab_state.controller);
        }
        for (auto* tab : loaded_tabs)
          RemoveTab(tab);
      }
      break;
    }
    default:
      NOTREACHED() << "Unknown notification received:" << type;
      break;
  }

  ReleaseIfDoneTracking();
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
  auto* tab_state = GetTabState(widget_host);
  if (tab_state) {
    observer_.Remove(tab_state->observed_host);
    tab_state->observed_host = nullptr;
  }
}

void SessionRestoreStatsCollector::RemoveTab(NavigationController* tab) {
  // Stop observing this tab.
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    Source<WebContents>(tab->GetWebContents()));
  registrar_.Remove(this, content::NOTIFICATION_LOAD_STOP,
                    Source<NavigationController>(tab));
  registrar_.Remove(this, content::NOTIFICATION_LOAD_START,
                    Source<NavigationController>(tab));
  auto tab_it = tabs_tracked_.find(tab);
  DCHECK(tab_it != tabs_tracked_.end());
  TabState& tab_state = tab_it->second;
  if (tab_state.observed_host)
    observer_.Remove(tab_state.observed_host);

  // If this tab was waiting for a NOTIFICATION_LOAD_STOP event then update
  // the loading counts.
  if (tab_state.loading_state == TAB_IS_LOADING) {
    DCHECK_LT(0u, loading_tab_count_);
    --loading_tab_count_;
  }

  // Only non-deferred not-loading/not-loaded tabs are waiting to be loaded.
  if (tab_state.loading_state != TAB_IS_LOADED && !tab_state.is_deferred) {
    DCHECK_LT(0u, waiting_for_load_tab_count_);
    // It's possible for waiting_for_load_tab_count_ to reach zero here. This
    // function is only called from 'Observe', so the transition will be
    // noticed there.
    --waiting_for_load_tab_count_;
  }

  if (tab_state.is_deferred)
    --deferred_tab_count_;

  // Remove the tab from the tabs_tracked_| map.
  tabs_tracked_.erase(tab_it);

  // It is possible for all restored contents to be destroyed or forcibly
  // renavigated before a first paint has arrived. This can be detected by
  // tabs_tracked_ containing only deferred tabs. At this point the paint
  // mechanism can be disabled and stats collection will stop.
  if (tabs_tracked_.size() == deferred_tab_count_ && waiting_for_first_paint_) {
    waiting_for_first_paint_ = false;
    registrar_.Remove(
        this,
        content::NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES,
        content::NotificationService::AllSources());
  }
}

SessionRestoreStatsCollector::TabState*
SessionRestoreStatsCollector::RegisterForNotifications(
    NavigationController* tab) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 Source<WebContents>(tab->GetWebContents()));
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 Source<NavigationController>(tab));
  registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                 Source<NavigationController>(tab));
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
  // This lookup can fail because DeferTab calls can arrive for tabs that have
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

void SessionRestoreStatsCollector::MarkTabAsLoading(TabState* tab_state) {
  // If the tab has already started or finished loading then a user navigation
  // has caused the tab to be forcibly reloaded. This tab can be removed from
  // observation.
  if (tab_state->loading_state == TAB_IS_LOADED) {
    RemoveTab(tab_state->controller);
    return;
  }

  DCHECK_EQ(TAB_IS_NOT_LOADING, tab_state->loading_state);
  if (tab_state->loading_state != TAB_IS_NOT_LOADING)
    return;
  tab_state->loading_state = TAB_IS_LOADING;
  ++loading_tab_count_;

  if (!done_tracking_non_deferred_tabs_)
    ++tab_loader_stats_.tabs_load_started;
}

void SessionRestoreStatsCollector::ReleaseIfDoneTracking() {
  // If non-deferred tabs are no longer being tracked then report tab loader
  // statistics.
  if (!done_tracking_non_deferred_tabs_ && !waiting_for_first_paint_ &&
      waiting_for_load_tab_count_ == 0) {
    done_tracking_non_deferred_tabs_ = true;
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
  }

  // If tracking is completely finished then emit collected metrics and destroy
  // this stats collector.
  if (done_tracking_non_deferred_tabs_ && tabs_tracked_.empty())
    this_retainer_ = nullptr;
}

SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    UmaStatsReportingDelegate()
    : got_report_tab_deferred_(false) {
}

void SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    ReportTabLoaderStats(const TabLoaderStats& tab_loader_stats) {
  UMA_HISTOGRAM_COUNTS_100("SessionRestore.TabCount",
                           tab_loader_stats.tab_count);

  // Emit suffix specializations of the SessionRestore.TabCount metric.
  if (tab_loader_stats.tabs_deferred) {
    UMA_HISTOGRAM_COUNTS_100("SessionRestore.TabCount_MemoryPressure",
                             tab_loader_stats.tab_count);
    UMA_HISTOGRAM_COUNTS_100("SessionRestore.TabCount_MemoryPressure_Loaded",
                             tab_loader_stats.tabs_loaded);
    UMA_HISTOGRAM_COUNTS_100(
        "SessionRestore.TabCount_MemoryPressure_LoadStarted",
        tab_loader_stats.tabs_load_started);
    UMA_HISTOGRAM_COUNTS_100("SessionRestore.TabCount_MemoryPressure_Deferred",
                             tab_loader_stats.tabs_deferred);
  } else {
    UMA_HISTOGRAM_COUNTS_100("SessionRestore.TabCount_NoMemoryPressure",
                             tab_loader_stats.tab_count);
    UMA_HISTOGRAM_COUNTS_100("SessionRestore.TabCount_NoMemoryPressure_Loaded",
                             tab_loader_stats.tabs_loaded);
    UMA_HISTOGRAM_COUNTS_100(
        "SessionRestore.TabCount_NoMemoryPressure_LoadStarted",
        tab_loader_stats.tabs_load_started);
  }

  EmitUmaSessionRestoreActionEvent(SESSION_RESTORE_ACTIONS_UMA_INITIATED);

  for (size_t i = 0; i < tab_loader_stats.tab_count; ++i) {
    EmitUmaSessionRestoreTabActionEvent(
        SESSION_RESTORE_TAB_ACTIONS_UMA_TAB_CREATED);
  }

  for (size_t i = 0; i < tab_loader_stats.tabs_loaded; ++i) {
    EmitUmaSessionRestoreTabActionEvent(
        SESSION_RESTORE_TAB_ACTIONS_UMA_TAB_LOADED);
  }

  for (size_t i = 0; i < tab_loader_stats.tabs_load_started; ++i) {
    EmitUmaSessionRestoreTabActionEvent(
        SESSION_RESTORE_TAB_ACTIONS_UMA_TAB_LOAD_STARTED);
  }

  if (!tab_loader_stats.foreground_tab_first_loaded.is_zero()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("SessionRestore.ForegroundTabFirstLoaded",
                               tab_loader_stats.foreground_tab_first_loaded,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromSeconds(100), 100);

    // Record a time for the number of tabs, to help track down contention.
    std::string time_for_count = base::StringPrintf(
        "SessionRestore.ForegroundTabFirstLoaded_%u",
        static_cast<unsigned int>(tab_loader_stats.tab_count));
    base::HistogramBase* counter_for_count = base::Histogram::FactoryTimeGet(
        time_for_count, base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromSeconds(100), 100,
        base::Histogram::kUmaTargetedHistogramFlag);
    counter_for_count->AddTime(tab_loader_stats.foreground_tab_first_loaded);
  }

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

  if (!tab_loader_stats.non_deferred_tabs_loaded.is_zero()) {
    UMA_HISTOGRAM_CUSTOM_TIMES("SessionRestore.AllTabsLoaded",
                               tab_loader_stats.non_deferred_tabs_loaded,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromSeconds(100), 100);

    // Record a time for the number of tabs, to help track down contention.
    std::string time_for_count = base::StringPrintf(
        "SessionRestore.AllTabsLoaded_%u",
        static_cast<unsigned int>(tab_loader_stats.tab_count));
    base::HistogramBase* counter_for_count = base::Histogram::FactoryTimeGet(
        time_for_count, base::TimeDelta::FromMilliseconds(10),
        base::TimeDelta::FromSeconds(100), 100,
        base::Histogram::kUmaTargetedHistogramFlag);
    counter_for_count->AddTime(tab_loader_stats.non_deferred_tabs_loaded);
  }
}

void SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    ReportTabDeferred() {
  if (!got_report_tab_deferred_) {
    got_report_tab_deferred_ = true;
    EmitUmaSessionRestoreActionEvent(SESSION_RESTORE_ACTIONS_UMA_DEFERRED_TABS);
  }

  EmitUmaSessionRestoreTabActionEvent(
      SESSION_RESTORE_TAB_ACTIONS_UMA_TAB_LOADING_DEFERRED);
}

void SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    ReportDeferredTabLoaded() {
  EmitUmaSessionRestoreTabActionEvent(
      SESSION_RESTORE_TAB_ACTIONS_UMA_DEFERRED_TAB_LOADED);
}

void SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    ReportTabTimeSinceActive(base::TimeDelta elapsed) {
  UMA_HISTOGRAM_CUSTOM_TIMES("SessionRestore.RestoredTab.TimeSinceActive",
                             elapsed, base::TimeDelta::FromSeconds(10),
                             base::TimeDelta::FromDays(7), 100);
}

void SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    ReportTabSiteEngagementScore(double engagement) {
  // This metric uses the same reporting format (no rounding, histogram shape)
  // as the equivalent SiteEngagementService.EngagementScore. See
  // site_engagement_metrics.cc for details.
  UMA_HISTOGRAM_COUNTS_100("SessionRestore.RestoredTab.SiteEngagementScore",
                           engagement);
}
