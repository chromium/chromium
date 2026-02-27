// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"

#include <memory>
#include <set>

#include "base/values.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/common/extensions/api/tabs.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "third_party/blink/public/common/page/page_zoom.h"

namespace extensions {

namespace {

constexpr char kAudibleKey[] = "audible";
constexpr char kAutoDiscardableKey[] = "autoDiscardable";
constexpr char kFromIndexKey[] = "fromIndex";
constexpr char kMutedInfoKey[] = "mutedInfo";
constexpr char kNewPositionKey[] = "newPosition";
constexpr char kNewWindowIdKey[] = "newWindowId";
constexpr char kOldPositionKey[] = "oldPosition";
constexpr char kOldWindowIdKey[] = "oldWindowId";
constexpr char kPinnedKey[] = "pinned";
constexpr char kTabIdKey[] = "tabId";
constexpr char kToIndexKey[] = "toIndex";

// Callback for the event dispatch system. Computes which tab properties have
// changed. Builds an argument list with an entry for the changed properties and
// another entry with all properties. The properties may be "scrubbed" of
// sensitive information (like the previous URL). Returns true so the event will
// be dispatched.
bool WillDispatchTabUpdatedEvent(
    content::WebContents* contents,
    const std::set<std::string>& changed_property_names,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::DictValue* listener_filter,
    std::optional<base::ListValue>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out,
    bool* dispatch_separate_event_out) {
  auto scrub_tab_behavior = ExtensionTabUtil::GetScrubTabBehavior(
      extension, target_context, contents);
  api::tabs::Tab tab_object = ExtensionTabUtil::CreateTabObject(
      contents, scrub_tab_behavior, extension);

  base::DictValue tab_value = tab_object.ToValue();

  base::DictValue changed_properties;
  for (const auto& property : changed_property_names) {
    if (const base::Value* value = tab_value.Find(property)) {
      changed_properties.Set(property, value->Clone());
    }
  }

  event_args_out.emplace();
  event_args_out->Append(ExtensionTabUtil::GetTabId(contents));
  event_args_out->Append(std::move(changed_properties));
  event_args_out->Append(std::move(tab_value));
  return true;
}

// Updates the arguments and appropriately scrubs data for tab creation events.
bool WillDispatchTabCreatedEvent(
    content::WebContents* contents,
    bool active,
    content::BrowserContext* browser_context,
    mojom::ContextType target_context,
    const Extension* extension,
    const base::DictValue* listener_filter,
    std::optional<base::ListValue>& event_args_out,
    mojom::EventFilteringInfoPtr& event_filtering_info_out,
    bool* dispatch_separate_event_out) {
  ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
      ExtensionTabUtil::GetScrubTabBehavior(extension, target_context,
                                            contents);
  base::DictValue tab_value =
      ExtensionTabUtil::CreateTabObject(contents, scrub_tab_behavior, extension)
          .ToValue();
  tab_value.Set(tabs_constants::kSelectedKey, active);
  tab_value.Set(tabs_constants::kActiveKey, active);

  event_args_out.emplace();
  event_args_out->Append(std::move(tab_value));
  return true;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TabsEventRouter::TabEntry:

TabsEventRouter::TabEntry::TabEntry(TabsEventRouter& router,
                                    content::WebContents& contents)
    : WebContentsObserver(&contents),
      router_(router) {
  // We should only get here for WebContents that are tabs.
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(&contents);
  CHECK(tab);

  pinned_state_subscription_ =
      tab->RegisterPinnedStateChanged(base::BindRepeating(
          &TabEntry::OnPinnedStateChanged, weak_factory_.GetWeakPtr()));

  auto* audible_helper = RecentlyAudibleHelper::FromWebContents(&contents);
  recently_audible_subscription_ =
      audible_helper->RegisterRecentlyAudibleChangedCallback(
          base::BindRepeating(&TabEntry::OnRecentlyAudibleStateChanged,
                              weak_factory_.GetWeakPtr()));
}

TabsEventRouter::TabEntry::~TabEntry() = default;

void TabsEventRouter::TabEntry::NavigationEntryCommitted(
    const content::LoadCommittedDetails& load_details) {
  // Send 'status' of tab change. Expecting 'loading' is fired.
  complete_waiting_on_load_ = true;
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kStatusKey);
  if (web_contents()->GetURL() != url_) {
    url_ = web_contents()->GetURL();
    changed_property_names.insert(tabs_constants::kUrlKey);
  }

  router_->TabUpdated(this, std::move(changed_property_names));
}

void TabsEventRouter::TabEntry::DidStopLoading() {
  // The tab may go in & out of loading (for instance if iframes navigate).
  // We only want to respond to the first change from loading to !loading after
  // the NavigationEntryCommitted() was fired.
  if (!complete_waiting_on_load_) {
    return;
  }

  // Cache that we're no longer waiting on the load to finish so that we don't
  // dispatch for any subsequent (same page) loads. This will be reset on the
  // next navigation.
  complete_waiting_on_load_ = false;

  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kStatusKey);

  if (web_contents()->GetURL() != url_) {
    url_ = web_contents()->GetURL();
    changed_property_names.insert(tabs_constants::kUrlKey);
  }

  router_->TabUpdated(this, std::move(changed_property_names));
}

void TabsEventRouter::TabEntry::TitleWasSet(content::NavigationEntry* entry) {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kTitleKey);
  router_->TabUpdated(this, std::move(changed_property_names));
}

void TabsEventRouter::TabEntry::DidUpdateAudioMutingState(bool muted) {
  router_->TabUpdated(this, {kMutedInfoKey});
}

void TabsEventRouter::TabEntry::WebContentsDestroyed() {
  int tab_id = ExtensionTabUtil::GetTabId(web_contents());
  if (!SessionID::IsValidValue(tab_id)) {
    return;
  }

  // This is necessary because it's possible for tabs to be created, detached
  // and then destroyed without ever having been re-attached and closed. This
  // happens in the case of a devtools WebContents that is opened in window,
  // docked, then closed.
  // Warning: |this| will be deleted after this call.
  router_->UnregisterForTabNotifications(*web_contents(),
                                         /*expect_registered=*/true);
}

void TabsEventRouter::TabEntry::OnRecentlyAudibleStateChanged(
    bool was_recently_audible) {
  router_->TabUpdated(this, {kAudibleKey});
}

void TabsEventRouter::TabEntry::OnPinnedStateChanged(tabs::TabInterface* tab,
                                                     bool new_pinned_state) {
  router_->TabUpdated(this, {kPinnedKey});
}

////////////////////////////////////////////////////////////////////////////////
// TabsEventRouter:

TabsEventRouter::TabsEventRouter(Profile* profile) : profile_(profile) {
  performance_manager::PageLiveStateDecorator::AddAllPageObserver(this);

  // We instantiate the platform delegate outside the member initialization
  // list. Construction of the platform delegate might call back into this class
  // (e.g. to add existing tabs to track), so we want to make sure all members
  // are fully instantiated before those methods are called.
  platform_delegate_.emplace(*this, *profile);

  initialized_ = true;
}

TabsEventRouter::~TabsEventRouter() {
  performance_manager::PageLiveStateDecorator::RemoveAllPageObserver(this);
}

bool TabsEventRouter::ShouldTrackBrowser(BrowserWindowInterface& browser) {
  return profile_->IsSameOrParent(browser.GetProfile()) &&
         ExtensionTabUtil::BrowserSupportsTabs(&browser);
}

void TabsEventRouter::TrackTabList(TabListInterface& tab_list) {
  tab_list_observations_.AddObservation(&tab_list);

  // Bootstrap: monitor all pre-existing tabs in the tab list.
  std::vector<tabs::TabInterface*> tabs = tab_list.GetAllTabs();
  for (size_t i = 0u; i < tabs.size(); ++i) {
    OnTabAdded(tab_list, tabs[i], i);
  }
  // TODO(https://crbug.com/473593117): Do we also need to fire selection
  // changed events? It looks like the non-Android BrowserTabStripTracker does.
}

void TabsEventRouter::RegisterForTabNotifications(
    content::WebContents& web_contents,
    int tab_index) {
  favicon_scoped_observations_.AddObservation(
      favicon::ContentFaviconDriver::FromWebContents(&web_contents));

  // Some pages on Android (like the NTP) may not have a zoom controller.
  if (auto* zoom_controller =
          zoom::ZoomController::FromWebContents(&web_contents)) {
    zoom_scoped_observations_.AddObservation(zoom_controller);
  }

  int tab_id = ExtensionTabUtil::GetTabId(&web_contents);
  DCHECK(tab_entries_.find(tab_id) == tab_entries_.end());
  auto tab_entry = std::make_unique<TabEntry>(*this, web_contents);
  tab_entry->set_last_known_index(tab_index);
  tab_entries_[tab_id] = std::move(tab_entry);
}

void TabsEventRouter::UnregisterForTabNotifications(
    content::WebContents& web_contents,
    bool expect_registered) {
  if (auto* zoom_controller =
          zoom::ZoomController::FromWebContents(&web_contents);
      zoom_controller &&
      zoom_scoped_observations_.IsObservingSource(zoom_controller)) {
    zoom_scoped_observations_.RemoveObservation(zoom_controller);
  }
  auto* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(&web_contents);
  bool is_observing_favicon_driver =
      favicon_scoped_observations_.IsObservingSource(favicon_driver);
  CHECK(is_observing_favicon_driver || !expect_registered);
  if (is_observing_favicon_driver) {
    favicon_scoped_observations_.RemoveObservation(favicon_driver);
  }

  int tab_id = ExtensionTabUtil::GetTabId(&web_contents);
  int removed_count = tab_entries_.erase(tab_id);
  DCHECK(removed_count > 0 || !expect_registered);
}

TabsEventRouter::TabEntry* TabsEventRouter::GetTabEntry(
    content::WebContents& contents) {
  const auto it = tab_entries_.find(ExtensionTabUtil::GetTabId(&contents));

  return it == tab_entries_.end() ? nullptr : it->second.get();
}

void TabsEventRouter::TabUpdated(TabEntry* entry,
                                 std::set<std::string> changed_property_names) {
  CHECK(!changed_property_names.empty());
  DispatchTabUpdatedEvent(entry->web_contents(),
                          std::move(changed_property_names));
}

void TabsEventRouter::DispatchTabUpdatedEvent(
    content::WebContents* contents,
    std::set<std::string> changed_property_names) {
  DCHECK(!changed_property_names.empty());
  DCHECK(contents);

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());

  auto event = std::make_unique<Event>(
      events::TABS_ON_UPDATED, api::tabs::OnUpdated::kEventName,
      // The event arguments depend on the extension's permission. They are set
      // in WillDispatchTabUpdatedEvent().
      base::ListValue(), profile);
  event->user_gesture = EventRouter::UserGestureState::kNotEnabled;
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchTabUpdatedEvent, contents,
                          std::move(changed_property_names));
  EventRouter::Get(profile)->BroadcastEvent(std::move(event));
}

void TabsEventRouter::DispatchTabCreatedEvent(content::WebContents* contents,
                                              bool active) {
  Profile* const profile =
      Profile::FromBrowserContext(contents->GetBrowserContext());
  auto event = std::make_unique<Event>(events::TABS_ON_CREATED,
                                       api::tabs::OnCreated::kEventName,
                                       base::ListValue(), profile);
  event->user_gesture = EventRouter::UserGestureState::kNotEnabled;
  event->will_dispatch_callback =
      base::BindRepeating(&WillDispatchTabCreatedEvent, contents, active);
  EventRouter::Get(profile)->BroadcastEvent(std::move(event));
}

void TabsEventRouter::DispatchTabRemovedEvent(
    content::WebContents& web_contents,
    bool is_window_closing) {
  int tab_id = ExtensionTabUtil::GetTabId(&web_contents);

  base::ListValue args;
  args.Append(tab_id);

  base::DictValue object_args;
  object_args.Set(tabs_constants::kWindowIdKey,
                  ExtensionTabUtil::GetWindowIdOfTab(&web_contents));
  object_args.Set(tabs_constants::kIsWindowClosingKey, is_window_closing);
  args.Append(std::move(object_args));

  Profile* profile =
      Profile::FromBrowserContext(web_contents.GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_REMOVED,
                api::tabs::OnRemoved::kEventName, std::move(args),
                EventRouter::UserGestureState::kUnknown);

  UnregisterForTabNotifications(web_contents, /*expect_registered=*/true);
}

void TabsEventRouter::DispatchTabDetachedEvent(
    content::WebContents& web_contents) {
  TabEntry* tab_entry = GetTabEntry(web_contents);
  if (!tab_entry) {
    // The tab was removed. Don't send detach event.
    return;
  }

  base::ListValue args;
  args.Append(ExtensionTabUtil::GetTabId(&web_contents));

  base::DictValue object_args;
  object_args.Set(kOldWindowIdKey,
                  ExtensionTabUtil::GetWindowIdOfTab(&web_contents));
  object_args.Set(kOldPositionKey, tab_entry->last_known_index());
  args.Append(std::move(object_args));

  Profile* profile =
      Profile::FromBrowserContext(web_contents.GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_DETACHED,
                api::tabs::OnDetached::kEventName, std::move(args),
                EventRouter::UserGestureState::kUnknown);
}

void TabsEventRouter::DispatchEvent(
    Profile* profile,
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::ListValue args,
    EventRouter::UserGestureState user_gesture) {
  EventRouter* event_router = EventRouter::Get(profile);
  if (!profile_->IsSameOrParent(profile) || !event_router) {
    return;
  }

  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(args), profile);
  event->user_gesture = user_gesture;
  event_router->BroadcastEvent(std::move(event));
}

void TabsEventRouter::UpdateTabIndices(TabListInterface& tab_list) {
  std::vector<tabs::TabInterface*> tabs = tab_list.GetAllTabs();
  for (size_t i = 0; i < tabs.size(); ++i) {
    content::WebContents* web_contents = tabs[i]->GetContents();
    CHECK(web_contents);
    TabEntry* tab_entry = GetTabEntry(*web_contents);
    if (!tab_entry) {
      // We're not yet tracking this tab; this can happen when this is called
      // from adding a new tab. The index for that tab will be updated when it's
      // added to the set of tracked tabs.
      continue;
    }
    tab_entry->set_last_known_index(i);
  }
}

void TabsEventRouter::OnTabAdded(TabListInterface& tab_list,
                                 tabs::TabInterface* tab,
                                 int index) {
  content::WebContents* contents = tab->GetContents();
  CHECK(contents);

  // Adding a new tab can affect the indices of all existing tabs in the tab
  // list. Update them.
  UpdateTabIndices(tab_list);

  // Check if we've ever seen this tab.
  TabEntry* tab_entry = GetTabEntry(*contents);
  if (tab_entry) {
    // This is a known tab. Update the tab index and dispatch `onAttached`.
    tab_entry->set_last_known_index(index);
    int tab_id = ExtensionTabUtil::GetTabId(contents);
    base::ListValue args;
    args.Append(tab_id);

    base::DictValue object_args;
    object_args.Set(kNewWindowIdKey,
                    base::Value(ExtensionTabUtil::GetWindowIdOfTab(contents)));
    object_args.Set(kNewPositionKey, base::Value(index));
    args.Append(std::move(object_args));

    Profile* profile =
        Profile::FromBrowserContext(contents->GetBrowserContext());
    DispatchEvent(profile, events::TABS_ON_ATTACHED,
                  api::tabs::OnAttached::kEventName, std::move(args),
                  EventRouter::UserGestureState::kUnknown);

    return;
  }

  // We've never seen this tab. Begin tracking it.
  RegisterForTabNotifications(*contents, index);

  // If we're still initializing the event router, assume this is
  // bootstrapping instead of a new tab.
  if (!initialized_) {
    return;
  }

  // Otherwise, dispatch the `onCreated` event.
  DispatchTabCreatedEvent(contents, tab->IsActivated());
}

void TabsEventRouter::OnActiveTabChanged(TabListInterface& tab_list,
                                         tabs::TabInterface* tab) {
  content::WebContents* tab_contents = tab->GetContents();
  CHECK(tab_contents);

  base::ListValue args;
  int tab_id = ExtensionTabUtil::GetTabId(tab_contents);
  args.Append(tab_id);

  base::DictValue object_args;
  object_args.Set(tabs_constants::kWindowIdKey,
                  ExtensionTabUtil::GetWindowIdOfTab(tab_contents));
  args.Append(object_args.Clone());

  // The onActivated event replaced onActiveChanged and onSelectionChanged. The
  // deprecated events take two arguments: tabId, {windowId}.
  Profile* profile =
      Profile::FromBrowserContext(tab_contents->GetBrowserContext());

  DispatchEvent(profile, events::TABS_ON_SELECTION_CHANGED,
                api::tabs::OnSelectionChanged::kEventName, args.Clone(),
                EventRouter::UserGestureState::kUnknown);
  DispatchEvent(profile, events::TABS_ON_ACTIVE_CHANGED,
                api::tabs::OnActiveChanged::kEventName, std::move(args),
                EventRouter::UserGestureState::kUnknown);

  // The onActivated event takes one argument: {windowId, tabId}.
  base::ListValue on_activated_args;
  object_args.Set(kTabIdKey, tab_id);
  on_activated_args.Append(std::move(object_args));
  DispatchEvent(
      profile, events::TABS_ON_ACTIVATED, api::tabs::OnActivated::kEventName,
      std::move(on_activated_args), EventRouter::UserGestureState::kUnknown);
}

void TabsEventRouter::OnTabRemoved(TabListInterface& tab_list,
                                   tabs::TabInterface* tab,
                                   TabRemovedReason removed_reason) {
  content::WebContents* web_contents = tab->GetContents();
  CHECK(web_contents);

  // Removing a tab can affect the indices of all existing tabs in the tab
  // list. Update them.
  UpdateTabIndices(tab_list);

  switch (removed_reason) {
    case TabRemovedReason::kDeleted:
    case TabRemovedReason::kInsertedIntoSidePanel:
      DispatchTabRemovedEvent(*web_contents, tab_list.IsClosingAllTabs());
      break;
    case TabRemovedReason::kInsertedIntoOtherTabStrip:
      DispatchTabDetachedEvent(*web_contents);
      break;
  }
}

void TabsEventRouter::OnTabMoved(TabListInterface& tab_list,
                                 tabs::TabInterface* tab,
                                 int from_index,
                                 int to_index) {
  CHECK(tab);
  content::WebContents* web_contents = tab->GetContents();
  CHECK(web_contents);

  // Moving tab can affect the indices of all existing tabs in the tab list
  // (not just the one being moved). Update them.
  UpdateTabIndices(tab_list);

  base::ListValue args;
  args.Append(ExtensionTabUtil::GetTabId(web_contents));

  base::DictValue object_args;
  object_args.Set(tabs_constants::kWindowIdKey,
                  ExtensionTabUtil::GetWindowIdOfTab(web_contents));
  object_args.Set(kFromIndexKey, from_index);
  object_args.Set(kToIndexKey, to_index);
  args.Append(std::move(object_args));

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_MOVED, api::tabs::OnMoved::kEventName,
                std::move(args), EventRouter::UserGestureState::kUnknown);
}

void TabsEventRouter::OnHighlightedTabsChanged(
    TabListInterface& tab_list,
    const std::set<tabs::TabInterface*>& highlighted_tabs) {
  api::tabs::OnHighlighted::HighlightInfo highlight_info;
  highlight_info.tab_ids.reserve(highlighted_tabs.size());
  highlight_info.window_id = -1;
  Profile* profile = nullptr;

  if (highlighted_tabs.empty()) {
    // The highlighted tabs could be empty, such as during window shutdown when
    // the whole tab list is empty. Bail in this case.
    return;
  }

  for (tabs::TabInterface* tab : highlighted_tabs) {
    content::WebContents* web_contents = tab->GetContents();
    CHECK(web_contents);

    // All the tabs should be in the same window, so just grab the window ID and
    // browser context from the first.
    if (highlight_info.window_id == -1) {
      profile = Profile::FromBrowserContext(web_contents->GetBrowserContext());
      highlight_info.window_id =
          ExtensionTabUtil::GetWindowIdOfTab(web_contents);
    }

    int tab_id = ExtensionTabUtil::GetTabId(web_contents);
    highlight_info.tab_ids.push_back(tab_id);
  }
  CHECK(profile);

  base::ListValue args = api::tabs::OnHighlighted::Create(highlight_info);
  // The onHighlighted event replaced onHighlightChanged.
  DispatchEvent(profile, events::TABS_ON_HIGHLIGHT_CHANGED,
                api::tabs::OnHighlightChanged::kEventName, args.Clone(),
                EventRouter::UserGestureState::kUnknown);
  DispatchEvent(profile, events::TABS_ON_HIGHLIGHTED,
                api::tabs::OnHighlighted::kEventName, std::move(args),
                EventRouter::UserGestureState::kUnknown);
}

void TabsEventRouter::OnTabListDestroyed(TabListInterface& tab_list) {
  tab_list_observations_.RemoveObservation(&tab_list);
}

void TabsEventRouter::OnZoomControllerDestroyed(
    zoom::ZoomController* zoom_controller) {
  if (zoom_scoped_observations_.IsObservingSource(zoom_controller)) {
    zoom_scoped_observations_.RemoveObservation(zoom_controller);
  }
}

void TabsEventRouter::OnZoomChanged(
    const zoom::ZoomController::ZoomChangedEventData& data) {
  DCHECK(data.web_contents);
  int tab_id = ExtensionTabUtil::GetTabId(data.web_contents);
  if (tab_id < 0) {
    return;
  }

  // Prepare the zoom change information.
  api::tabs::OnZoomChange::ZoomChangeInfo zoom_change_info;
  zoom_change_info.tab_id = tab_id;
  zoom_change_info.old_zoom_factor =
      blink::ZoomLevelToZoomFactor(data.old_zoom_level);
  zoom_change_info.new_zoom_factor =
      blink::ZoomLevelToZoomFactor(data.new_zoom_level);
  ZoomModeToZoomSettings(data.zoom_mode, &zoom_change_info.zoom_settings);

  // Dispatch the |onZoomChange| event.
  Profile* profile =
      Profile::FromBrowserContext(data.web_contents->GetBrowserContext());
  DispatchEvent(profile, events::TABS_ON_ZOOM_CHANGE,
                api::tabs::OnZoomChange::kEventName,
                api::tabs::OnZoomChange::Create(zoom_change_info),
                EventRouter::UserGestureState::kUnknown);
}

void TabsEventRouter::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  if (notification_icon_type == NON_TOUCH_16_DIP && icon_url_changed) {
    favicon::ContentFaviconDriver* content_favicon_driver =
        static_cast<favicon::ContentFaviconDriver*>(favicon_driver);
    FaviconUrlUpdated(content_favicon_driver->web_contents());
  }
}

void TabsEventRouter::FaviconUrlUpdated(content::WebContents* contents) {
  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  if (!entry || !entry->GetFavicon().valid) {
    return;
  }
  std::set<std::string> changed_property_names;
  changed_property_names.insert(tabs_constants::kFaviconUrlKey);
  DispatchTabUpdatedEvent(contents, std::move(changed_property_names));
}

void TabsEventRouter::OnIsAutoDiscardableChanged(
    const performance_manager::PageNode* page_node) {
  DispatchTabUpdatedEvent(page_node->GetWebContents().get(),
                          {kAutoDiscardableKey});
}

}  // namespace extensions
