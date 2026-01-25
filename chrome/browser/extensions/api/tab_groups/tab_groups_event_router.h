// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_EVENT_ROUTER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/tab_groups/tab_group_id.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class EventRouter;

// The TabGroupsEventRouter listens to group events and routes them to listeners
// inside extension process renderers.
// TabGroupsEventRouter will only route events from windows/tabs within a
// profile to extension processes in the same profile.
class TabGroupsEventRouter : public KeyedService {
 public:
  explicit TabGroupsEventRouter(content::BrowserContext* context);
  TabGroupsEventRouter(const TabGroupsEventRouter&) = delete;
  TabGroupsEventRouter& operator=(const TabGroupsEventRouter&) = delete;
  ~TabGroupsEventRouter() override;

 private:
  // Helps observe events about tab groups in a cross-platform way.
  class PlatformDelegate;

  // Methods called from OnTabGroupChanged.
  void DispatchGroupCreated(tab_groups::TabGroupId group);
  void DispatchGroupRemoved(tab_groups::TabGroupId group);
  void DispatchGroupMoved(tab_groups::TabGroupId group);
  void DispatchGroupUpdated(tab_groups::TabGroupId group);

  void DispatchEvent(events::HistogramValue histogram_value,
                     const std::string& event_name,
                     base::ListValue args);

  const raw_ptr<Profile> profile_;
  const raw_ptr<EventRouter> event_router_;
  std::unique_ptr<PlatformDelegate> platform_delegate_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_EVENT_ROUTER_H_
