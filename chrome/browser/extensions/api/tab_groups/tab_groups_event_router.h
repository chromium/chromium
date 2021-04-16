// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_EVENT_ROUTER_H_

#include <memory>
#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/tab_groups/tab_group_id.h"
#include "extensions/browser/event_router.h"

namespace extensions {

// The TabGroupsEventRouter listens to group events and routes them to listeners
// inside extension process renderers.
// TabGroupsEventRouter will only route events from windows/tabs within a
// profile to extension processes in the same profile.
class TabGroupsEventRouter : public TabStripModelObserver,
                             public BrowserTabStripTrackerDelegate,
                             public KeyedService {
 public:
  explicit TabGroupsEventRouter(content::BrowserContext* context);
  TabGroupsEventRouter(const TabGroupsEventRouter&) = delete;
  TabGroupsEventRouter& operator=(const TabGroupsEventRouter&) = delete;
  ~TabGroupsEventRouter() override = default;

  // TabStripModelObserver:
  void OnTabGroupChanged(const TabGroupChange& change) override;

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(Browser* browser) override;

 private:
  // Methods called from OnTabGroupChanged.
  void DispatchGroupCreated(tab_groups::TabGroupId group);
  void DispatchGroupRemoved(tab_groups::TabGroupId group);
  void DispatchGroupMoved(tab_groups::TabGroupId group);
  void DispatchGroupUpdated(tab_groups::TabGroupId group);

  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     std::unique_ptr<base::ListValue> args);

  Profile* const profile_;
  EventRouter* const event_router_ = nullptr;
  BrowserTabStripTracker browser_tab_strip_tracker_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_EVENT_ROUTER_H_
