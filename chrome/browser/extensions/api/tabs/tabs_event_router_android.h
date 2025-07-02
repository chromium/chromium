// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_ANDROID_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"

class Profile;
class TabAndroid;
class TabModel;

namespace extensions {

// TabsEventRouterAndroid listens to tab events on Android and routes them to
// listeners inside extension process renderers.
//
// TODO(crbug.com/427503497): This class is a temporary solution to unblock the
// Tabs API on desktop Android. It can be deleted once TabsEventRouter works on
// desktop Android.
class TabsEventRouterAndroid : public TabModelListObserver,
                               public TabModelObserver {
 public:
  explicit TabsEventRouterAndroid(Profile* profile);
  TabsEventRouterAndroid(const TabsEventRouterAndroid&) = delete;
  TabsEventRouterAndroid& operator=(const TabsEventRouterAndroid&) = delete;
  ~TabsEventRouterAndroid() override;

  // TabModelListObserver:
  void OnTabModelAdded(TabModel* tab_model) override;
  void OnTabModelRemoved(TabModel* tab_model) override;

  // TabModelObserver:
  void WillAddTab(TabAndroid* tab, TabModel::TabLaunchType type) override;
  void TabRemoved(TabAndroid* tab) override;

 private:
  raw_ptr<Profile> profile_;

  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_ANDROID_H_
