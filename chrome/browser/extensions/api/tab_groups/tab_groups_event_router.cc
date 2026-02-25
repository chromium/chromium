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

#if BUILDFLAG(IS_ANDROID)
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#else
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#endif  // BUILDFLAG(IS_ANDROID)

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

#if BUILDFLAG(IS_ANDROID)

class TabGroupsEventRouter::PlatformDelegate : public TabModelListObserver,
                                               public TabModelObserver {
 public:
  PlatformDelegate(TabGroupsEventRouter* owner, Profile* profile)
      : owner_(owner), profile_(profile) {}

  PlatformDelegate(const PlatformDelegate&) = delete;
  PlatformDelegate& operator=(const PlatformDelegate&) = delete;

  ~PlatformDelegate() override {
    tab_model_observations_.RemoveAllObservations();
    TabModelList::RemoveObserver(this);
  }

  void Init() {
    // Equivalent to observing for new windows.
    TabModelList::AddObserver(this);
    // Add models for existing windows.
    for (TabModel* model : TabModelList::models()) {
      OnTabModelAdded(model);
    }
  }

  // TabModelListObserver:
  void OnTabModelAdded(TabModel* model) override {
    // We only want to observe tab models (i.e. windows) associated with this
    // PlatformDelegate's profile. We should also ignore tab models that are
    // non-standard as they have tabs that will never have WebContents. For
    // example, imagine a regular window with a regular profile. It has one
    // TabModelEventRouter associated with the profile and one PlatformDelegate.
    // If we then create an incognito profile and window, we get a second
    // TabModelEventRouter and PlatformDelegate. But we only want to observe the
    // TabModel associated with the incognito profile, not the regular profile,
    // otherwise we'll see event notifications twice (once per observer). See
    // TestTabGroupEventsAcrossProfiles.
    if (profile_ != model->GetProfile() ||
        model->GetTabModelType() != TabModel::TabModelType::kStandard) {
      return;
    }
    tab_model_observations_.AddObservation(model);

    // TODO(crbug.com/405219902): Should we fire a "created" event for existing
    // tab groups? This object is created early in startup, so tab groups may
    // not exist yet. Need to check Win/Mac/Linux behavior.
  }

  void OnTabModelRemoved(TabModel* model) override {
    if (tab_model_observations_.IsObservingSource(model)) {
      tab_model_observations_.RemoveObservation(model);
    }
  }

  // TabModelObserver:
  void OnTabGroupCreated(tab_groups::TabGroupId group_id) override {
    owner_->DispatchGroupCreated(group_id);

    // TODO(crbug.com/405219902): For compatibility with Win/Mac/Linux we also
    // fire "updated" when a group is created. Check if this is necessary as
    // more observer methods are added, specifically the updated method.
    owner_->DispatchGroupUpdated(group_id);
  }

  void OnTabGroupRemoving(tab_groups::TabGroupId group_id) override {
    // We must dispatch this message before the group is removed (i.e. in
    // remov*ing*) because the first thing DispatchGroupRemoved() does is look
    // up the group to build group data for the event. This is also compatible
    // with Win/Mac/Linux.
    owner_->DispatchGroupRemoved(group_id);
  }

  void OnTabGroupMoved(tab_groups::TabGroupId group_id,
                       int old_index) override {
    owner_->DispatchGroupMoved(group_id);
  }

  void OnTabGroupVisualsChanged(tab_groups::TabGroupId group_id) override {
    owner_->DispatchGroupUpdated(group_id);
  }

 private:
  const raw_ptr<TabGroupsEventRouter> owner_;
  const raw_ptr<Profile> profile_;
  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};
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
  }
  PlatformDelegate(const PlatformDelegate&) = delete;
  PlatformDelegate& operator=(const PlatformDelegate&) = delete;
  ~PlatformDelegate() override = default;

  void Init() { browser_tab_strip_tracker_.Init(); }

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
      platform_delegate_(std::make_unique<PlatformDelegate>(this, profile_)) {
  platform_delegate_->Init();
}

TabGroupsEventRouter::~TabGroupsEventRouter() = default;

void TabGroupsEventRouter::DispatchGroupCreated(tab_groups::TabGroupId group) {
  std::optional<api::tab_groups::TabGroup> tab_group =
      ExtensionTabUtil::CreateTabGroupObject(group);
  if (!tab_group) {
    // May be nullopt in tests.
    return;
  }
  auto args(api::tab_groups::OnCreated::Create(*tab_group));
  DispatchEvent(events::TAB_GROUPS_ON_CREATED,
                api::tab_groups::OnCreated::kEventName, std::move(args));
}

void TabGroupsEventRouter::DispatchGroupRemoved(tab_groups::TabGroupId group) {
  std::optional<api::tab_groups::TabGroup> tab_group =
      ExtensionTabUtil::CreateTabGroupObject(group);
  if (!tab_group) {
    // May be nullopt in tests.
    return;
  }
  auto args(api::tab_groups::OnRemoved::Create(*tab_group));
  DispatchEvent(events::TAB_GROUPS_ON_REMOVED,
                api::tab_groups::OnRemoved::kEventName, std::move(args));
}

void TabGroupsEventRouter::DispatchGroupMoved(tab_groups::TabGroupId group) {
  std::optional<api::tab_groups::TabGroup> tab_group =
      ExtensionTabUtil::CreateTabGroupObject(group);
  if (!tab_group) {
    // May be nullopt in tests.
    return;
  }
  auto args(api::tab_groups::OnMoved::Create(*tab_group));
  DispatchEvent(events::TAB_GROUPS_ON_MOVED,
                api::tab_groups::OnMoved::kEventName, std::move(args));
}

void TabGroupsEventRouter::DispatchGroupUpdated(tab_groups::TabGroupId group) {
  std::optional<api::tab_groups::TabGroup> tab_group =
      ExtensionTabUtil::CreateTabGroupObject(group);
  if (!tab_group) {
    // May be nullopt in tests.
    return;
  }
  auto args(api::tab_groups::OnUpdated::Create(*tab_group));
  DispatchEvent(events::TAB_GROUPS_ON_UPDATED,
                api::tab_groups::OnUpdated::kEventName, std::move(args));
}

void TabGroupsEventRouter::DispatchEvent(events::HistogramValue histogram_value,
                                         const std::string& event_name,
                                         base::ListValue args) {
  // |event_router_| can be null in tests.
  if (!event_router_) {
    return;
  }

  auto event = std::make_unique<Event>(histogram_value, event_name,
                                       std::move(args), profile_);
  event_router_->BroadcastEvent(std::move(event));
}

}  // namespace extensions
