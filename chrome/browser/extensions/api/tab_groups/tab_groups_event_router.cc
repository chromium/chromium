// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tab_groups/tab_groups_event_router.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "extensions/browser/event_router.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif  // !BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

#if BUILDFLAG(IS_ANDROID)
// TODO(crbug.com/405219902): Implement PlatformDelegate for Android.
class TabGroupsEventRouter::PlatformDelegate {
 public:
  PlatformDelegate(TabGroupsEventRouter* owner, Profile* profile) {}
  PlatformDelegate(const PlatformDelegate&) = delete;
  PlatformDelegate& operator=(const PlatformDelegate&) = delete;
  ~PlatformDelegate() = default;
};

#else

class TabGroupsEventRouter::PlatformDelegate
    : public TabStripModelObserver,
      public BrowserTabStripTrackerDelegate {
 public:
  PlatformDelegate(TabGroupsEventRouter* owner, Profile* profile)
      : owner_(owner),
        profile_(profile),
        browser_tab_strip_tracker_(/*tab_strip_model_observer=*/this,
                                   /*delegate=*/this) {
    CHECK(profile_);
    browser_tab_strip_tracker_.Init();
  }
  PlatformDelegate(const PlatformDelegate&) = delete;
  PlatformDelegate& operator=(const PlatformDelegate&) = delete;
  ~PlatformDelegate() override = default;

  // TabStripModelObserver:
  void OnTabGroupChanged(const TabGroupChange& change) override {
    switch (change.type) {
      case TabGroupChange::kCreated: {
        owner_->DispatchGroupCreated(change.group);
        // Synthesize the initial kVisualsChanged notification while detaching
        // and reattaching groups.
        // TODO(crbug.com/398256328): Remove after fixing initial
        // kVisualsChanged case.
        if (change.GetCreateChange()->reason() ==
            TabGroupChange::TabGroupCreationReason::
                kInsertedFromAnotherTabstrip) {
          owner_->DispatchGroupUpdated(change.group);
        }
        break;
      }
      case TabGroupChange::kClosed: {
        owner_->DispatchGroupRemoved(change.group);
        break;
      }
      case TabGroupChange::kMoved: {
        owner_->DispatchGroupMoved(change.group);
        break;
      }
      case TabGroupChange::kVisualsChanged: {
        owner_->DispatchGroupUpdated(change.group);
        break;
      }
      case TabGroupChange::kEditorOpened:
        break;
    }

    return;
  }

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(BrowserWindowInterface* browser) override {
    return profile_ == browser->GetProfile() &&
           ExtensionTabUtil::BrowserSupportsTabs(browser);
  }

 private:
  const raw_ptr<TabGroupsEventRouter> owner_;
  const raw_ptr<Profile> profile_;
  BrowserTabStripTracker browser_tab_strip_tracker_;
};

#endif  // BUILDFLAG(IS_ANDROID)

TabGroupsEventRouter::TabGroupsEventRouter(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)),
      event_router_(EventRouter::Get(context)),
      platform_delegate_(std::make_unique<PlatformDelegate>(this, profile_)) {}

TabGroupsEventRouter::~TabGroupsEventRouter() = default;

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
