// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_observer.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "content/public/browser/navigation_throttle.h"

class GURL;

namespace content {
class WebContents;
}  // namespace content

namespace resource_coordinator {

// TabManager is responsible for triggering tab lifecycle state transitions.
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
                   public LifecycleUnitSourceObserver {
 public:
  // Forward declaration of resource coordinator signal observer.
  class ResourceCoordinatorSignalObserver;

  using TabDiscardDoneCB = base::ScopedClosureRunner;

  TabManager();

  TabManager(const TabManager&) = delete;
  TabManager& operator=(const TabManager&) = delete;

  ~TabManager() override;

  // Start the Tab Manager.
  void Start();

  // Method used by the extensions API to discard tabs. If |contents| is null,
  // discards the least important tab using DiscardTab(). Otherwise discards
  // the given contents. Returns the new web_contents or null if no tab
  // was discarded.
  content::WebContents* DiscardTabByExtension(content::WebContents* contents);

 private:
  friend class TabManagerTest;

  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, IsInternalPage);
  FRIEND_TEST_ALL_PREFIXES(TabManagerIgnoreWorkersTest,
                           UrgentFastShutdownWithWorker);

  // Returns true if the |url| represents an internal Chrome web UI page that
  // can be easily reloaded and hence makes a good choice to discard.
  static bool IsInternalPage(const GURL& url);

  // Discards the less important LifecycleUnit that supports discarding under
  // |reason|. Exposes |minimum_time_in_background_to_discard| so tests can set
  // this to 0.
  content::WebContents* DiscardTabImpl(
      LifecycleUnitDiscardReason reason,
      base::TimeDelta minimum_time_in_background_to_discard);

  void OnSessionRestoreStartedLoadingTabs();
  void OnSessionRestoreFinishedLoadingTabs();

  // Returns the number of tabs that are not pending load or discarded.
  int GetNumAliveTabs() const;

  // LifecycleUnitObserver:
  void OnLifecycleUnitDestroyed(LifecycleUnit* lifecycle_unit) override;

  // LifecycleUnitSourceObserver:
  void OnLifecycleUnitCreated(LifecycleUnit* lifecycle_unit) override;

  // LifecycleUnits managed by this.
  LifecycleUnitSet lifecycle_units_;

  // A listener to global memory pressure events.
  std::unique_ptr<base::MemoryPressureListenerRegistration>
      memory_pressure_listener_registration_;

  // Weak pointer factory used for posting delayed tasks.
  base::WeakPtrFactory<TabManager> weak_ptr_factory_{this};
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_MANAGER_H_
