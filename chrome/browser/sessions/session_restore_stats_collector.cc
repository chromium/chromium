// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_restore_stats_collector.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/session_restore_policy.h"
#include "components/performance_manager/public/decorators/site_data_recorder.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/persistence/site_data/feature_usage.h"
#include "components/performance_manager/public/persistence/site_data/site_data_reader.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using content::RenderWidgetHost;
using content::RenderWidgetHostView;
using content::WebContents;
using performance_manager::PerformanceManager;
using performance_manager::SiteDataReader;
using performance_manager::SiteFeatureUsage;

SessionRestoreStatsCollector* g_instance = nullptr;

// Returns the RenderWidgetHost associated with a WebContents.
RenderWidgetHost* GetRenderWidgetHost(WebContents* web_contents) {
  content::RenderWidgetHostView* render_widget_host_view =
      web_contents->GetRenderWidgetHostView();
  if (render_widget_host_view)
    return render_widget_host_view->GetRenderWidgetHost();
  return nullptr;
}

// Returns the site engagement score of a WebContents. Copied from
// chrome/browser/resource_coordinator/session_restore_policy.cc.
size_t GetSiteEngagementScore(content::WebContents* contents) {
  // Get the active navigation entry. Restored tabs should always have one.
  auto& controller = contents->GetController();
  auto* nav_entry =
      controller.GetEntryAtIndex(controller.GetCurrentEntryIndex());
  DCHECK(nav_entry);

  auto* engagement_svc = site_engagement::SiteEngagementService::Get(
      Profile::FromBrowserContext(contents->GetBrowserContext()));
  double engagement =
      engagement_svc->GetDetails(nav_entry->GetURL()).total_score;

  // Return the engagement as an integer.
  return engagement;
}

bool HasLowSiteEngagement(content::WebContents* contents) {
  // There are 3 ways to handle session restore:
  //
  // Case 1: kBackgroundTabLoadingFromPerformanceManager is disabled.
  //
  // The algorithm uses SessionRestorePolicy::kMinSiteEngagementToRestore.
  // SessionRestore.TabCount.LowSiteEngagement counts tabs that are less than
  // this.
  //
  // Case 2: kBackgroundTabLoadingFromPerformanceManager is enabled, and uses
  // a min value from kBackgroundTabLoadingMinSiteEngagement.
  //
  // SessionRestore.TabCount.LowSiteEngagement counts tabs that are less than
  // kBackgroundTabLoadingMinSiteEngagement.
  //
  // Case 3: kBackgroundTabLoadingFromPerformanceManager is enabled, but
  // kBackgroundTabLoadingMinSiteEngagement returns 0.
  //
  // The session restore algorithm ignores site engagement.
  // SessionRestore.TabCount.LowSiteEngagement counts tabs that are less than
  // SessionRestorePolicy::kMinSiteEngagementToRestore, to see what data the PM
  // algorithm is ignoring.
  const size_t min_site_engagement =
      performance_manager::features::kBackgroundTabLoadingMinSiteEngagement
          .Get();
  if (min_site_engagement == 0) {
    return GetSiteEngagementScore(contents) <
           resource_coordinator::SessionRestorePolicy::
               kMinSiteEngagementToRestore;
  }
  return GetSiteEngagementScore(contents) < min_site_engagement;
}

bool HasNotificationPermission(content::WebContents* contents) {
  return contents->GetBrowserContext()
             ->GetPermissionController()
             ->GetPermissionResultForOriginWithoutContext(
                 content::PermissionDescriptorUtil::
                     CreatePermissionDescriptorForPermissionType(
                         blink::PermissionType::NOTIFICATIONS),
                 url::Origin::Create(contents->GetLastCommittedURL()))
             .status == blink::mojom::PermissionStatus::GRANTED;
}

void LogFirstPaintHistogram(base::TimeDelta paint_time,
                            std::string_view suffix = "") {
  base::UmaHistogramCustomTimes(
      base::StrCat({"SessionRestore.ForegroundTabFirstPaint4", suffix}),
      paint_time, base::Milliseconds(100), base::Minutes(16), 50);
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
    if (tab.is_active()) {
      tab_loader_stats_.active_tab_count++;
    }
    if (tab.is_app()) {
      tab_loader_stats_.app_tab_count++;
    }
    if (tab.is_internal_page()) {
      tab_loader_stats_.internal_page_tab_count++;
    }
    if (tab.is_pinned()) {
      tab_loader_stats_.pinned_tab_count++;
    }
    if (tab.group().has_value()) {
      tab_loader_stats_.grouped_tab_count++;
    }

    // Look up background feature use in `site_data_reader` on the PM sequence,
    // and post the result to OnTabUpdatesInBackground() on the current
    // sequence, along with site engagement and permissions info.
    base::OnceCallback<bool(const SiteDataReader&)> on_site_data_ready =
        base::BindOnce([](const SiteDataReader& site_data_reader) {
          return site_data_reader.UpdatesFaviconInBackground() ==
                     SiteFeatureUsage::kSiteFeatureInUse ||
                 site_data_reader.UpdatesTitleInBackground() ==
                     SiteFeatureUsage::kSiteFeatureInUse;
        });

    base::OnceCallback<void(bool)> on_tab_updates_in_background =
        base::BindOnce(&SessionRestoreStatsCollector::OnTabUpdatesInBackground,
                       weak_factory_.GetWeakPtr(),
                       HasLowSiteEngagement(tab.contents()),
                       HasNotificationPermission(tab.contents()));

    performance_manager::WaitForSiteDataReader(
        PerformanceManager::GetPrimaryPageNodeForWebContents(tab.contents()),
        std::move(on_site_data_ready)
            .Then(std::move(on_tab_updates_in_background)));
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

void SessionRestoreStatsCollector::OnTabUpdatesInBackground(
    bool low_site_engagement,
    bool background_notification_permission,
    bool updates_in_background) {
  if (background_notification_permission) {
    tab_loader_stats_.notification_permission_tab_count++;
  }
  if (updates_in_background) {
    tab_loader_stats_.updates_in_background_tab_count++;
  }
  if (low_site_engagement) {
    tab_loader_stats_.low_site_engagement_tab_count++;
    if (background_notification_permission || updates_in_background) {
      tab_loader_stats_
          .low_site_engagement_with_background_communication_tab_count++;
    }
  }
}

SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    UmaStatsReportingDelegate() = default;

void SessionRestoreStatsCollector::UmaStatsReportingDelegate::
    ReportTabLoaderStats(const TabLoaderStats& tab_loader_stats) {
  if (!tab_loader_stats.foreground_tab_first_paint.is_zero()) {
    LogFirstPaintHistogram(tab_loader_stats.foreground_tab_first_paint);
    std::string_view count_suffix;
    CHECK_GT(tab_loader_stats.tab_count, 0u);
    if (tab_loader_stats.tab_count < 2) {
      count_suffix = ".1Tab";
    } else if (tab_loader_stats.tab_count < 4) {
      count_suffix = ".2to3Tabs";
    } else if (tab_loader_stats.tab_count < 8) {
      count_suffix = ".4to7Tabs";
    } else if (tab_loader_stats.tab_count < 16) {
      count_suffix = ".8to15Tabs";
    } else if (tab_loader_stats.tab_count < 32) {
      count_suffix = ".16to31Tabs";
    } else {
      count_suffix = ".32PlusTabs";
    }
    LogFirstPaintHistogram(tab_loader_stats.foreground_tab_first_paint,
                           count_suffix);
  }
  base::UmaHistogramEnumeration(
      "SessionRestore.ForegroundTabFirstPaint4.FinishReason",
      tab_loader_stats.tab_first_paint_reason, PAINT_FINISHED_UMA_MAX);

  base::UmaHistogramCounts100("SessionRestore.TabCount",
                              tab_loader_stats.tab_count);
  base::UmaHistogramCounts100("SessionRestore.TabCount.Active",
                              tab_loader_stats.active_tab_count);
  base::UmaHistogramCounts100("SessionRestore.TabCount.App",
                              tab_loader_stats.app_tab_count);
  base::UmaHistogramCounts100("SessionRestore.TabCount.InternalPage",
                              tab_loader_stats.internal_page_tab_count);
  base::UmaHistogramCounts100("SessionRestore.TabCount.Pinned",
                              tab_loader_stats.pinned_tab_count);
  base::UmaHistogramCounts100("SessionRestore.TabCount.Grouped",
                              tab_loader_stats.grouped_tab_count);
  base::UmaHistogramCounts100("SessionRestore.TabCount.LowSiteEngagement",
                              tab_loader_stats.low_site_engagement_tab_count);
  base::UmaHistogramCounts100(
      "SessionRestore.TabCount.LowSiteEngagementWithBackgroundCommunication",
      tab_loader_stats
          .low_site_engagement_with_background_communication_tab_count);
  base::UmaHistogramCounts100(
      "SessionRestore.TabCount.BackgroundNotificationPermission",
      tab_loader_stats.notification_permission_tab_count);
  base::UmaHistogramCounts100(
      "SessionRestore.TabCount.UpdatesTitleOrFaviconInBackground",
      tab_loader_stats.updates_in_background_tab_count);
}
