// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface_observer.h"
#include "components/favicon/core/favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/zoom/zoom_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/extensions/api/tabs/tabs_event_router_platform_delegate_android.h"
#else
#include "chrome/browser/extensions/api/tabs/tabs_event_router_platform_delegate_non_android.h"
#endif

class Profile;

namespace content {
class WebContents;
}

namespace tabs {
class TabInterface;
}

namespace extensions {

// The TabsEventRouter listens to tab events and routes them to listeners inside
// extension process renderers.
// TabsEventRouter will only route events from windows/tabs within a profile to
// extension processes in the same profile.
// TODO(https://crbug.com/473593117): Right now, the entire functionality of
// this class is essentially delegated to its platform delegate. We need to
// pull this functionality into this class.
class TabsEventRouter : public favicon::FaviconDriverObserver,
                        public performance_manager::PageLiveStateObserver,
                        public TabListInterfaceObserver,
                        public zoom::ZoomObserver {
 public:
  explicit TabsEventRouter(Profile* profile);
  TabsEventRouter(const TabsEventRouter&) = delete;
  TabsEventRouter& operator=(const TabsEventRouter&) = delete;
  ~TabsEventRouter() override;

 private:
  // The platform delegate is basically a platform-specific addendum to this
  // class, so we allow it to reach into this class's internal state.
  friend class TabsEventRouterPlatformDelegate;

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
    // `contents`.
    TabEntry(TabsEventRouter& router, content::WebContents& contents);

    TabEntry(const TabEntry&) = delete;
    TabEntry& operator=(const TabEntry&) = delete;

    ~TabEntry() override;

    // Update the audible state and return whether they were changed.
    bool SetAudible(bool new_val);

    // content::WebContentsObserver:
    void NavigationEntryCommitted(
        const content::LoadCommittedDetails& load_details) override;
    void DidStopLoading() override;
    void TitleWasSet(content::NavigationEntry* entry) override;
    void DidUpdateAudioMutingState(bool muted) override;
    void WebContentsDestroyed() override;

    int last_known_index() const { return last_known_index_; }
    void set_last_known_index(int last_known_index) {
      last_known_index_ = last_known_index;
    }

   private:
    // Called when the recently-audible state for the tab changed.
    void OnRecentlyAudibleStateChanged(bool was_recently_audible);

    // Called when the pin state has changed for the given tab.
    void OnPinnedStateChanged(tabs::TabInterface* tab, bool new_pinned_state);

    // Whether we are waiting to fire the 'complete' status change. This will
    // occur the first time the WebContents stops loading after the
    // NavigationEntryCommitted() method was called. The tab may go back into
    // and out of the loading state subsequently, but we will ignore those
    // changes.
    bool complete_waiting_on_load_ = false;

    GURL url_;

    // Callback subscription to be notified as the "pinned" state changes.
    base::CallbackListSubscription pinned_state_subscription_;

    // Callback subscription to be notified as the "recently audible" state
    // changes.
    base::CallbackListSubscription recently_audible_subscription_;

    // Event router that the WebContents's notifications are forwarded to.
    raw_ref<TabsEventRouter> router_;

    // The most recent index we have associated with this tab.
    int last_known_index_ = -1;

    base::WeakPtrFactory<TabEntry> weak_factory_{this};
  };

  // Returns true if the event router should track the given `browser`.
  bool ShouldTrackBrowser(BrowserWindowInterface& browser);

  // Starts tracking the given `tab_list`.
  void TrackTabList(TabListInterface& tab_list);

  // Registers to receive the various notifications we are interested in for a
  // tab.
  void RegisterForTabNotifications(content::WebContents& contents,
                                   int tab_index);

  // Removes notifications and tab entry added in RegisterForTabNotifications.
  // `expect_registered` indicates whether we should enforce that the tab was
  // being observed.
  void UnregisterForTabNotifications(content::WebContents& contents,
                                     bool expect_registered);

  // Gets the TabEntry for the given `contents`. Returns TabEntry* if found,
  // nullptr if not.
  TabEntry* GetTabEntry(content::WebContents& contents);

  // Internal processing of tab updated events. Intended to be called when
  // there's any changed property.
  void TabUpdated(TabEntry* entry,
                  std::set<std::string> changed_property_names);

  // Packages `changed_property_names` as a tab updated event for the tab
  // `contents` and dispatches the event to the extension.
  void DispatchTabUpdatedEvent(content::WebContents* contents,
                               std::set<std::string> changed_property_names);

  // Dispatches the `tabs.onCreated` API event for the given `contents`.
  // `active` indicates if the tab is active in its tab strip.
  void DispatchTabCreatedEvent(content::WebContents* contents, bool active);

  // Dispatches the `tabs.onRemoved` event. `is_window_closing` indicates if
  // the window owning the tab is also closing.
  void DispatchTabRemovedEvent(content::WebContents& contents,
                               bool is_window_closing);

  // Dispatches the `tabs.onDetached` event.
  void DispatchTabDetachedEvent(content::WebContents& contents);

  // The DispatchEvent methods forward events to the `profile`'s event router.
  // The TabsEventRouterPlatformDelegate listens to events for all profiles,
  // so we avoid duplication by dropping events destined for other profiles.
  void DispatchEvent(Profile* profile,
                     events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::ListValue args,
                     EventRouter::UserGestureState user_gesture);

  // Updates the last known indices recorded in `tab_entries_` for the tabs in
  // `tab_list`.
  void UpdateTabIndices(TabListInterface& tab_list);

  // TabListInterfaceObserver:
  void OnTabAdded(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int index) override;
  void OnActiveTabChanged(TabListInterface& tab_list,
                          tabs::TabInterface* tab) override;
  void OnTabRemoved(TabListInterface& tab_list,
                    tabs::TabInterface* tab,
                    TabRemovedReason removed_reason) override;
  void OnTabMoved(TabListInterface& tab_list,
                  tabs::TabInterface* tab,
                  int from_index,
                  int to_index) override;
  void OnHighlightedTabsChanged(
      TabListInterface& tab_list,
      const std::set<tabs::TabInterface*>& highlighted_tabs) override;
  void OnTabListDestroyed(TabListInterface& tab_list) override;

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

  // Triggers a tab updated event if the favicon URL changes.
  void FaviconUrlUpdated(content::WebContents* contents);

  // performance_manager::PageLiveStateObserver:
  void OnIsAutoDiscardableChanged(
      const performance_manager::PageNode* page_node) override;

  // Whether this event router has been fully initialized.
  bool initialized_ = false;

  // Observations for different state changes in tabs.

  using TabEntryMap = std::map<int, std::unique_ptr<TabEntry>>;
  TabEntryMap tab_entries_;

  base::ScopedMultiSourceObservation<TabListInterface, TabListInterfaceObserver>
      tab_list_observations_{this};

  base::ScopedMultiSourceObservation<favicon::FaviconDriver,
                                     favicon::FaviconDriverObserver>
      favicon_scoped_observations_{this};
  base::ScopedMultiSourceObservation<zoom::ZoomController, zoom::ZoomObserver>
      zoom_scoped_observations_{this};

  // The profile this router is associated with.
  raw_ptr<Profile> profile_;

  // The associated platform delegate. This is only wrapped in an optional to
  // allow delayed instantiation. See also comment in the constructor
  // definition.
  std::optional<TabsEventRouterPlatformDelegate> platform_delegate_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_H_
