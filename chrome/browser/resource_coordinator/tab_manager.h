// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-forward.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/usage_clock.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/navigation_throttle.h"
#include "ui/gfx/native_widget_types.h"

class GURL;
class TabStripModel;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace resource_coordinator {

class BackgroundTabNavigationThrottle;

#if defined(OS_CHROMEOS)
class TabManagerDelegate;
#endif
class TabManagerStatsCollector;

// TabManager is responsible for triggering tab lifecycle state transitions.
//
// The TabManager also delays background tabs' navigation when needed in order
// to improve users' experience with the foreground tab.
//
// Note that the browser tests are only active for platforms that use
// TabManager (CrOS only for now) and need to be adjusted accordingly if
// support for new platforms is added.
//
// Tabs are identified by a unique ID vended by this component. These IDs are
// not reused in a session. They are stable for a given conceptual tab, and will
// follow it through discards, reloads, tab strip operations, etc.
//
// TODO(fdoray): Rename to LifecycleManager. https://crbug.com/775644
class TabManager : public LifecycleUnitObserver,
                   public LifecycleUnitSourceObserver,
                   public TabLoadTracker::Observer,
                   public TabStripModelObserver,
                   public metrics::DesktopSessionDurationTracker::Observer {
 public:
  // Forward declaration of resource coordinator signal observer.
  class ResourceCoordinatorSignalObserver;

  class WebContentsData;

  using TabDiscardDoneCB = base::ScopedClosureRunner;

  explicit TabManager(TabLoadTracker* tab_load_tracker);
  ~TabManager() override;

  // Start the Tab Manager.
  void Start();

  // Returns the LifecycleUnits managed by this, sorted from less to most
  // important to the user. It is unsafe to access a pointer in the returned
  // vector after a LifecycleUnit has been destroyed.
  LifecycleUnitVector GetSortedLifecycleUnits();

  // Discards a tab to free the memory occupied by its renderer. The tab still
  // exists in the tab-strip; clicking on it will reload it. If the |reason| is
  // urgent, an aggressive fast-kill will be attempted if the sudden termination
  // disablers are allowed to be ignored (e.g. On ChromeOS, we can ignore an
  // unload handler and fast-kill the tab regardless).
  void DiscardTab(
      LifecycleUnitDiscardReason reason,
      TabDiscardDoneCB tab_discard_done = TabDiscardDoneCB(base::DoNothing()));

  // Method used by the extensions API to discard tabs. If |contents| is null,
  // discards the least important tab using DiscardTab(). Otherwise discards
  // the given contents. Returns the new web_contents or null if no tab
  // was discarded.
  content::WebContents* DiscardTabByExtension(content::WebContents* contents);

  // Log memory statistics for the running processes, then discards a tab.
  // Tab discard happens sometime later, as collecting the statistics touches
  // multiple threads and takes time.
  void LogMemoryAndDiscardTab(LifecycleUnitDiscardReason reason);

  // Log memory statistics for the running processes.
  void LogMemory(const std::string& title);

  // TODO(fdoray): Remove these methods. TabManager shouldn't know about tabs.
  // https://crbug.com/775644
  void AddObserver(TabLifecycleObserver* observer);
  void RemoveObserver(TabLifecycleObserver* observer);

  // Indicates how TabManager should load pending background tabs. The mode is
  // recorded in tracing for easier debugging. The existing explicit numbering
  // should be kept as is when new modes are added.
  enum BackgroundTabLoadingMode {
    kStaggered = 0,  // Load a background tab after another tab is done loading.
    kPaused = 1      // Pause loading background tabs unless a user selects it.
  };

  // Maybe throttle a tab's navigation based on current system status.
  content::NavigationThrottle::ThrottleCheckResult MaybeThrottleNavigation(
      BackgroundTabNavigationThrottle* throttle);

  // Notifies TabManager that one navigation has finished (committed, aborted or
  // replaced). TabManager should clean up the NavigationHandle objects bookkept
  // before.
  void OnDidFinishNavigation(content::NavigationHandle* navigation_handle);

  // Notifies TabManager that one tab WebContents has been destroyed. TabManager
  // needs to clean up data related to that tab.
  void OnWebContentsDestroyed(content::WebContents* contents);

  // Return whether tabs are being loaded during session restore.
  bool IsSessionRestoreLoadingTabs() const {
    return is_session_restore_loading_tabs_;
  }

  // Returns the number of background tabs that are loading in a background tab
  // opening session.
  size_t GetBackgroundTabLoadingCount() const;

  // Returns the number of background tabs that are pending in a background tab
  // opening session.
  size_t GetBackgroundTabPendingCount() const;

  // Returns the number of tabs open in all browser instances.
  int GetTabCount() const;

  // Returns the number of restored tabs during session restore. This is
  // non-zero only during session restore.
  int restored_tab_count() const { return restored_tab_count_; }

  UsageClock* usage_clock() { return &usage_clock_; }

  // Returns true if the tab was created by session restore and has not finished
  // the first navigation.
  static bool IsTabInSessionRestore(content::WebContents* web_contents);

  // Returns true if the tab was created by session restore and initially in
  // foreground.
  static bool IsTabRestoredInForeground(content::WebContents* web_contents);

 private:
  friend class TabManagerStatsCollectorTest;
  friend class TabManagerWithProactiveDiscardExperimentEnabledTest;

  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, AutoDiscardable);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, BackgroundTabLoadingMode);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, BackgroundTabLoadingSlots);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, BackgroundTabsLoadingOrdering);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, CanOnlyDiscardOnce);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ChildProcessNotifications);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, EnablePageAlmostIdleSignal);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, FreezeTab);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, InvalidOrEmptyURL);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, TabDiscardDoneCallback);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, IsInBackgroundTabOpeningSession);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, IsInternalPage);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, IsTabRestoredInForeground);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, MaybeThrottleNavigation);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, OnDelayedTabSelected);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, OnDidFinishNavigation);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, OnTabIsLoaded);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, OnWebContentsDestroyed);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, OomPressureListener);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, PauseAndResumeBackgroundTabOpening);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownSharedTabProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTestWithTwoTabs,
                           ProactiveFastShutdownSingleTabProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownWithBeforeunloadHandler);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownWithUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ProtectDevToolsTabsFromDiscarding);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ProtectPDFPages);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProtectRecentlyUsedTabsFromUrgentDiscarding);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, ProtectVideoTabs);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           SessionRestoreAfterBackgroundTabOpeningSession);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           SessionRestoreBeforeBackgroundTabOpeningSession);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, TabManagerBasics);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, TabManagerWasDiscarded);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           TabManagerWasDiscardedCrossSiteSubFrame);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, TimeoutWhenLoadingBackgroundTabs);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           TrackingNumberOfLoadedLifecycleUnits);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, UrgentFastShutdownSharedTabProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTestWithTwoTabs,
                           UrgentFastShutdownSingleTabProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           UrgentFastShutdownWithBeforeunloadHandler);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, UrgentFastShutdownWithUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(TabManagerWithExperimentDisabledTest,
                           IsInBackgroundTabOpeningSession);
  FRIEND_TEST_ALL_PREFIXES(TabManagerWithProactiveDiscardExperimentEnabledTest,
                           GetTimeInBackgroundBeforeProactiveDiscardTest);
  FRIEND_TEST_ALL_PREFIXES(
      TabManagerWithProactiveDiscardExperimentEnabledTest,
      NoProactiveDiscardWhenDiscardingVariationParamDisabled);
  FRIEND_TEST_ALL_PREFIXES(TabManagerWithProactiveDiscardExperimentEnabledTest,
                           FreezingWhenDiscardingVariationParamDisabled);
  FRIEND_TEST_ALL_PREFIXES(TabManagerWithProactiveDiscardExperimentEnabledTest,
                           NoUnfreezeWhenUnfreezingVariationParamDisabled);

  // Returns true if the |url| represents an internal Chrome web UI page that
  // can be easily reloaded and hence makes a good choice to discard.
  static bool IsInternalPage(const GURL& url);

  // Makes a request to the WebContents at the specified index to freeze its
  // page.
  void FreezeWebContentsAt(int index, TabStripModel* model);

  // Pause or resume background tab opening according to memory pressure change
  // if there are pending background tabs.
  void PauseBackgroundTabOpeningIfNeeded();
  void ResumeBackgroundTabOpeningIfNeeded();

  // Called by the memory pressure listener when the memory pressure rises.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);

  // Called when we finished handling the memory pressure by discarding tabs.
  void OnTabDiscardDone();

  // Register to start listening to memory pressure. Called on startup or end
  // of tab discards.
  void RegisterMemoryPressureListener();

  // Unregister to stop listening to memory pressure. Called on shutdown or
  // beginning of tab discards.
  void UnregisterMemoryPressureListener();

  // Called by OnTabStripModelChanged()
  void OnActiveTabChanged(content::WebContents* old_contents,
                          content::WebContents* new_contents);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // TabLoadTracker::Observer:
  void OnStartTracking(content::WebContents* web_contents,
                       LoadingState loading_state) override;
  void OnLoadingStateChange(content::WebContents* web_contents,
                            LoadingState old_loading_state,
                            LoadingState new_loading_state) override;
  void OnStopTracking(content::WebContents* web_contents,
                      LoadingState loading_state) override;

  // DesktopSessionDurationTracker::Observer:
  void OnSessionStarted(base::TimeTicks session_start) override;

  // Returns the WebContentsData associated with |contents|. Also takes care of
  // creating one if needed.
  static WebContentsData* GetWebContentsData(content::WebContents* contents);

  // Discards the less important LifecycleUnit that supports discarding under
  // |reason|.
  content::WebContents* DiscardTabImpl(
      LifecycleUnitDiscardReason reason,
      TabDiscardDoneCB tab_discard_done = TabDiscardDoneCB(base::DoNothing()));

  void OnSessionRestoreStartedLoadingTabs();
  void OnSessionRestoreFinishedLoadingTabs();
  void OnWillRestoreTab(content::WebContents* contents);

  // Returns true if it is in BackgroundTabOpening session, which is defined as
  // the duration from the time when the browser starts to load background tabs
  // until the time when browser has finished loading those tabs. During the
  // session, the session can end when background tabs' loading are paused due
  // to memory pressure. A new session starts when background tabs' loading
  // resume when memory pressure returns to normal.
  bool IsInBackgroundTabOpeningSession() const;

  // Returns true if TabManager can start loading next tab.
  bool CanLoadNextTab() const;

  // Start |force_load_timer_| to load the next background tab if the timer
  // expires before the current tab loading is finished.
  void StartForceLoadTimer();

  // Start loading the next background tab if needed. This is called when:
  // 1. a tab has finished loading;
  // 2. or a tab has been destroyed;
  // 3. or memory pressure is relieved;
  // 4. or |force_load_timer_| fires.
  void LoadNextBackgroundTabIfNeeded();

  // Resume the tab's navigation if it is pending right now. This is called when
  // a tab is selected.
  void ResumeTabNavigationIfNeeded(content::WebContents* contents);

  // Resume navigation.
  void ResumeNavigation(BackgroundTabNavigationThrottle* throttle);

  // Remove the pending navigation for the provided WebContents. Return the
  // removed NavigationThrottle. Return nullptr if it doesn't exists.
  BackgroundTabNavigationThrottle* RemovePendingNavigationIfNeeded(
      content::WebContents* contents);

  // Returns true if |first| is considered to resume navigation before |second|.
  static bool ComparePendingNavigations(
      const BackgroundTabNavigationThrottle* first,
      const BackgroundTabNavigationThrottle* second);

  // Returns the number of tabs that are not pending load or discarded.
  int GetNumAliveTabs() const;

  // Check if the tab is loading. Use only in tests.
  bool IsTabLoadingForTest(content::WebContents* contents) const;

  // Check if the navigation is delayed. Use only in tests.
  bool IsNavigationDelayedForTest(
      const content::NavigationHandle* navigation_handle) const;

  // Set |loading_slots_|. Use only in tests.
  void SetLoadingSlotsForTest(size_t loading_slots) {
    loading_slots_ = loading_slots;
  }

  // Reset |memory_pressure_listener_| in test so that the test is not affected
  // by memory pressure.
  void ResetMemoryPressureListenerForTest() {
    memory_pressure_listener_.reset();
  }

  TabManagerStatsCollector* stats_collector() { return stats_collector_.get(); }

  // Returns true if the background tab force load timer is running.
  bool IsForceLoadTimerRunning() const;

  // Returns the threshold after which a background LifecycleUnit gets
  // discarded, given the current number of alive LifecycleUnits and experiment
  // parameters.
  base::TimeDelta GetTimeInBackgroundBeforeProactiveDiscard() const;

  // Schedules a call to PerformStateTransitions() in |delay|. This overrides
  // any previously scheduled call.
  void SchedulePerformStateTransitions(base::TimeDelta delay);

  // Performs LifecycleUnit state transitions.
  //
  // To avoid reentrancy, this is never called synchronously. When a state
  // transition should happen in response to an event, an asynchronous call to
  // this is scheduled via SchedulePerformStateTransitions(base::TimeDelta()).
  // https://crbug.com/855053
  void PerformStateTransitions();

  // If |lifecycle_unit| can be frozen, freezes it. Returns the time at which
  // this should be called again, or TimeTicks::Max() if no further call is
  // needed. |now| is the current time.
  base::TimeTicks MaybeFreezeLifecycleUnit(LifecycleUnit* lifecycle_unit,
                                           base::TimeTicks now);

  // If |lifecycle_unit| has been frozen long enough and a sufficient amount of
  // time elapsed since the last unfreeze, unfreezes it and returns the time at
  // which it should be frozen again. If |lifecycle_unit| can't be unfrozen now,
  // returns the time at which this should be called again. |lifecycle_unit|
  // must be FROZEN. |now| is the current time.
  base::TimeTicks MaybeUnfreezeLifecycleUnit(LifecycleUnit* lifecycle_unit,
                                             base::TimeTicks now);

  // If enough Chrome usage time has elapsed since |lifecycle_unit| was hidden,
  // proactively discards it. |lifecycle_unit| must be discardable. Returns the
  // time at which this should be called again, or TimeTicks::Max() if no
  // further call is needed. Always returns a zero TimeTicks when a discard
  // happen, to check immediately if another discard should happen. |now| is the
  // current time.
  base::TimeTicks MaybeDiscardLifecycleUnit(LifecycleUnit* lifecycle_unit,
                                            base::TimeTicks now);

  // LifecycleUnitObserver:
  void OnLifecycleUnitVisibilityChanged(
      LifecycleUnit* lifecycle_unit,
      content::Visibility visibility) override;
  void OnLifecycleUnitDestroyed(LifecycleUnit* lifecycle_unit) override;
  void OnLifecycleUnitStateChanged(
      LifecycleUnit* lifecycle_unit,
      LifecycleUnitState last_state,
      LifecycleUnitStateChangeReason reason) override;

  // LifecycleUnitSourceObserver:
  void OnLifecycleUnitCreated(LifecycleUnit* lifecycle_unit) override;

  // Indicates if TabManager should proactively discard tabs.
  bool ShouldProactivelyDiscardTabs();

  // LifecycleUnits managed by this.
  LifecycleUnitSet lifecycle_units_;

  // Number of LifecycleUnits in |lifecycle_units_| that are not discarded. Used
  // to determine timeout threshold for proactive discarding.
  int num_loaded_lifecycle_units_ = 0;

  // Parameters for proactive freezing and discarding.
  ProactiveTabFreezeAndDiscardParams proactive_freeze_discard_params_;

  // Timer to update the state of LifecycleUnits. This is an std::unique_ptr to
  // allow initialization after mock time is setup in unit tests.
  std::unique_ptr<base::OneShotTimer> state_transitions_timer_;

  // Callback for |state_transitions_timer_|. Stored in a member to avoid
  // repetitive binds.
  const base::RepeatingClosure state_transitions_callback_;

  // A listener to global memory pressure events.
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

#if defined(OS_CHROMEOS)
  std::unique_ptr<TabManagerDelegate> delegate_;
#endif

  // Responsible for automatically registering this class as an observer of all
  // TabStripModels. Automatically tracks browsers as they come and go.
  BrowserTabStripTracker browser_tab_strip_tracker_;

  bool is_session_restore_loading_tabs_;
  size_t restored_tab_count_;

  class TabManagerSessionRestoreObserver;
  std::unique_ptr<TabManagerSessionRestoreObserver> session_restore_observer_;

  // The mode that TabManager is using to load pending background tabs.
  BackgroundTabLoadingMode background_tab_loading_mode_;

  // When the timer fires, it forces loading the next background tab if needed.
  std::unique_ptr<base::OneShotTimer> force_load_timer_;

  // The list of navigations that are delayed.
  std::vector<BackgroundTabNavigationThrottle*> pending_navigations_;

  // The tabs that are currently loading. We will consider loading the next
  // background tab when these tabs have finished loading or a background tab
  // is brought to foreground.
  std::set<content::WebContents*> loading_contents_;

  // The number of loading slots that TabManager can use to load background tabs
  // in parallel.
  size_t loading_slots_;

  // Records UMAs for tab and system-related events and properties during
  // session restore.
  std::unique_ptr<TabManagerStatsCollector> stats_collector_;

  // Last time at which a LifecycleUnit was temporarily unfrozen.
  base::TimeTicks last_unfreeze_time_;

  // A clock that advances when Chrome is in use.
  UsageClock usage_clock_;

  // The tab load tracker observed by this instance.
  TabLoadTracker* const tab_load_tracker_;

  // Weak pointer factory used for posting delayed tasks.
  base::WeakPtrFactory<TabManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(TabManager);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_H_
