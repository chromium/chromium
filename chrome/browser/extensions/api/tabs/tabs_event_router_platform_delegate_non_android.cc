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
constexpr char kFrozenKey[] = "frozen";
constexpr char kDiscardedKey[] = "discarded";

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
    case TabStripModelChange::kRemoved: {
      // These are handled via the TabsEventRouter's observation of
      // TabListInterface.
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
    router_->RegisterForTabNotifications(*new_contents, index);
  }
}

}  // namespace extensions
