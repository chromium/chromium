// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LOAD_TRACKER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LOAD_TRACKER_H_

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/process/kill.h"
#include "base/sequence_checker.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

class ResourceCoordinatorParts;
class ResourceCoordinatorTabHelper;

// DEPRECATED. New users must observe PageNode::IsLoading() with a
// PageNodeObserver. For guidance: //components/performance_manager/OWNERS
//
// This class has the sole purpose of tracking the state of all tab-related
// WebContents, and whether or not they are in an unloaded, currently loading,
// or loaded state.
//
// This class must be bound to a given Sequence and all access to it must occur
// on that Sequence. In practice, this is intended to be on the UI thread as the
// notifications of interest occur natively on that thread. All calculations are
// very short and quick, so it is suitable for use on that thread.
//
// This class is intended to be created in early startup and persists as a
// singleton in the browser process. It is deliberately leaked at shutdown.
//
// This class isn't directly an observer of anything. An external source must
// invoke the callbacks in the protected section of the class. In the case of
// the TabManager this is done by a combination of the
// ResourceCoordinatorTabHelper and the
// TabManagerResourceCoordinatorSignalObserver.
class TabLoadTracker {
 public:
  // An observer class. This allows external classes to be notified of loading
  // state changes.
  class Observer;

  using LoadingState = ::mojom::LifecycleUnitLoadingState;

  TabLoadTracker(const TabLoadTracker&) = delete;
  TabLoadTracker& operator=(const TabLoadTracker&) = delete;

  // A brief note around loading states specifically as they are defined in the
  // context of a WebContents:
  //
  // An initially constructed WebContents with no loaded content is UNLOADED.
  //
  // A WebContents transitions to LOADING when network data starts being
  // received for a top-level load to a different document. This considers
  // throttled navigations as not yet loading, and will only transition to
  // loading once the throttle has been removed.
  //
  // A LOADING WebContents transitions to LOADED when it reaches an "almost
  // idle" state, based on CPU and network quiescence or after an absolute
  // timeout (see PageLoadTrackerDecorator).
  //
  // A WebContents transitions to UNLOADED when its render process is gone.

  ~TabLoadTracker();

  // Returns the singleton TabLoadTracker instance.
  static TabLoadTracker* Get();

  // Allows querying the state of a tab. The provided |web_contents| must be
  // actively tracked.
  LoadingState GetLoadingState(content::WebContents* web_contents) const;

  // Returns the total number of tabs that are being tracked by this class.
  size_t GetTabCount() const;

  // Returns the number of tabs in each state.
  size_t GetTabCount(LoadingState loading_state) const;
  size_t GetUnloadedTabCount() const;
  size_t GetLoadingTabCount() const;
  size_t GetLoadedTabCount() const;

  // Adds/removes an observer. It is up to the observer to ensure their lifetime
  // exceeds that of the TabLoadTracker, as is removed prior to its destruction.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Exposed so that state transitions can be simulated in tests.
  void TransitionStateForTesting(content::WebContents* web_contents,
                                 LoadingState loading_state);

 protected:
  friend class ResourceCoordinatorParts;

  // For unittesting.
  friend class LocalSiteCharacteristicsWebContentsObserverTest;

  // These declarations allows the various bits of TabManager plumbing to
  // forward notifications to the TabLoadTracker.
  friend class ResourceCoordinatorTabHelper;
  friend class TabManagerResourceCoordinatorSignalObserver;

  FRIEND_TEST_ALL_PREFIXES(TabLifecycleUnitTest, CannotFreezeAFrozenTab);

  // This class is a singleton so the constructor is protected.
  TabLoadTracker();

  // Initiates tracking of a WebContents. This is fully able to determine the
  // initial state of the WebContents, even if it was created long ago
  // (is LOADING or LOADED) and only just attached to the tracker. See the
  // implementation of DetermineLoadingState for details.
  void StartTracking(content::WebContents* web_contents);

  // Stops tracking a |web_contents|.
  void StopTracking(content::WebContents* web_contents);

  // These are analogs of WebContentsObserver functions. This class is not
  // actually an observer, but the relevant events are forwarded to it from the
  // TabManager.
  //
  // In all cases, a call to PrimaryPageChanged() is expected to be followed by
  // a call to StopTracking(), RenderProcessGone() or OnPageStoppedLoading().
  void PrimaryPageChanged(content::WebContents* web_contents);
  void DidStopLoading(content::WebContents* web_contents);
  void RenderProcessGone(content::WebContents* web_contents,
                         base::TerminationStatus status);

  // Notifications to this are driven by the
  // TabManagerResourceCoordinatorSignalObserver.
  void OnPageStoppedLoading(content::WebContents* web_contents);

 private:
  // For unittesting.
  friend class TestTabLoadTracker;

  // Some metadata used to track the current state of the WebContents.
  struct WebContentsData {
    LoadingState loading_state = LoadingState::UNLOADED;
  };

  using TabMap = base::flat_map<content::WebContents*, WebContentsData>;

  // Helper function for determining the current state of a |web_contents|.
  LoadingState DetermineLoadingState(content::WebContents* web_contents);

  // Transitions a web contents to the given state. This updates the various
  // |state_counts_| and |tabs_| data. Setting |validate_transition| to false
  // means that valid state machine transitions aren't enforced via checks; this
  // is only used by state transitions forced via TransitionStateForTesting.
  void TransitionState(TabMap::iterator it, LoadingState loading_state);

  // The list of known WebContents and their states.
  TabMap tabs_;

  // The counts of tabs in each state.
  size_t state_counts_[static_cast<size_t>(LoadingState::kMaxValue) + 1] = {0};

  base::ObserverList<Observer>::UncheckedAndDanglingUntriaged observers_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// A class for observing loading state changes of WebContents under observation
// by a given TabLoadTracker. All of the callbacks will be invoked on the
// sequence to which the TabLoadTracker is bound.
class TabLoadTracker::Observer {
 public:
  using LoadingState = TabLoadTracker::LoadingState;

  Observer();

  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;

  virtual ~Observer();

  // Called when a |web_contents| is starting to be tracked.
  virtual void OnStartTracking(content::WebContents* web_contents,
                               LoadingState loading_state) {}

  // Called for every loading state change observed on a |web_contents|.
  virtual void OnLoadingStateChange(content::WebContents* web_contents,
                                    LoadingState old_loading_state,
                                    LoadingState new_loading_state) {}

  // Called when a |web_contents| is no longer being tracked.
  virtual void OnStopTracking(content::WebContents* web_contents,
                              LoadingState loading_state) {}
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LOAD_TRACKER_H_
