// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_ANDROID_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/event_router.h"
#include "url/gurl.h"

class Profile;
class TabAndroid;
class TabModel;

namespace extensions {
class TabsEventRouter;

// TODO(crbug.com/473593117): This class is a temporary solution to unblock the
// Tabs API on desktop Android. It can be deleted once TabsEventRouter works on
// desktop Android.
class TabsEventRouterPlatformDelegate : public TabModelListObserver,
                                        public TabModelObserver {
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

  // TabModelObserver:
  void TabRemoved(TabAndroid* tab) override;

 private:
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::ListValue args,
                     EventRouter::UserGestureState user_gesture);

  // The platform-agnostic TabsEventRouter.
  // TODO(https://crbug.com/473593117): This should go away; it's just here
  // while we migrate code.
  raw_ref<TabsEventRouter> router_;

  raw_ref<Profile> profile_;

  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_PLATFORM_DELEGATE_ANDROID_H_
