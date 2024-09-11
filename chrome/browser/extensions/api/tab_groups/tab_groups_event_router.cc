// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

namespace extensions {

TabGroupsEventRouter::TabGroupsEventRouter(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      event_router_(EventRouter::Get(context)),
      browser_tab_strip_tracker_(this, this) {
  browser_tab_strip_tracker_.Init();
}

void TabGroupsEventRouter::OnTabGroupChanged(const TabGroupChange& change) {
  switch (change.type) {
    case TabGroupChange::kCreated: {
      DispatchGroupCreated(change.group);
      break;
    }
    case TabGroupChange::kClosed: {
      DispatchGroupRemoved(change.group);
      break;
    }
    case TabGroupChange::kMoved: {
      DispatchGroupMoved(change.group);
      break;
    }
    case TabGroupChange::kVisualsChanged: {
      DispatchGroupUpdated(change.group);
      break;
    }
    case TabGroupChange::kContentsChanged:
    case TabGroupChange::kEditorOpened:
      break;
  }

  return;
}

bool TabGroupsEventRouter::ShouldTrackBrowser(Browser* browser) {
  return profile_ == browser->profile() &&
         ExtensionTabUtil::BrowserSupportsTabs(browser);
}

void TabGroupsEventRouter::DispatchGroupCreated(tab_groups::TabGroupId group) {
  auto args(api::tab_groups::OnCreated::Create(
      *ExtensionTabUtil::CreateTabGroupObject(group)));

  DispatchEvent(events::TAB_GROUPS_ON_CREATED,
                api::tab_groups::OnCreated::kEventName, std::move(args));
}

void TabGroupsEventRouter::DispatchGroupRemoved(tab_groups::TabGroupId group) {
  auto args(api::tab_groups::OnRemoved::Create(
      *ExtensionTabUtil::CreateTabGroupObject(group)));

  DispatchEvent(events::TAB_GROUPS_ON_REMOVED,
                api::tab_groups::OnRemoved::kEventName, std::move(args));
}

void TabGroupsEventRouter::DispatchGroupMoved(tab_groups::TabGroupId group) {
  auto args(api::tab_groups::OnMoved::Create(
      *ExtensionTabUtil::CreateTabGroupObject(group)));

  DispatchEvent(events::TAB_GROUPS_ON_MOVED,
                api::tab_groups::OnMoved::kEventName, std::move(args));
}

void TabGroupsEventRouter::DispatchGroupUpdated(tab_groups::TabGroupId group) {
  auto args(api::tab_groups::OnUpdated::Create(
      *ExtensionTabUtil::CreateTabGroupObject(group)));

  DispatchEvent(events::TAB_GROUPS_ON_UPDATED,
                api::tab_groups::OnUpdated::kEventName, std::move(args));
}

void TabGroupsEventRouter::DispatchEvent(events::HistogramValue histogram_value,
                                         const std::string& event_name,
                                         base::Value::List args) {
  // |event_router_| can be null in tests.
  if (!event_router_)
    return;

  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(args), profile_);
  event_router_->BroadcastEvent(std::move(event));
}

}  // namespace extensions
