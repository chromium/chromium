// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/zoom/zoom_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/event_router.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {
class TabManager;
}

namespace extensions {

// The TabsEventRouter listens to tab events and routes them to listeners inside
// extension process renderers.
// TabsEventRouter will only route events from windows/tabs within a profile to
// extension processes in the same profile.
class TabsEventRouter : public TabStripModelObserver,
                        public BrowserTabStripTrackerDelegate,
                        public BrowserListObserver,
                        public favicon::FaviconDriverObserver,
                        public zoom::ZoomObserver,
                        public resource_coordinator::TabLifecycleObserver {
 public:
  explicit TabsEventRouter(Profile* profile);

  TabsEventRouter(const TabsEventRouter&) = delete;
  TabsEventRouter& operator=(const TabsEventRouter&) = delete;

  ~TabsEventRouter() override;

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(Browser* browser) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;
  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) override;
  void TabGroupedStateChanged(std::optional<tab_groups::TabGroupId> group,
                              tabs::TabModel* tab,
                              int index) override;

  // ZoomObserver:
  void OnZoomControllerDestroyed(
      zoom::ZoomController* zoom_controller) override;
  void OnZoomChanged(
      const zoom::ZoomController::ZoomChangedEventData& data) override;

  // favicon::FaviconDriverObserver:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

  // resource_coordinator::TabLifecycleObserver:
  void OnDiscardedStateChange(content::WebContents* contents,
                              ::mojom::LifecycleUnitDiscardReason reason,
                              bool is_discarded) override;
  void OnAutoDiscardableStateChange(content::WebContents* contents,
                                    bool is_auto_discardable) override;

 private:
  // Methods called from OnTabStripModelChanged.
  void DispatchTabInsertedAt(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index,
                             bool active);
  void DispatchTabClosingAt(TabStripModel* tab_strip_model,
                            content::WebContents* contents,
                            int index);
  void DispatchTabDetachedAt(content::WebContents* contents,
                             int index,
                             bool was_active);
  void DispatchActiveTabChanged(content::WebContents* old_contents,
                                content::WebContents* new_contents);
  void DispatchTabSelectionChanged(TabStripModel* tab_strip_model,
                                   const ui::ListSelectionModel& old_model);
  void DispatchTabMoved(content::WebContents* contents,
                        int from_index,
                        int to_index);
  void DispatchTabReplacedAt(content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index);

  // "Synthetic" event. Called from DispatchTabInsertedAt if new tab is
  // detected.
  void TabCreatedAt(content::WebContents* contents, int index, bool active);

  // Internal processing of tab updated events. Intended to be called when
  // there's any changed property.
  class TabEntry;
  void TabUpdated(TabEntry* entry,
                  std::set<std::string> changed_property_names);

  // Triggers a tab updated event if the favicon URL changes.
  void FaviconUrlUpdated(content::WebContents* contents);

  // The DispatchEvent methods forward events to the |profile|'s event router.
  // The TabsEventRouter listens to events for all profiles,
  // so we avoid duplication by dropping events destined for other profiles.
  void DispatchEvent(Profile* profile,
                     events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List args,
                     EventRouter::UserGestureState user_gesture);

  // Packages |changed_property_names| as a tab updated event for the tab
  // |contents| and dispatches the event to the extension.
  void DispatchTabUpdatedEvent(content::WebContents* contents,
                               std::set<std::string> changed_property_names);

  // Register ourselves to receive the various notifications we are interested
  // in for a tab. Also create tab entry to observe web contents notifications.
  void RegisterForTabNotifications(content::WebContents* contents);

  // Removes notifications and tab entry added in RegisterForTabNotifications.
  void UnregisterForTabNotifications(content::WebContents* contents);

  // Maintain some information about known tabs, so we can:
  //
  //  - distinguish between tab creation and tab insertion
  //  - not send tab-detached after tab-removed
  //  - reduce the "noise" of TabChangedAt() when sending events to extensions
  //  - remember last muted and audible states to know if there was a change
  //  - listen to WebContentsObserver notifications and forward them to the
  //    event router.
  class TabEntry : public content::WebContentsObserver {
   public:
    // Create a TabEntry associated with, and tracking state changes to,
    // |contents|.
    TabEntry(TabsEventRouter* router, content::WebContents* contents);

    TabEntry(const TabEntry&) = delete;
    TabEntry& operator=(const TabEntry&) = delete;

    // Indicate via a list of property names if a tab is loading based on its
    // WebContents. Whether the state has changed or not is used to determine if
    // events need to be sent to extensions during processing of TabChangedAt()
    // If this method indicates that a tab should "hold" a state-change to
    // "loading", the NavigationEntryCommitted() method should eventually send a
    // similar message to undo it.
    std::set<std::string> UpdateLoadState();

    // Update the audible and muted states and return whether they were changed
    bool SetAudible(bool new_val);
    bool SetMuted(bool new_val);

    // content::WebContentsObserver:
    void NavigationEntryCommitted(
        const content::LoadCommittedDetails& load_details) override;
    void TitleWasSet(content::NavigationEntry* entry) override;
    void WebContentsDestroyed() override;

   private:
    // Whether we are waiting to fire the 'complete' status change. This will
    // occur the first time the WebContents stops loading after the
    // NAV_ENTRY_COMMITTED was fired. The tab may go back into and out of the
    // loading state subsequently, but we will ignore those changes.
    bool complete_waiting_on_load_;

    // Previous audible and muted states
    bool was_audible_;
    bool was_muted_;

    GURL url_;

    // Event router that the WebContents's noficiations are forwarded to.
    raw_ptr<TabsEventRouter> router_;
  };

  // Gets the TabEntry for the given |contents|. Returns TabEntry* if found,
  // nullptr if not.
  TabEntry* GetTabEntry(content::WebContents* contents);

  using TabEntryMap = std::map<int, std::unique_ptr<TabEntry>>;
  TabEntryMap tab_entries_;

  // The main profile that owns this event router.
  raw_ptr<Profile> profile_;

  base::ScopedMultiSourceObservation<favicon::FaviconDriver,
                                     favicon::FaviconDriverObserver>
      favicon_scoped_observations_{this};
  base::ScopedMultiSourceObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_scoped_observations_{this};

  BrowserTabStripTracker browser_tab_strip_tracker_;

  base::ScopedObservation<resource_coordinator::TabManager,
                          resource_coordinator::TabLifecycleObserver>
      tab_manager_scoped_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
