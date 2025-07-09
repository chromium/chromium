// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_ANDROID_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/event_router.h"
#include "url/gurl.h"

class Profile;
class TabAndroid;
class TabModel;

namespace content {
class WebContents;
}

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
  void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type) override;
  void TabRemoved(TabAndroid* tab) override;

 private:
  class TabEntry : public content::WebContentsObserver {
   public:
    TabEntry(TabsEventRouterAndroid* router, content::WebContents* contents);
    ~TabEntry() override;

    // content::WebContentsObserver:
    void DidStopLoading() override;
    void TitleWasSet(content::NavigationEntry* entry) override;
    void WebContentsDestroyed() override;

    const raw_ptr<TabsEventRouterAndroid> router_;
    GURL url_;
  };

  void TabCreatedAt(content::WebContents* contents, bool active);

  // Internal processing of tab updated events. Intended to be called when
  // there's any changed property.
  void TabUpdated(TabEntry* entry,
                  std::set<std::string> changed_property_names);

  void DispatchTabUpdatedEvent(content::WebContents* contents,
                               std::set<std::string> changed_property_names);
  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::Value::List args,
                     EventRouter::UserGestureState user_gesture);

  void RegisterForTabNotifications(content::WebContents* contents);

  // Gets the TabEntry for the given `contents`. Returns TabEntry* if found,
  // nullptr if not.
  TabEntry* GetTabEntry(content::WebContents* contents);

  raw_ptr<Profile> profile_;

  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};

  // The map is keyed by tab id.
  using TabEntryMap = std::map<int, std::unique_ptr<TabEntry>>;
  TabEntryMap tab_entries_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TABS_TABS_EVENT_ROUTER_ANDROID_H_
