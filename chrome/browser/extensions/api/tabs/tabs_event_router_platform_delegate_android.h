// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_ANDROID_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"

class Profile;
class TabModel;

namespace extensions {
class TabsEventRouter;

// TODO(crbug.com/473593117): This class is a temporary solution to unblock the
// Tabs API on desktop Android. It can be deleted once TabsEventRouter works on
// desktop Android.
class TabsEventRouterPlatformDelegate : public TabModelListObserver {
 public:
  TabsEventRouterPlatformDelegate(TabsEventRouter& router, Profile& profile);
  TabsEventRouterPlatformDelegate(const TabsEventRouterPlatformDelegate&) =
      delete;
  TabsEventRouterPlatformDelegate& operator=(
      const TabsEventRouterPlatformDelegate&) = delete;
  ~TabsEventRouterPlatformDelegate() override;

  // TabModelListObserver:
  void OnTabModelAdded(TabModel* tab_model) override;
  void OnTabModelRemoved(TabModel* tab_model) override;

 private:
  // The platform-agnostic TabsEventRouter.
  // TODO(https://crbug.com/473593117): This should go away; it's just here
  // while we migrate code.
  raw_ref<TabsEventRouter> router_;

  raw_ref<Profile> profile_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_ANDROID_H_
