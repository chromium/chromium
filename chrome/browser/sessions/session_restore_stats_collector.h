// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_SESSION_RESTORE_STATS_COLLECTOR_H_
#define CHROME_BROWSER_SESSIONS_SESSION_RESTORE_STATS_COLLECTOR_H_

#include <stddef.h>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base//scoped_observer.h"
#include "base/callback_list.h"
#include "base/macros.h"
#include "base/time/tick_clock.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_observer.h"

namespace content {
class NavigationController;
}

// SessionRestoreStatsCollector observes SessionRestore events ands records UMA
// accordingly.
//
// A SessionRestoreStatsCollector is tied to an instance of a session restore,
// currently being instantianted and owned by the TabLoader. It has two main
// phases to its life:
//
// 1. The session restore is active and ongoing (the TabLoader is still
//    scheduling tabs for loading). This phases ends when there are no
//    non-deferred tabs left to be loaded. During this phases statistics are
//    gathered in a structure before being emitted as UMA metrics at the end of
//    this phase. At this point the TabLoader ceases to exist and destroys it's
//    reference to the SessionRestoreStatsCollector.
// 2. If any tabs have been deferred the SessionRestoreStatsCollector continues
//    tracking deferred tabs. This continues to observe the tabs to see which
//    (if any) of the deferred tabs are subsequently forced to be loaded by the
//    user. Since such tabs may exist until the end of the browsers life the
//    statistics are emitted immediately, or risk being lost entirely. When
//    there are no longer deferred tabs to track the
//    SessionRestoreStatsCollector will destroy itself.
//
// TODO(chrisha): Many of these metrics don't make sense to collect in the
// presence of an unavailable network, or when tabs are closed during loading.
// Rethink the collection in these cases.
class SessionRestoreStatsCollector
    : public content::NotificationObserver,
      public content::RenderWidgetHostObserver,
      public base::RefCounted<SessionRestoreStatsCollector> {
 public:
  // Recorded in SessionRestore.ForegroundTabFirstPaint4.FinishReason metric.
  // Values other than PAINT_FINISHED_UMA_DONE indicate why FirstPaint time
  // was not recorded.
  enum SessionRestorePaintFinishReasonUma {
    // SessionRestore.ForegroundTabFirstPaint4_XX successfully recorded.
    PAINT_FINISHED_UMA_DONE = 0,
    // No tabs were visible the whole time before first paint.
    PAINT_FINISHED_UMA_NO_COMPLETELY_VISIBLE_TABS = 1,
    // No restored tabs were painted.
    PAINT_FINISHED_UMA_NO_PAINT = 2,
    // A non-restored tab was painted first.
    PAINT_FINISHED_NON_RESTORED_TAB_PAINTED_FIRST = 3,
    // The size of this enum. Must be the last entry.
    PAINT_FINISHED_UMA_MAX = 4,
  };

  // Houses all of the statistics gathered by the SessionRestoreStatsCollector
  // while the underlying TabLoader is active. These statistics are all reported
  // at once via the reporting delegate.
  struct TabLoaderStats {
    // Constructor that initializes everything to zero.
    TabLoaderStats();

    // The number of tabs involved in all overlapping session restores being
    // tracked by this SessionRestoreStatsCollector. This corresponds to the
    // "SessionRestore.TabCount" metric and one bucket of the
    // "SessionRestore.TabActions" histogram. If any tabs were deferred it also
    // corresponds to the "SessionRestore.TabCount.MemoryPressure.Total"
    // histogram.
    size_t tab_count;

    // The number of restored tabs that were deferred. Corresponds to the
    // "SessionRestore.TabCount.MemoryPressure.Deferred" histogram.
    size_t tabs_deferred;

    // The number of tabs whose loading was automatically started because they
    // are active or explicitly caused to be loaded by the TabLoader. This
    // corresponds to one bucket of the "SessionRestore.TabActions" histogram
    // and the "SessionRestore.TabCount.MemoryPressure.LoadStarted".
    size_t tabs_load_started;

    // The number of tabs loaded automatically because they are active, or
    // explicitly caused to be loaded by the TabLoader. This corresponds to one
    // bucket of the "SessionRestore.TabActions" histogram, and the
    // "SessionRestore.TabCount.MemoryPressure.Loaded" histogram.
    size_t tabs_loaded;

    // The time elapsed between |restore_started| and reception of the first
    // NOTIFICATION_LOAD_STOP event for any of the active tabs involved in the
    // session restore. If this is zero it is because it has not been
    // recorded (all visible tabs were closed before they finished loading, or
    // the user switched to an already loaded tab before a visible session
    // restore tab finished loading). Corresponds to
    // "SessionRestore.ForegroundTabFirstLoaded" and its _XX variants.
    base::TimeDelta foreground_tab_first_loaded;

    // The time elapsed between |restore_started| and reception of the first
    // NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_VISUAL_PROPERTIES event for
    // any of the tabs involved in the session restore. If this is zero it is
    // because it has not been recorded (all restored tabs were closed or
    // hidden before they were painted, or were never painted). Corresponds to
    // "SessionRestore.ForegroundTabFirstPaint4" and its _XX variants.
    base::TimeDelta foreground_tab_first_paint;

    // Whether we recorded |foreground_tab_first_paint| and if not, why.
    SessionRestorePaintFinishReasonUma tab_first_paint_reason;

    // The time taken for all non-deferred tabs to be loaded. This corresponds
    // to the "SessionRestore.AllTabsLoaded" metric and its _XX variants
    // (vaguely named for historical reasons, as it predates the concept of
    // deferred tabs).
    base::TimeDelta non_deferred_tabs_loaded;
  };

  // The StatsReportingDelegate is responsible for delivering statistics
  // reported by the SessionRestoreStatsCollector.
  class StatsReportingDelegate;

  // An implementation of StatsReportingDelegate for reporting via UMA.
  class UmaStatsReportingDelegate;

  // Constructs a SessionRestoreStatsCollector.
  SessionRestoreStatsCollector(
      const base::TimeTicks& restore_started,
      std::unique_ptr<StatsReportingDelegate> reporting_delegate);

  // Adds new tabs to the list of tracked tabs.
  void TrackTabs(const std::vector<SessionRestoreDelegate::RestoredTab>& tabs);

  // Called to indicate that the loading of a tab has been deferred by session
  // restore.
  void DeferTab(content::NavigationController* tab);

  // Called when about to load the next tab. Used as a signal to record how
  // often timeout happens. Timeout means we want to start loading the next tab
  // even though the previous tab is still loading.
  void OnWillLoadNextTab(bool timeout);

  // Exposed for unittesting.
  const TabLoaderStats& tab_loader_stats() const { return tab_loader_stats_; }

 private:
  friend class TestSessionRestoreStatsCollector;
  friend class base::RefCounted<SessionRestoreStatsCollector>;

  enum TabLoadingState { TAB_IS_NOT_LOADING, TAB_IS_LOADING, TAB_IS_LOADED };

  // State that is tracked for a tab while it is being observed.
  struct TabState {
    explicit TabState(content::NavigationController* controller);

    // The NavigationController associated with the tab. This is the primary
    // index for it and is never null.
    content::NavigationController* controller;

    // Set to true if the tab has been deferred by the TabLoader.
    bool is_deferred;

    // True if the tab was ever hidden or occluded during the restore process.
    bool was_hidden_or_occluded;

    // The current loading state of the tab.
    TabLoadingState loading_state;

    // RenderWidgetHost* SessionRestoreStatsCollector is observing for this tab,
    // if any.
    content::RenderWidgetHost* observed_host;
  };

  // Maps a NavigationController to its state. This is the primary map and
  // physically houses the state.
  using NavigationControllerMap =
      std::map<content::NavigationController*, TabState>;

  ~SessionRestoreStatsCollector() override;

  // NotificationObserver method. This is the workhorse of the class and drives
  // all state transitions.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // RenderWidgetHostObserver:
  void RenderWidgetHostVisibilityChanged(content::RenderWidgetHost* widget_host,
                                         bool became_visible) override;

  void RenderWidgetHostDestroyed(
      content::RenderWidgetHost* widget_host) override;

  // Called when a tab is no longer tracked. This is called by the 'Observe'
  // notification callback. Takes care of unregistering all observers and
  // removing the tab from all internal data structures.
  void RemoveTab(content::NavigationController* tab);

  // Registers for relevant notifications for a tab and inserts the tab into
  // to |tabs_tracked_| map. Return a pointer to the newly created TabState.
  TabState* RegisterForNotifications(content::NavigationController* tab);

  // Returns the tab state, nullptr if not found.
  TabState* GetTabState(content::NavigationController* tab);
  TabState* GetTabState(content::RenderWidgetHost* tab);

  // Marks a tab as loading.
  void MarkTabAsLoading(TabState* tab_state);

  // Checks to see if the SessionRestoreStatsCollector has finished collecting,
  // and if so, releases the self reference to the shared pointer.
  void ReleaseIfDoneTracking();

  // Testing seam for configuring the tick clock in use.
  void set_tick_clock(std::unique_ptr<const base::TickClock> tick_clock) {
    tick_clock_ = std::move(tick_clock);
  }

  // Has ReleaseIfDoneTracking determined that there are no non-deferred tabs to
  // track?
  bool done_tracking_non_deferred_tabs_;

  // Has the time for foreground tab load been recorded?
  bool got_first_foreground_load_;

  // False if the time for foreground tab paint been recorded, or no more
  // non-deferred tabs are left to wait for, true otherwise.
  bool waiting_for_first_paint_;

  // Won't record time for foreground tab paint because a non-restored
  // tab was painted first.
  bool non_restored_tab_painted_first_;

  // Got first paint of tab that was hidden or occluded before being painted.
  bool hidden_or_occluded_tab_ignored_;

  // The time the restore process started.
  const base::TimeTicks restore_started_;

  // List of tracked tabs, mapped to their TabState.
  NavigationControllerMap tabs_tracked_;

  // Counts the number of non-deferred tabs that the
  // SessionRestoreStatsCollector is waiting to see load.
  size_t waiting_for_load_tab_count_;

  // Counts the current number of actively loading tabs.
  size_t loading_tab_count_;

  // Counts the current number of deferred tabs.
  size_t deferred_tab_count_;

  // Notification registrar.
  content::NotificationRegistrar registrar_;

  // Statistics gathered regarding the TabLoader.
  TabLoaderStats tab_loader_stats_;

  // The source of ticks used for taking timing information. This is
  // configurable as a testing seam. Defaults to using base::DefaultTickClock,
  // which in turn uses base::TimeTicks.
  std::unique_ptr<const base::TickClock> tick_clock_;

  // The reporting delegate used to report gathered statistics.
  std::unique_ptr<StatsReportingDelegate> reporting_delegate_;

  // For keeping SessionRestoreStatsCollector alive while it is still working
  // even if no TabLoader references it. The object only lives on if it still
  // has deferred tabs remaining from an interrupted session restore.
  scoped_refptr<SessionRestoreStatsCollector> this_retainer_;

  ScopedObserver<content::RenderWidgetHost, content::RenderWidgetHostObserver>
      observer_{this};

  DISALLOW_COPY_AND_ASSIGN(SessionRestoreStatsCollector);
};

// An abstract reporting delegate is used as a testing seam.
class SessionRestoreStatsCollector::StatsReportingDelegate {
 public:
  StatsReportingDelegate() {}
  virtual ~StatsReportingDelegate() {}

  // Called when TabLoader has completed its work.
  virtual void ReportTabLoaderStats(const TabLoaderStats& tab_loader_stats) = 0;

  // Called when a tab has been deferred.
  virtual void ReportTabDeferred() = 0;

  // Called when a deferred tab has been loaded.
  virtual void ReportDeferredTabLoaded() = 0;

  // Called when a tab starts being tracked. Logs the relative time since last
  // use of the tab.
  virtual void ReportTabTimeSinceActive(base::TimeDelta elapsed) = 0;

  // Called when a tab starts being tracked. Logs the relative time since last
  // use of the tab. The |engagement| is a value that is typically between
  // 0 and 100, but is technically unbounded. See
  // chrome/browser/engagement/site_engagement_service.h for details.
  virtual void ReportTabSiteEngagementScore(double engagement) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StatsReportingDelegate);
};

// The default reporting delegate, which reports statistics via UMA.
class SessionRestoreStatsCollector::UmaStatsReportingDelegate
    : public StatsReportingDelegate {
 public:
  UmaStatsReportingDelegate();
  ~UmaStatsReportingDelegate() override {}

  // StatsReportingDelegate:
  void ReportTabLoaderStats(const TabLoaderStats& tab_loader_stats) override;
  void ReportTabDeferred() override;
  void ReportDeferredTabLoaded() override;
  void ReportTabTimeSinceActive(base::TimeDelta elapsed) override;
  void ReportTabSiteEngagementScore(double engagement) override;

 private:
  // Has ReportTabDeferred been called?
  bool got_report_tab_deferred_;

  DISALLOW_COPY_AND_ASSIGN(UmaStatsReportingDelegate);
};

#endif  // CHROME_BROWSER_SESSIONS_SESSION_RESTORE_STATS_COLLECTOR_H_
