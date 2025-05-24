// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_SOURCE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_SOURCE_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_base.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/performance_manager/public/mojom/coordination_unit.mojom.h"
#include "components/performance_manager/public/mojom/lifecycle.mojom-forward.h"

class TabStripModel;

namespace content {
class WebContents;
}

namespace resource_coordinator {

class TabLifecycleStateObserver;
class TabLifecycleUnitExternal;

// Creates and destroys LifecycleUnits as tabs are created and destroyed.
class TabLifecycleUnitSource : public BrowserListObserver,
                               public LifecycleUnitSourceBase,
                               public LifecycleUnitObserver,
                               public TabStripModelObserver {
 public:
  class TabLifecycleUnit;
  class LifecycleStateObserver;

  TabLifecycleUnitSource();

  TabLifecycleUnitSource(const TabLifecycleUnitSource&) = delete;
  TabLifecycleUnitSource& operator=(const TabLifecycleUnitSource&) = delete;

  ~TabLifecycleUnitSource() override;

  // Should be called once all the dependencies of this class have been created
  // (e.g. the global PerformanceManager instance).
  void Start();

  // Returns the TabLifecycleUnitExternal instance associated with
  // |web_contents|, or nullptr if |web_contents| isn't a tab.
  static TabLifecycleUnitExternal* GetTabLifecycleUnitExternal(
      content::WebContents* web_contents);

  // Adds / removes an observer that is notified when the discarded state of any
  // tab changes.
  void AddLifecycleObserver(LifecycleUnitObserver* observer);
  void RemoveLifecycleObserver(LifecycleUnitObserver* observer);

  // Pretend that |tab_strip| is the TabStripModel of the focused window.
  void SetFocusedTabStripModelForTesting(TabStripModel* tab_strip);

  // Returns the state of the MemoryLimitMbEnabled enterprise policy.
  bool memory_limit_enterprise_policy() const {
    return memory_limit_enterprise_policy_;
  }

  void SetMemoryLimitEnterprisePolicyFlag(bool enabled);

 protected:
  class TabLifecycleUnitHolder;

 private:
  friend class TabLifecycleStateObserver;
  friend class TabLifecycleUnitTest;
  friend class TabManagerTest;
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, TabManagerWasDiscarded);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           TabManagerWasDiscardedCrossSiteSubFrame);
  FRIEND_TEST_ALL_PREFIXES(TabLifecycleUnitSourceTest, Freeze);

  // Returns the TabLifecycleUnit instance associated with |web_contents|, or
  // nullptr if |web_contents| isn't a tab.
  static TabLifecycleUnit* GetTabLifecycleUnit(
      content::WebContents* web_contents);

  // Returns the TabStripModel of the focused browser window, if any.
  TabStripModel* GetFocusedTabStripModel() const;

  // Updates the focused TabLifecycleUnit.
  void UpdateFocusedTab();

  // Updates the focused TabLifecycleUnit to |new_focused_lifecycle_unit|.
  // TabInsertedAt() calls this directly instead of UpdateFocusedTab() because
  // the active WebContents of a TabStripModel isn't updated when
  // TabInsertedAt() is called.
  void UpdateFocusedTabTo(TabLifecycleUnit* new_focused_lifecycle_unit);

  // Methods called by OnTabStripModelChanged()
  void OnTabInserted(TabStripModel* tab_strip_model,
                     content::WebContents* contents,
                     bool foreground);
  void OnTabDetached(content::WebContents* contents);
  void OnTabReplaced(content::WebContents* old_contents,
                     content::WebContents* new_contents);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // BrowserListObserver:
  void OnBrowserRemoved(Browser* browser) override;
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // LifecycleUnitObserver:
  void OnLifecycleUnitStateChanged(
      LifecycleUnit* lifecycle_unit,
      LifecycleUnitState last_state,
      LifecycleUnitStateChangeReason reason) override;
  void OnLifecycleUnitDestroyed(LifecycleUnit* lifecycle_unit) override;

  // This is called indirectly from the corresponding event on a PageNode in the
  // performance_manager Graph.
  static void OnLifecycleStateChanged(
      content::WebContents* web_contents,
      performance_manager::mojom::LifecycleState state);
  static void OnIsHoldingWebLockChanged(content::WebContents* web_contents,
                                        bool is_holding_weblock);
  static void OnIsHoldingIndexedDBLockChanged(
      content::WebContents* web_contents,
      bool is_holding_indexeddb_lock);

  // Callback for TabLifecyclesEnterprisePreferenceMonitor.
  void SetTabLifecyclesEnterprisePolicy(bool enabled);

  // Tracks the BrowserList and all TabStripModels.
  BrowserTabStripTracker browser_tab_strip_tracker_;

  // Pretend that this is the TabStripModel of the focused window, for testing.
  raw_ptr<TabStripModel, AcrossTasksDanglingUntriaged>
      focused_tab_strip_model_for_testing_ = nullptr;

  // The currently focused TabLifecycleUnit. Updated by UpdateFocusedTab().
  raw_ptr<TabLifecycleUnit> focused_lifecycle_unit_ = nullptr;

  // Observers notified when the discarded state of any tab changes.
  base::ObserverList<LifecycleUnitObserver>::UncheckedAndDanglingUntriaged
      lifecycle_unit_observers_;

  // Observes all LifecycleUnits tracked by this source to forward their
  // notifications to `lifecycle_unit_observers_`
  base::ScopedMultiSourceObservation<LifecycleUnit, LifecycleUnitObserver>
      lifecycle_unit_observations_{this};

  // The enterprise policy for setting a limit on total physical memory usage.
  bool memory_limit_enterprise_policy_ = false;
};

}  // namespace resource_coordinator

namespace base {

// Adaptor to allow base::ScopedObservation to install LifecycleUnitObservers
// for all tabs.
template <>
struct ScopedObservationTraits<resource_coordinator::TabLifecycleUnitSource,
                               resource_coordinator::LifecycleUnitObserver> {
  static void AddObserver(
      resource_coordinator::TabLifecycleUnitSource* source,
      resource_coordinator::LifecycleUnitObserver* observer) {
    source->AddLifecycleObserver(observer);
  }
  static void RemoveObserver(
      resource_coordinator::TabLifecycleUnitSource* source,
      resource_coordinator::LifecycleUnitObserver* observer) {
    source->RemoveLifecycleObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_SOURCE_H_
