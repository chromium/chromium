// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_NON_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_NON_ANDROID_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/tabs/tabs_api.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/event_router.h"

namespace content {
class WebContents;
}

namespace resource_coordinator {
class TabLifecycleUnitSource;
}

namespace extensions {
class TabsEventRouter;

// A non-android implementation of tabs event routing.
// TODO(https://crbug.com/473593117): Pull most of this logic into the parent
// TabsEventRouter.
class TabsEventRouterPlatformDelegate
    : public TabStripModelObserver,
      public BrowserTabStripTrackerDelegate,
      public BrowserListObserver,
      public resource_coordinator::LifecycleUnitObserver {
 public:
  TabsEventRouterPlatformDelegate(TabsEventRouter& router, Profile& profile);

  TabsEventRouterPlatformDelegate(const TabsEventRouterPlatformDelegate&) =
      delete;
  TabsEventRouterPlatformDelegate& operator=(
      const TabsEventRouterPlatformDelegate&) = delete;

  ~TabsEventRouterPlatformDelegate() override;

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(BrowserWindowInterface* browser) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserAdded(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void TabGroupedStateChanged(TabStripModel* tab_strip_model,
                              std::optional<tab_groups::TabGroupId> old_group,
                              std::optional<tab_groups::TabGroupId> new_group,
                              tabs::TabInterface* tab,
                              int index) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;
  void OnSplitTabChanged(const SplitTabChange& change) override;

  // resource_coordinator::LifecycleUnitObserver:
  void OnLifecycleUnitStateChanged(
      resource_coordinator::LifecycleUnit* lifecycle_unit,
      ::mojom::LifecycleUnitState previous_state) override;

 private:
  // Methods called from OnTabStripModelChanged.
  void DispatchTabInsertedAt(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index,
                             bool active);
  void DispatchTabReplacedAt(content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int index);

  // "Synthetic" event. Called from DispatchTabInsertedAt if new tab is
  // detected.
  void TabCreatedAt(content::WebContents* contents, int index, bool active);

  // The platform-agnostic TabsEventRouter.
  // TODO(https://crbug.com/473593117): This should go away; it's just here
  // while we migrate code.
  raw_ref<TabsEventRouter> router_;

  // The main profile that owns this event router.
  raw_ref<Profile> profile_;

  BrowserTabStripTracker browser_tab_strip_tracker_;

  base::ScopedObservation<resource_coordinator::TabLifecycleUnitSource,
                          resource_coordinator::LifecycleUnitObserver>
      tab_source_scoped_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_NON_ANDROID_H_
