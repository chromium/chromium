// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_NON_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_NON_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class Profile;

namespace resource_coordinator {
class TabLifecycleUnitSource;
}

class GlobalBrowserCollection;

namespace extensions {
class TabsEventRouter;

// A non-android implementation of tabs event routing.
// TODO(https://crbug.com/473593117): Pull most of this logic into the parent
// TabsEventRouter.
class TabsEventRouterPlatformDelegate
    : public TabStripModelObserver,
      public BrowserTabStripTrackerDelegate,
      public BrowserCollectionObserver,
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

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserCreated(BrowserWindowInterface* browser) override;

  // TabStripModelObserver:
  void OnSplitTabChanged(const SplitTabChange& change) override;

  // resource_coordinator::LifecycleUnitObserver:
  void OnLifecycleUnitStateChanged(
      resource_coordinator::LifecycleUnit* lifecycle_unit,
      ::mojom::LifecycleUnitState previous_state) override;

 private:
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

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_NON_ANDROID_H_
