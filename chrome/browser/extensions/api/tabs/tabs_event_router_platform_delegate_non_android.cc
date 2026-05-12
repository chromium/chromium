// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_event_router_platform_delegate_non_android.h"

#include <utility>

#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/extensions/browser_extension_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

using base::Value;
using content::WebContents;

namespace extensions {

namespace {

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

  browser_collection_observation_.Observe(
      GlobalBrowserCollection::GetInstance());
  browser_tab_strip_tracker_.Init();

  // Track any existing browsers. The `browser_tab_strip_tracker_` does this for
  // TabStripModel observations, but we need the parent router to also observe
  // TabListInterface.
  ForEachCurrentAndNewBrowserWindowInterfaceOrderedByActivation(
      [this](BrowserWindowInterface* browser) {
        OnBrowserCreated(browser);
        return true;  // Keep iterating.
      });

  tab_source_scoped_observation_.Observe(
      resource_coordinator::GetTabLifecycleUnitSource());
}

TabsEventRouterPlatformDelegate::~TabsEventRouterPlatformDelegate() = default;

bool TabsEventRouterPlatformDelegate::ShouldTrackBrowser(
    BrowserWindowInterface* browser) {
  return router_->ShouldTrackBrowser(*browser);
}

void TabsEventRouterPlatformDelegate::OnBrowserActivated(
    BrowserWindowInterface* browser) {
  TabsWindowsAPI* tabs_window_api = TabsWindowsAPI::Get(&(*profile_));
  if (tabs_window_api) {
    tabs_window_api->windows_event_router()->OnActiveWindowChanged(
        BrowserExtensionWindowController::From(browser));
  }
}

void TabsEventRouterPlatformDelegate::OnBrowserCreated(
    BrowserWindowInterface* browser) {
  if (ShouldTrackBrowser(browser)) {
    TabListInterface* tab_list = TabListInterface::From(browser);
    CHECK(tab_list);
    router_->TrackTabList(*tab_list);
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

}  // namespace extensions
