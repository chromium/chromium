// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router_platform_delegate_non_android.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/recently_audible_helper.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/features/feature.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/mojom/event_dispatcher.mojom-forward.h"
#include "ui/gfx/range/range.h"

using base::Value;
using content::WebContents;

namespace extensions {

namespace {

constexpr char kGroupIdKey[] = "groupId";
constexpr char kSplitIdKey[] = "splitViewId";
constexpr char kOldPositionKey[] = "oldPosition";
constexpr char kOldWindowIdKey[] = "oldWindowId";
constexpr char kFrozenKey[] = "frozen";
constexpr char kDiscardedKey[] = "discarded";
constexpr char kTabIdKey[] = "tabId";
constexpr char kTabIdsKey[] = "tabIds";

}  // namespace

TabsEventRouterPlatformDelegate::TabsEventRouterPlatformDelegate(
    TabsEventRouter& router,
    Profile& profile)
    : router_(router),
      profile_(profile),
      browser_tab_strip_tracker_(this, this) {
  DCHECK(!profile.IsOffTheRecord());

  BrowserList::AddObserver(this);
  browser_tab_strip_tracker_.Init();

  // Track any existing browsers. The `browser_tab_strip_tracker_` does this for
  // TabStripModel observations, but we need the parent router to also observe
  // TabListInterface.
  ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        OnBrowserAdded(browser->GetBrowserForMigrationOnly());
        return true;  // Keep iterating.
      });

  tab_source_scoped_observation_.Observe(
      resource_coordinator::GetTabLifecycleUnitSource());
}

TabsEventRouterPlatformDelegate::~TabsEventRouterPlatformDelegate() {
  BrowserList::RemoveObserver(this);
}

bool TabsEventRouterPlatformDelegate::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return router_->ShouldTrackBrowser(*browser);
}

void TabsEventRouterPlatformDelegate::OnBrowserSetLastActive(Browser* browser) {
  TabsWindowsAPI* tabs_window_api = TabsWindowsAPI::Get(&(*profile_));
  if (tabs_window_api) {
    tabs_window_api->windows_event_router()->OnActiveWindowChanged(
        browser ? BrowserExtensionWindowController::From(browser) : nullptr);
  }
}

void TabsEventRouterPlatformDelegate::OnBrowserAdded(Browser* browser) {
  if (ShouldTrackBrowser(browser)) {
    TabListInterface* tab_list = TabListInterface::From(browser);
    CHECK(tab_list);
    router_->TrackTabList(*tab_list);
  }
}

void TabsEventRouterPlatformDelegate::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  switch (change.type()) {
    case TabStripModelChange::kInserted:
    case TabStripModelChange::kMoved:
      // These are handled via the TabsEventRouter's observation of
      // TabListInterface.
      break;
    case TabStripModelChange::kRemoved: {
      for (const auto& contents : change.GetRemove()->contents) {
        if (contents.remove_reason ==
                TabStripModelChange::RemoveReason::kDeleted ||
            contents.remove_reason ==
                TabStripModelChange::RemoveReason::kInsertedIntoSidePanel) {
          DispatchTabClosingAt(tab_strip_model, contents.contents,
                               contents.index);
        }

        DispatchTabDetachedAt(contents.contents, contents.index,
                              selection.old_contents == contents.contents);
      }
      break;
    }
    case TabStripModelChange::kReplaced: {
      auto* replace = change.GetReplace();
      DispatchTabReplacedAt(replace->old_contents, replace->new_contents,
                            replace->index);
      break;
    }
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (tab_strip_model->empty()) {
    return;
  }

  if (selection.active_tab_changed()) {
    DispatchActiveTabChanged(selection.old_contents, selection.new_contents);
  }

  if (selection.selection_changed()) {
    DispatchTabSelectionChanged(tab_strip_model, selection.old_model);
  }
}

void TabsEventRouterPlatformDelegate::OnTabGroupChanged(
    const TabGroupChange& change) {
  // Maintain the previous tabstrip observation call sequence for extension so
  // that it does not cause a breaking change for clients during detaching and
  // re-inserting tab groups.
  if (change.type == TabGroupChange::kCreated &&
      change.GetCreateChange()->reason() ==
          TabGroupChange::TabGroupCreationReason::
              kInsertedFromAnotherTabstrip) {
    for (tabs::TabInterface* tab :
         change.GetCreateChange()->GetDetachedTabs()) {
      std::set<std::string> changed_property_names;
      changed_property_names.insert(kGroupIdKey);
      router_->DispatchTabUpdatedEvent(tab->GetContents(),
                                       std::move(changed_property_names));
    }
  } else if (change.type == TabGroupChange::kClosed &&
             change.GetCloseChange()->reason() ==
                 TabGroupChange::TabGroupClosureReason::
                     kDetachedToAnotherTabstrip) {
    for (tabs::TabInterface* tab : change.GetCloseChange()->GetDetachedTabs()) {
      std::set<std::string> changed_property_names;
      changed_property_names.insert(kGroupIdKey);
      router_->DispatchTabUpdatedEvent(tab->GetContents(),
                                       std::move(changed_property_names));
    }
  }
}

void TabsEventRouterPlatformDelegate::OnSplitTabChanged(
    const SplitTabChange& change) {
  if (change.type == SplitTabChange::Type::kAdded &&
      change.GetAddedChange()->reason() !=
          SplitTabChange::SplitTabAddReason::kInsertedFromAnotherTabstrip) {
    for (const std::pair<tabs::TabInterface*, int>& tab :
         change.GetAddedChange()->tabs()) {
      std::set<std::string> changed_property_names;
      changed_property_names.insert(kSplitIdKey);
      router_->DispatchTabUpdatedEvent(tab.first->GetContents(),
                                       std::move(changed_property_names));
    }
  }
  if (change.type == SplitTabChange::Type::kRemoved &&
      change.GetRemovedChange()->reason() !=
          SplitTabChange::SplitTabRemoveReason::kDetachedToAnotherTabstrip) {
    for (const std::pair<tabs::TabInterface*, int>& tab :
         change.GetRemovedChange()->tabs()) {
      std::set<std::string> changed_property_names;
      changed_property_names.insert(kSplitIdKey);
      router_->DispatchTabUpdatedEvent(tab.first->GetContents(),
                                       std::move(changed_property_names));
    }
  }
}

void TabsEventRouterPlatformDelegate::TabGroupedStateChanged(
    TabStripModel* tab_strip_model,
    std::optional<tab_groups::TabGroupId> old_group,
    std::optional<tab_groups::TabGroupId> new_group,
    tabs::TabInterface* tab,
    int index) {
  std::set<std::string> changed_property_names;
  changed_property_names.insert(kGroupIdKey);
  router_->DispatchTabUpdatedEvent(tab->GetContents(),
                                   std::move(changed_property_names));
}

void TabsEventRouterPlatformDelegate::OnLifecycleUnitStateChanged(
    resource_coordinator::LifecycleUnit* lifecycle_unit,
    ::mojom::LifecycleUnitState previous_state) {
  const ::mojom::LifecycleUnitState new_state = lifecycle_unit->GetState();
  auto previous_or_new_state_is = [&](::mojom::LifecycleUnitState state) {
    return previous_state == state || new_state == state;
  };

  std::set<std::string> changed_property_names;

  if (previous_or_new_state_is(::mojom::LifecycleUnitState::DISCARDED)) {
    // If the "discarded" property changes, so does the "status" property:
    // - a discarded tab has status "unloaded", and will transition to "loading"
    //   on un-discarding; and,
    // - a tab can only be discarded if its status is "complete" or "loading",
    //   in which case it will transition to "unloaded".
    changed_property_names.insert(kDiscardedKey);
    changed_property_names.insert(tabs_constants::kStatusKey);
  }

  if (previous_or_new_state_is(::mojom::LifecycleUnitState::FROZEN)) {
    changed_property_names.insert(kFrozenKey);
  }

  if (!changed_property_names.empty()) {
    router_->DispatchTabUpdatedEvent(
        lifecycle_unit->AsTabLifecycleUnitExternal()->GetWebContents(),
        std::move(changed_property_names));
  }
}

void TabsEventRouterPlatformDelegate::DispatchTabClosingAt(
    TabStripModel* tab_strip_model,
    WebContents* contents,
    int index) {
  int tab_id = ExtensionTabUtil::GetTabId(contents);

  base::ListValue args;
  args.Append(tab_id);

  base::DictValue object_args;
  object_args.Set(tabs_constants::kWindowIdKey,
                  ExtensionTabUtil::GetWindowIdOfTab(contents));
  object_args.Set(tabs_constants::kIsWindowClosingKey,
                  tab_strip_model->closing_all());
  args.Append(std::move(object_args));

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  router_->DispatchEvent(profile, events::TABS_ON_REMOVED,
                         api::tabs::OnRemoved::kEventName, std::move(args),
                         EventRouter::UserGestureState::kUnknown);

  router_->UnregisterForTabNotifications(*contents, /*expect_registered=*/true);
}

void TabsEventRouterPlatformDelegate::DispatchTabDetachedAt(
    WebContents* contents,
    int index,
    bool was_active) {
  if (!router_->GetTabEntry(*contents)) {
    // The tab was removed. Don't send detach event.
    return;
  }

  base::ListValue args;
  args.Append(ExtensionTabUtil::GetTabId(contents));

  base::DictValue object_args;
  object_args.Set(kOldWindowIdKey,
                  ExtensionTabUtil::GetWindowIdOfTab(contents));
  object_args.Set(kOldPositionKey, index);
  args.Append(std::move(object_args));

  Profile* profile = Profile::FromBrowserContext(contents->GetBrowserContext());
  router_->DispatchEvent(profile, events::TABS_ON_DETACHED,
                         api::tabs::OnDetached::kEventName, std::move(args),
                         EventRouter::UserGestureState::kUnknown);
}

void TabsEventRouterPlatformDelegate::DispatchActiveTabChanged(
    WebContents* old_contents,
    WebContents* new_contents) {
  base::ListValue args;
  int tab_id = ExtensionTabUtil::GetTabId(new_contents);
  args.Append(tab_id);

  base::DictValue object_args;
  object_args.Set(tabs_constants::kWindowIdKey,
                  ExtensionTabUtil::GetWindowIdOfTab(new_contents));
  args.Append(object_args.Clone());

  // The onActivated event replaced onActiveChanged and onSelectionChanged. The
  // deprecated events take two arguments: tabId, {windowId}.
  Profile* profile =
      Profile::FromBrowserContext(new_contents->GetBrowserContext());

  router_->DispatchEvent(profile, events::TABS_ON_SELECTION_CHANGED,
                         api::tabs::OnSelectionChanged::kEventName,
                         args.Clone(), EventRouter::UserGestureState::kUnknown);
  router_->DispatchEvent(profile, events::TABS_ON_ACTIVE_CHANGED,
                         api::tabs::OnActiveChanged::kEventName,
                         std::move(args),
                         EventRouter::UserGestureState::kUnknown);

  // The onActivated event takes one argument: {windowId, tabId}.
  base::ListValue on_activated_args;
  object_args.Set(kTabIdKey, tab_id);
  on_activated_args.Append(std::move(object_args));
  router_->DispatchEvent(
      profile, events::TABS_ON_ACTIVATED, api::tabs::OnActivated::kEventName,
      std::move(on_activated_args), EventRouter::UserGestureState::kUnknown);
}

void TabsEventRouterPlatformDelegate::DispatchTabSelectionChanged(
    TabStripModel* tab_strip_model,
    const ui::ListSelectionModel& old_model) {
  base::ListValue all_tabs;

  for (tabs::TabInterface* tab :
       tab_strip_model->selection_model().selected_tabs()) {
    WebContents* contents = tab->GetContents();
    if (!contents) {
      break;
    }
    int tab_id = ExtensionTabUtil::GetTabId(contents);
    all_tabs.Append(tab_id);
  }

  base::ListValue args;
  base::DictValue select_info;

  int window_id = -1;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [tab_strip_model,
       &window_id](BrowserWindowInterface* browser_window_interface) {
        if (browser_window_interface->GetTabStripModel() == tab_strip_model) {
          window_id = ExtensionTabUtil::GetWindowId(browser_window_interface);
          return false;
        }
        return true;
      });

  select_info.Set(tabs_constants::kWindowIdKey, window_id);

  select_info.Set(kTabIdsKey, std::move(all_tabs));
  args.Append(std::move(select_info));

  // The onHighlighted event replaced onHighlightChanged.
  Profile* profile = tab_strip_model->profile();
  router_->DispatchEvent(profile, events::TABS_ON_HIGHLIGHT_CHANGED,
                         api::tabs::OnHighlightChanged::kEventName,
                         args.Clone(), EventRouter::UserGestureState::kUnknown);
  router_->DispatchEvent(profile, events::TABS_ON_HIGHLIGHTED,
                         api::tabs::OnHighlighted::kEventName, std::move(args),
                         EventRouter::UserGestureState::kUnknown);
}

void TabsEventRouterPlatformDelegate::DispatchTabReplacedAt(
    WebContents* old_contents,
    WebContents* new_contents,
    int index) {
  // Notify listeners that the next tabs closing or being added are due to
  // WebContents being swapped.
  const int new_tab_id = ExtensionTabUtil::GetTabId(new_contents);
  const int old_tab_id = ExtensionTabUtil::GetTabId(old_contents);
  base::ListValue args;
  args.Append(new_tab_id);
  args.Append(old_tab_id);

  router_->DispatchEvent(
      Profile::FromBrowserContext(new_contents->GetBrowserContext()),
      events::TABS_ON_REPLACED, api::tabs::OnReplaced::kEventName,
      std::move(args), EventRouter::UserGestureState::kUnknown);

  router_->UnregisterForTabNotifications(*old_contents,
                                         /*expect_registered=*/true);

  if (!router_->GetTabEntry(*new_contents)) {
    router_->RegisterForTabNotifications(*new_contents);
  }
}

}  // namespace extensions
