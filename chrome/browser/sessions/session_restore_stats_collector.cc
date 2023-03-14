// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_stats_collector.h"

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace {

using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;

SessionRestoreStatsCollector* g_instance = nullptr;

// Returns the RenderWidgetHost associated with a WebContents.
RenderWidgetHost* GetRenderWidgetHost(WebContents* web_contents) {
  content::RenderWidgetHostView* render_widget_host_view =
      web_contents->GetRenderWidgetHostView();
  if (render_widget_host_view)
    return render_widget_host_view->GetRenderWidgetHost();
  return nullptr;
}

}  // namespace

SessionRestoreStatsCollector::TabLoaderStats::TabLoaderStats()
    : tab_count(0u),
      tab_first_paint_reason(PAINT_FINISHED_UMA_MAX) {}

SessionRestoreStatsCollector::SessionRestoreStatsCollector(
    const base::TimeTicks& restore_started,
    std::unique_ptr<StatsReportingDelegate> reporting_delegate)
    : non_restored_tab_painted_first_(false),
      hidden_or_occluded_tab_ignored_(false),
      restore_started_(restore_started),
      reporting_delegate_(std::move(reporting_delegate)) {
  DCHECK(!g_instance);
  g_instance = this;
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
  tab_loader_stats_.tab_count += tabs.size();
  for (const auto& tab : tabs) {
    RegisterObserverForTab(tab.contents());
  }

  // If we were not able to register observers for any tab, report stats.
  if (tracked_tabs_occluded_map_.empty())
    ReportStatsAndSelfDestroy();
}

void SessionRestoreStatsCollector::RenderWidgetHostVisibilityChanged(
    content::RenderWidgetHost* widget_host,
    bool became_visible) {
  if (became_visible)
    return;

  auto host_and_occluded_it = tracked_tabs_occluded_map_.find(widget_host);
  if (host_and_occluded_it == tracked_tabs_occluded_map_.end())
    return;

  host_and_occluded_it->second = true;
}

void SessionRestoreStatsCollector::RenderWidgetHostDidUpdateVisualProperties(
    content::RenderWidgetHost* widget_host) {
  // Only process this event if first paint hasn't been seen and this is a
  // paint of a tab that has not been hidden or occluded.
  if (!widget_host->GetView() || !widget_host->GetView()->IsShowing())
    return;

  auto host_and_occluded_it = tracked_tabs_occluded_map_.find(widget_host);
  if (host_and_occluded_it != tracked_tabs_occluded_map_.end()) {
    // Ignore first paint of a restored tab that was hidden or occluded
    // before first paint. If another restored tab is painted, its paint
    // time will be recorded.
    if (host_and_occluded_it->second) {
      hidden_or_occluded_tab_ignored_ = true;
      return;
    }
    // This is a paint for a tab that is explicitly being tracked so
    // update the statistics. Otherwise the host is for a tab that's not
    // being tracked. Thus some other tab has visibility and has rendered
    // and there's no point in tracking the time to first paint. This can
    // happen because the user opened a different tab or restored tabs
    // to an already existing browser and an existing tab was in the
    // foreground.
    base::TimeDelta time_to_paint = base::TimeTicks::Now() - restore_started_;
    tab_loader_stats_.foreground_tab_first_paint = time_to_paint;
  } else {
    non_restored_tab_painted_first_ = true;
  }

  ReportStatsAndSelfDestroy();
}

void SessionRestoreStatsCollector::RenderWidgetHostDestroyed(
    content::RenderWidgetHost* widget_host) {
  render_widget_host_observations_.RemoveObservation(widget_host);
  tracked_tabs_occluded_map_.erase(widget_host);

  if (tracked_tabs_occluded_map_.empty())
    ReportStatsAndSelfDestroy();
}

void SessionRestoreStatsCollector::RegisterObserverForTab(WebContents* tab) {
  content::RenderWidgetHost* rwh = GetRenderWidgetHost(tab);

  // If we don't have a RenderWidgetHost, we can't track paints for the tab so
  // ignore it.
  if (!rwh)
    return;

  // Assume that tabs do not start out hidden or occluded.
  auto result = tracked_tabs_occluded_map_.emplace(rwh, false);
  DCHECK(result.second);
  render_widget_host_observations_.AddObservation(rwh);
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
                               base::Milliseconds(100), base::Minutes(16), 50);

    std::string time_for_count = base::StringPrintf(
        "SessionRestore.ForegroundTabFirstPaint4_%u",
        static_cast<unsigned int>(tab_loader_stats.tab_count));
    base::HistogramBase* counter_for_count = base::Histogram::FactoryTimeGet(
        time_for_count, base::Milliseconds(100), base::Minutes(16), 50,
        base::Histogram::kUmaTargetedHistogramFlag);
    counter_for_count->AddTime(tab_loader_stats.foreground_tab_first_paint);
  }
  UMA_HISTOGRAM_ENUMERATION(
      "SessionRestore.ForegroundTabFirstPaint4.FinishReason",
      tab_loader_stats.tab_first_paint_reason, PAINT_FINISHED_UMA_MAX);
}
